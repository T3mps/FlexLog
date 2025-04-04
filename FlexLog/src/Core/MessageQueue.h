#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "Common.h"
#include "Message.h"

namespace FlexLog
{
    /**
    * @brief A lock-free, bounded, multiple-producer/multiple-consumer message queue.
    * This implementation provides a fixed-size circular buffer.
    */
    class MessageQueue
    {
    public:
        explicit MessageQueue(size_t capacity = DEFAULT_CAPACITY);
        ~MessageQueue();

        MessageQueue(const MessageQueue&) = delete;
        MessageQueue& operator=(const MessageQueue&) = delete;
        MessageQueue(MessageQueue&&) = delete;
        MessageQueue& operator=(MessageQueue&&) = delete;

        bool TryEnqueue(Message* message);
        Message* TryDequeue();
        size_t DequeueAll(std::vector<Message*>& out);

        bool IsEmpty() const;

        float UsagePercentage() const;
        size_t GetPeakUsage() const;
        void ResetPeakUsage();

        size_t Size() const;
        constexpr size_t Capacity() const { return m_capacity; }

    private:
        static constexpr size_t DEFAULT_CAPACITY = 1024;

        static constexpr size_t RoundUpPow2(size_t v)
        {
            --v;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            if constexpr (sizeof(size_t) == 8)
                v |= v >> 32;
            return v + 1;
        }

        size_t IndexMask() const { return m_capacity - 1; }

        struct alignas(64) Slot
        {
            std::atomic<size_t> sequence;
            Message* message;

            Slot() : sequence(0), message(nullptr) {}
        };

        std::unique_ptr<Slot[]> m_slots;
        size_t m_capacity;

        alignas(64) std::atomic<size_t> m_producerIndex{0};
        alignas(64) std::atomic<size_t> m_consumerIndex{0};
        alignas(64) std::atomic<size_t> m_peakUsage{0};
    };
}
