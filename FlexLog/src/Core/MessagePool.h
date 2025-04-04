#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "Common.h"
#include "Message.h"

namespace FlexLog
{
    class MessagePool
    {
    public:
        MessagePool();
        ~MessagePool() = default;

        MessageRef AcquireRef() { return MessageRef(Acquire()); }
        Message* Acquire();

        void Release(Message* message);
        void Release(MessageRef& messageRef);
        void FinalizeRelease(Message* message);

        size_t GetSize() const { return m_size.load(std::memory_order_relaxed); }
        size_t GetCapacity() const { return m_capacity.load(std::memory_order_relaxed); }
        size_t GetPeakUsage() const { return m_peakUsage.load(std::memory_order_relaxed); }
        float GetUsagePercentage() const
        {
            size_t capacity = m_capacity.load(std::memory_order_relaxed);
            return capacity > 0 ? static_cast<float>(m_size.load(std::memory_order_relaxed)) / capacity * 100.0f : 0.0f;
        }

        void TryShrink(float threshold = 0.3334f);

    private:
        void ResetMessage(Message* message);
        void InitializeChunk(size_t chunkSize);

        Message* AcquireFromThreadLocalCache();
        bool TryReleaseToThreadLocalCache(Message* message);

        struct alignas(64) Chunk
        {
            std::unique_ptr<Message[]> objects;
            std::unique_ptr<std::atomic<bool>[]> used;
            size_t size;
        };

        static thread_local struct alignas(64) ThreadLocalCache
        {
            static constexpr size_t CACHE_SIZE = 64;
            std::array<Message, CACHE_SIZE> messages;
            std::array<std::atomic<bool>, CACHE_SIZE> used;
            std::atomic<size_t> usedCount{0};

            ThreadLocalCache();
        } s_localCache;

        std::vector<Chunk> m_chunks;
        std::mutex m_chunkMutex;

        alignas(64) std::atomic<size_t> m_size{0};
        alignas(64) std::atomic<size_t> m_capacity{0};
        alignas(64) std::atomic<size_t> m_peakUsage{0};
        alignas(64) std::atomic<size_t> m_nextChunkIndex{0};

        static constexpr size_t INITIAL_CAPACITY = 1024;
        static constexpr size_t GROWTH_FACTOR = 2;
    };
}
