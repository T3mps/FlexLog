#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <source_location>
#include <string>
#include <string_view>

#include "Common.h"
#include "Level.h"
#include "Core/StringStorage.h"
#include "Format/Structured/StructuredData.h"

namespace FlexLog
{
    class MessagePool;
    class Logger;

    enum class MessageState : uint8_t
    {
        Pooled,     // In pool, not in use
        Active,     // In active use
        Releasing   // Marked for release but references still exist
    };

    struct Message
    {
        std::chrono::time_point<std::chrono::system_clock> timestamp;
        std::string_view name;
        Level level = Level::Info;
        std::string_view message;
        std::source_location sourceLocation;

        StringStorage messageStorage;
        Logger* logger = nullptr;

        StructuredData structuredData;

        std::atomic<uint32_t> refCount{0};
        std::atomic<MessageState> state{MessageState::Pooled};

        Message() = default;

        void AddRef() { refCount.fetch_add(1, std::memory_order_acquire); }

        // Release reference and return true if this was the last reference
        bool ReleaseRef() { return refCount.fetch_sub(1, std::memory_order_acq_rel) == 1; }

        // Check if message is still valid for use
        bool IsActive() const { return state.load(std::memory_order_acquire) == MessageState::Active; }

        friend class MessagePool;
    };

    class MessageRef
    {
    public:
        MessageRef() = default;
        explicit MessageRef(Message* message);
        MessageRef(const MessageRef& other);
        MessageRef(MessageRef&& other) noexcept;
        ~MessageRef();

        MessageRef& operator=(const MessageRef& other);
        MessageRef& operator=(MessageRef&& other) noexcept;

        explicit operator bool() const { return m_message != nullptr && m_message->IsActive(); }

        void Reset();

        Message* Get() const { return m_message; }
        Message* operator->() const { return m_message; }
        Message& operator*() const { return *m_message; }

    private:
        Message* m_message = nullptr;
    };
}
