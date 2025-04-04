#include "LoggerThreadPool.h"

#include <algorithm>
#include <iostream>

#include "Logger.h"
#include "LogManager.h"
#include "MessagePool.h"

FlexLog::LoggerThreadPool::LoggerThreadPool(size_t threadCount) : m_running(true)
{
    // Ensure at least one thread in the pool
    threadCount = std::max<size_t>(1, threadCount);

    m_queues.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i)
    {
        m_queues.push_back(std::make_unique<QueueData>());
    }
    for (size_t i = 0; i < threadCount; ++i)
    {
        m_workers.emplace_back(&LoggerThreadPool::WorkerFunction, this, i);
    }
}

FlexLog::LoggerThreadPool::~LoggerThreadPool()
{
    Shutdown();
}

void FlexLog::LoggerThreadPool::Shutdown(bool flushBeforeShutdown, std::chrono::milliseconds timeout)
{
    // Set running flag to false using memory_order_release to ensure visibility
    bool wasRunning = m_running.exchange(false, std::memory_order_release);
    if (!wasRunning)
        return; // Already shut down

    if (flushBeforeShutdown)
    {
        // Set flushing state with memory_order_release to ensure visibility
        m_flushing.store(true, std::memory_order_release);
        Flush(timeout);
        m_flushing.store(false, std::memory_order_release);
    }

    // Wake up all worker threads so they can exit
    for (auto& queueData : m_queues)
    {
        std::lock_guard<std::mutex> lock(queueData->mutex);
        queueData->cv.notify_all(); // Notify all threads, not just one
    }

    // Join all worker threads with a timeout
    auto joinEndTime = std::chrono::steady_clock::now() + timeout;

    for (auto& worker : m_workers)
    {
        if (worker.joinable())
        {
            // Calculate remaining timeout
            auto remainingTime = joinEndTime - std::chrono::steady_clock::now();
            if (remainingTime <= std::chrono::milliseconds(0))
            {
                // We've exceeded our timeout - log a warning and detach thread
                std::cerr << "Warning: Thread join timeout in ThreadPool shutdown" << std::endl;
                worker.detach();
                continue;
            }

            // Join with the remaining timeout
            worker.join();
        }
    }
}

void FlexLog::LoggerThreadPool::Flush(std::chrono::milliseconds timeout)
{
    // Use an absolute timeout point
    auto endTime = std::chrono::steady_clock::now() + timeout;

    // First, create snapshot of pending message counts
    size_t totalPending = 0;

    for (auto& queueData : m_queues)
    {
        std::lock_guard<std::mutex> lock(queueData->mutex);
        totalPending += queueData->pendingMessages;

        // Wake up workers to process messages
        queueData->cv.notify_one();
    }

    // If nothing pending, return immediately
    if (totalPending == 0)
        return;

    // Wait until all queues are empty or timeout
    while (std::chrono::steady_clock::now() < endTime)
    {
        // Sleep a short time to avoid CPU spinning
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Check if all queues are empty
        size_t currentPending = 0;
        for (auto& queueData : m_queues)
        {
            std::lock_guard<std::mutex> lock(queueData->mutex);
            currentPending += queueData->pendingMessages;
        }

        // Exit when all messages are processed
        if (currentPending == 0)
            return;
    }

    // If we get here, we've timed out
    // Log a warning (using std::cerr as a fallback since we can't use the logger itself)
    std::cerr << "Warning: ThreadPool flush timed out with " << GetPendingMessageCount() << " messages remaining" << std::endl;
}

void FlexLog::LoggerThreadPool::EnqueueMessage(Message* message, uint8_t priority)
{
    // Early check if the thread pool is running or flushing
    // memory_order_acquire ensures we see the most recent state
    if (!m_running.load(std::memory_order_acquire) || 
        m_flushing.load(std::memory_order_acquire) ||
        !message || 
        !message->IsActive())
    {
        if (message)
            LogManager::GetInstance().GetMessagePool().Release(message);
        return;
    }

    // Add a reference to the message before placing it in the queue
    message->AddRef();

    // Select a queue using round-robin approach
    size_t queueIndex = SelectQueue(message);
    auto& queueData = m_queues[queueIndex];

    // Lock the queue to ensure thread safety
    {
        std::lock_guard<std::mutex> lock(queueData->mutex);
        queueData->messageQueue.push({message, priority});
        ++queueData->pendingMessages;

        // Use notify_one to wake up a single worker thread
        queueData->cv.notify_one();
    }
}

size_t FlexLog::LoggerThreadPool::GetPendingMessageCount() const
{
    size_t count = 0;
    for (auto& queueData : m_queues)
    {
        std::lock_guard<std::mutex> lock(queueData->mutex);
        count += queueData->pendingMessages;
    }
    return count;
}

