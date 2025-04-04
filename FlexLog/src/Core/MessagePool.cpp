#include "MessagePool.h"

#include <algorithm>

thread_local FlexLog::MessagePool::ThreadLocalCache FlexLog::MessagePool::s_localCache;

FlexLog::MessagePool::MessagePool()
{
    InitializeChunk(INITIAL_CAPACITY);
    m_capacity.store(INITIAL_CAPACITY, std::memory_order_relaxed);
}

FlexLog::Message* FlexLog::MessagePool::Acquire()
{
    // FAST PATH: Try thread-local cache first (completely lock-free)
    Message* message = AcquireFromThreadLocalCache();
    if (message)
    {
        // Use memory_order_release to ensure all initialization is visible to other threads
        message->state.store(MessageState::Active, std::memory_order_release);
        message->refCount.store(1, std::memory_order_release);
        return message;
    }

    // MEDIUM PATH: Try lock-free acquisition from shared pool
    // Use a consistent round-robin approach to reduce contention
    size_t numChunks = m_chunks.size();

    // Get a unique chunk index with memory_order_relaxed since we're just getting a starting point
    // We don't need ordering guarantees for the index selection
    size_t startChunkIndex = m_nextChunkIndex.fetch_add(1, std::memory_order_relaxed) % numChunks;

    // Try each chunk, starting at our assigned index
    for (size_t i = 0; i < numChunks; ++i)
    {
        size_t chunkIndex = (startChunkIndex + i) % numChunks;
        auto& chunk = m_chunks[chunkIndex];

        const size_t scanLimit = std::min(chunk.size, static_cast<size_t>(16)); // Limit scanning

        // Scan a portion of the chunk for an available slot
        for (size_t j = 0; j < scanLimit; ++j)
        {
            // Try to atomically claim this slot
            bool expected = false;
            // memory_order_acquire ensures we see previous releases of this slot
            if (chunk.used[j].compare_exchange_strong(expected, true, std::memory_order_acquire))
            {
                // Use memory_order_relaxed for counter increment since exact ordering isn't critical
                m_size.fetch_add(1, std::memory_order_relaxed);

                // Update peak usage - relaxed is sufficient for statistics
                size_t currentSize = m_size.load(std::memory_order_relaxed);
                size_t peak = m_peakUsage.load(std::memory_order_relaxed);
                if (currentSize > peak)
                    m_peakUsage.store(currentSize, std::memory_order_relaxed);

                Message* msg = &chunk.objects[j];
                // memory_order_release ensures all thread see consistent message state
                msg->state.store(MessageState::Active, std::memory_order_release);
                msg->refCount.store(1, std::memory_order_release);
                return msg;
            }
        }
    }

    // SLOW PATH: Need to allocate a new chunk (mutex-protected)
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    // Double-check if another thread has already expanded the pool
    if (message = AcquireFromThreadLocalCache())
    {
        message->state.store(MessageState::Active, std::memory_order_release);
        message->refCount.store(1, std::memory_order_release);
        return message;
    }

    // Check all chunks one more time before allocating
    for (auto& chunk : m_chunks)
    {
        for (size_t i = 0; i < chunk.size; ++i)
        {
            bool expected = false;
            if (chunk.used[i].compare_exchange_strong(expected, true, std::memory_order_acquire))
            {
                m_size.fetch_add(1, std::memory_order_relaxed);

                Message* msg = &chunk.objects[i];
                msg->state.store(MessageState::Active, std::memory_order_release);
                msg->refCount.store(1, std::memory_order_release);
                return msg;
            }
        }
    }

    // Create a new chunk
    size_t newChunkSize = m_chunks.back().size * GROWTH_FACTOR;
    InitializeChunk(newChunkSize);
    m_capacity.fetch_add(newChunkSize, std::memory_order_release);

    // Use the first slot in the new chunk
    m_chunks.back().used[0].store(true, std::memory_order_release);
    m_size.fetch_add(1, std::memory_order_relaxed);

    size_t currentSize = m_size.load(std::memory_order_relaxed);
    size_t peak = m_peakUsage.load(std::memory_order_relaxed);
    if (currentSize > peak)
        m_peakUsage.store(currentSize, std::memory_order_relaxed);

    Message* msg = &m_chunks.back().objects[0];
    msg->state.store(MessageState::Active, std::memory_order_release);
    msg->refCount.store(1, std::memory_order_release);
    return msg;
}

void FlexLog::MessagePool::Release(FlexLog::MessageRef& messageRef)
{
    if (messageRef)
    {
        Release(messageRef.Get());
        messageRef.Reset();
    }
}

void FlexLog::MessagePool::Release(Message* message)
{
    if (!message)
        return;

    // Try to atomically transition from Active to Releasing
    MessageState expected = MessageState::Active;
    // memory_order_acq_rel ensures proper visibility of state changes
    if (!message->state.compare_exchange_strong(expected, MessageState::Releasing, std::memory_order_acq_rel))
    {
        // Message was already released or not active
        return;
    }

    // memory_order_acquire ensures we see the most up-to-date refCount
    uint32_t currentRefCount = message->refCount.load(std::memory_order_acquire);

    // If refCount is 1, we're the only reference holder and can release immediately
    // Use memory_order_acquire to ensure proper visibility of the reference count
    if (currentRefCount == 1)
    {
        // Set refCount to 0 with memory_order_release to ensure visibility
        message->refCount.store(0, std::memory_order_release);
        FinalizeRelease(message);
    }
    // Otherwise, the release will be completed when the last reference is gone
}

