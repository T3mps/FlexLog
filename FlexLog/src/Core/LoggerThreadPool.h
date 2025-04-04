#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "Common.h"
#include "Message.h"

namespace FlexLog
{
    class MessagePool;

    class LoggerThreadPool
    {
    public:
        LoggerThreadPool(size_t threadCount = std::thread::hardware_concurrency() / 2);
        ~LoggerThreadPool();

        void Shutdown(bool flushBeforeShutdown = true, std::chrono::milliseconds timeout = std::chrono::seconds(5));
        void Flush(std::chrono::milliseconds timeout = std::chrono::seconds(5));
        void EnqueueMessage(Message* message, uint8_t priority = 0);

        size_t GetPendingMessageCount() const;

        size_t GetThreadCount() const { return m_workers.size(); }
        bool IsRunning() const { return m_running; }

        bool Resize(size_t newThreadCount);

    private:
        size_t SelectQueue(Message* message);
        void WorkerFunction(size_t queueIndex);

        struct QueueItem
        {
            Message* message;
            uint8_t priority;

            bool operator<(const QueueItem& other) const { return priority < other.priority; }
        };

        struct QueueData
        {
            std::mutex mutex;
            std::condition_variable cv;
            std::priority_queue<QueueItem> messageQueue;
            size_t pendingMessages{0};
        };

        // Use memory_order_acquire/release for all atomic operations on these flags
        alignas(64) std::atomic<bool> m_running{true};
        alignas(64) std::atomic<bool> m_flushing{false};

        std::vector<std::thread> m_workers;
        std::vector<std::unique_ptr<QueueData>> m_queues;

        // Use a separate atomic for queue selection to avoid contention
        alignas(64) std::atomic<size_t> m_nextQueueIndex{0};

        mutable std::mutex m_resizeMutex;
    };
}