size_t FlexLog::LoggerThreadPool::SelectQueue(Message* message)
{
    if (m_queues.size() == 1)
        return 0;

    // Use round-robin distribution with memory_order_relaxed
    // since precise ordering is not critical for queue selection
    return m_nextQueueIndex.fetch_add(1, std::memory_order_relaxed) % m_queues.size();

    // Alternative strategies could include:
    // 1. Select least busy queue
    // 2. Use hash of logger name for affinity
    // 3. Use priority to determine queue
    // TODO: Decide after testing...
}

void FlexLog::LoggerThreadPool::WorkerFunction(size_t queueIndex)
{
    auto& queueData = m_queues[queueIndex];

    for (;;)
    {
        QueueItem item;
        bool hasItem = false;

        {
            std::unique_lock<std::mutex> lock(queueData->mutex);

            // Wait until there's work to do or we should exit
            queueData->cv.wait(lock, [this, &queueData]()
                {
                    // Use memory_order_acquire to ensure proper visibility
                    bool running = m_running.load(std::memory_order_acquire);
                    bool flushing = m_flushing.load(std::memory_order_acquire);

                    // Wake up if:
                    // 1. There are messages to process OR
                    // 2. We're shutting down and not flushing
                    return !queueData->messageQueue.empty() || (!running && !flushing);
                });

            // Exit if we're shutting down and there's no more work
            if (queueData->messageQueue.empty() && 
                !m_running.load(std::memory_order_acquire) && 
                !m_flushing.load(std::memory_order_acquire))
            {
                break;
            }

            // Process a message if available
            if (!queueData->messageQueue.empty())
            {
                item = queueData->messageQueue.top();
                queueData->messageQueue.pop();
                --queueData->pendingMessages;
                hasItem = true;
            }
        }

        // Process message outside of lock
        if (hasItem && item.message && item.message->IsActive())
        {
            if (item.message->logger)
                item.message->logger->ProcessMessage(item.message);

            // Release the message reference after processing
            if (item.message->ReleaseRef() && item.message->state.load(std::memory_order_acquire) == MessageState::Releasing)
                LogManager::GetInstance().GetMessagePool().FinalizeRelease(item.message);
        }
    }

    // Clean up remaining messages before exiting
    // This block only handles true cleanup, not processing
    std::unique_lock<std::mutex> lock(queueData->mutex);
    while (!queueData->messageQueue.empty())
    {
        QueueItem item = queueData->messageQueue.top();
        queueData->messageQueue.pop();
        --queueData->pendingMessages;

        if (!item.message)
            continue;

        // During final cleanup, we don't process messages, just release resources
        if (item.message->ReleaseRef() && item.message->state.load(std::memory_order_acquire) == MessageState::Releasing)
            LogManager::GetInstance().GetMessagePool().FinalizeRelease(item.message);
    }
}

bool FlexLog::LoggerThreadPool::Resize(size_t newThreadCount)
{
    // Ensure at least one thread
    newThreadCount = std::max<size_t>(1, newThreadCount);

    std::lock_guard<std::mutex> lock(m_resizeMutex);

    // Check if the thread pool is still running
    if (!m_running.load(std::memory_order_acquire))
        return false;

    const size_t currentThreadCount = m_workers.size();

    if (newThreadCount == currentThreadCount)
        return true;

    if (newThreadCount < currentThreadCount)
    {
        // Shrinking: remove excess threads and queues
        size_t threadsToRemove = currentThreadCount - newThreadCount;

        std::vector<std::thread> terminators;
        terminators.reserve(threadsToRemove);

        for (size_t i = 0; i < threadsToRemove; ++i)
        {
            terminators.emplace_back([this, indexToRemove = newThreadCount + i]()
                {
                    // Notify the worker to check its exit condition
                    m_queues[indexToRemove]->cv.notify_one();

                    if (indexToRemove < m_workers.size() && m_workers[indexToRemove].joinable())
                        m_workers[indexToRemove].join();
                });
        }

        for (auto& terminator : terminators)
        {
            if (terminator.joinable())
                terminator.join();
        }

        m_workers.resize(newThreadCount);
        // Note: we keep all queues to avoid reallocation
    }
    else
    {
        // Growing: add new threads and queues
        size_t threadsToAdd = newThreadCount - currentThreadCount;
        size_t currentSize = m_queues.size();

        // Ensure we have enough queues
        while (m_queues.size() < newThreadCount)
        {
            m_queues.push_back(std::make_unique<QueueData>());
        }

        // Add new worker threads
        for (size_t i = 0; i < threadsToAdd; ++i)
        {
            m_workers.emplace_back(&LoggerThreadPool::WorkerFunction, this, currentSize + i);
        }
    }

    return true;
}