void FlexLog::MessagePool::FinalizeRelease(Message* message)
{
    if (!message)
        return;

    // Ensure the message is in Releasing state before finalizing
    MessageState state = message->state.load(std::memory_order_acquire);
    if (state != MessageState::Releasing)
        return;

    // Try to return to thread-local cache first (lock-free fast path)
    if (TryReleaseToThreadLocalCache(message))
        return;

    // Find which chunk this message belongs to
    for (size_t chunkIndex = 0; chunkIndex < m_chunks.size(); ++chunkIndex)
    {
        auto& chunk = m_chunks[chunkIndex];
        Message* chunkStart = chunk.objects.get();
        Message* chunkEnd = chunkStart + chunk.size;

        if (message >= chunkStart && message < chunkEnd)
        {
            size_t index = message - chunkStart;

            if (index < chunk.size)
            {
                // Reset the message (no lock needed)
                ResetMessage(message);

                // Mark as unused with atomic exchange
                // memory_order_release ensures the reset is visible
                if (chunk.used[index].exchange(false, std::memory_order_release))
                    m_size.fetch_sub(1, std::memory_order_relaxed);
                return;
            }
        }
    }
}

void FlexLog::MessagePool::ResetMessage(Message* message)
{
    // Perform a complete reset of the message
    message->messageStorage = StringStorage();
    message->message = std::string_view();
    message->name = std::string_view();
    message->level = Level::Info;
    message->logger = nullptr;
    message->structuredData.Clear();

    // Use memory_order_release for the state to ensure all the above resets
    // are visible to the next thread that acquires this message
    message->state.store(MessageState::Pooled, std::memory_order_release);
    message->refCount.store(0, std::memory_order_release);
}

void FlexLog::MessagePool::InitializeChunk(size_t chunkSize)
{
    Chunk chunk;
    chunk.objects = std::make_unique<Message[]>(chunkSize);
    chunk.used = std::make_unique<std::atomic<bool>[]>(chunkSize);
    chunk.size = chunkSize;

    // Initialize all slots as unused
    for (size_t i = 0; i < chunkSize; ++i)
    {
        chunk.used[i].store(false, std::memory_order_relaxed);
    }

    m_chunks.push_back(std::move(chunk));
}

FlexLog::Message* FlexLog::MessagePool::AcquireFromThreadLocalCache()
{
    // Try to find an unused message in the thread-local cache
    for (size_t i = 0; i < ThreadLocalCache::CACHE_SIZE; ++i)
    {
        bool expected = false;
        if (s_localCache.used[i].compare_exchange_strong(expected, true, std::memory_order_acquire))
        {
            s_localCache.usedCount.fetch_add(1, std::memory_order_relaxed);
            return &s_localCache.messages[i];
        }
    }
    return nullptr;
}

bool FlexLog::MessagePool::TryReleaseToThreadLocalCache(Message* message)
{
    // Check if message is in the thread-local range
    auto cache_begin = &s_localCache.messages[0];
    auto cache_end = cache_begin + ThreadLocalCache::CACHE_SIZE;

    if (message >= cache_begin && message < cache_end)
    {
        size_t index = message - cache_begin;

        // Reset the message before making it available
        ResetMessage(message);

        // Mark as unused in thread local cache
        // memory_order_release ensures the reset is visible
        s_localCache.used[index].store(false, std::memory_order_release);
        s_localCache.usedCount.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }
    return false;
}

FlexLog::MessagePool::ThreadLocalCache::ThreadLocalCache()
{
    for (auto& flag : used)
    {
        flag.store(false, std::memory_order_relaxed);
    }
}

void FlexLog::MessagePool::TryShrink(float threshold)
{
    // This is called infrequently, so using a mutex is acceptable
    std::lock_guard<std::mutex> lock(m_chunkMutex);

    float usagePercentage = GetUsagePercentage();

    // Only shrink if usage is below threshold and we have more than one chunk
    if (usagePercentage > threshold * 100.0f || m_chunks.size() <= 1)
        return;

    // Simple shrinking strategy: just remove completely unused chunks from the end
    while (m_chunks.size() > 1)
    {
        const auto& lastChunk = m_chunks.back();
        bool chunkIsEmpty = true;

        // Check if chunk is completely unused
        for (size_t i = 0; i < lastChunk.size && chunkIsEmpty; ++i)
        {
            if (lastChunk.used[i].load(std::memory_order_acquire))
            {
                chunkIsEmpty = false;
                break;
            }
        }

        if (!chunkIsEmpty)
            break;

        // Remove the chunk
        size_t removedSize = lastChunk.size;
        m_chunks.pop_back();
        m_capacity.fetch_sub(removedSize, std::memory_order_relaxed);
    }
}
