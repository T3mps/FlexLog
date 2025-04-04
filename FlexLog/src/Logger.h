#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Common.h"
#include "Core/RCUList.h"
#include "Format/Format.h"
#include "Level.h"
#include "LoggingService.h"
#include "Message.h"
#include "Sink/Sink.h"

namespace FlexLog
{
    class LoggerThreadPool;

    class Logger : public LoggingService
    {
    public:
        Logger(std::string name, Level level = Level::Trace);
        ~Logger() override = default;

        FLOG_FORCE_INLINE bool Log(std::string_view msg, Level level, const std::source_location& location = std::source_location::current()) override;
        FLOG_FORCE_INLINE bool Log(std::string_view msg, const StructuredData& data, Level level, const std::source_location& location = std::source_location::current()) override;

        void Flush();

        void RegisterSink(std::shared_ptr<Sink> sink);
        void RegisterSinks(std::span<std::shared_ptr<Sink>> sinks);

        template<typename SinkType, typename... Args>
        void EmplaceSink(Args&&... args) { RegisterSink(std::make_shared<SinkType>(std::forward<Args>(args)...)); }

        [[nodiscard]] const std::string& GetName() const { return m_name; }
        void SetName(std::string_view name) { m_name = name; }

        [[nodiscard]] Level GetLevel() const { return m_level.load(std::memory_order_relaxed); }
        void SetLevel(Level level) { m_level.store(level, std::memory_order_relaxed); }
        [[nodiscard]] bool IsLevelEnabled(Level level) const noexcept { return level >= m_level.load(std::memory_order_acquire) && level < Level::Off; }

        Format& GetFormat() { return m_format; }
        const Format& GetFormat() const { return m_format; }
        void SetFormat(const Format& format) { m_format = format; }

        std::vector<std::shared_ptr<Sink>> GetSinks();

        uint64_t GetDroppedMessageCount() const { return m_droppedMessages.load(std::memory_order_relaxed); }
        void ResetDroppedMessageCount() { m_droppedMessages.store(0, std::memory_order_relaxed); }

    private:
        using SinkList = RCUList<std::shared_ptr<Sink>>;

        Message* CreateMessage(std::string_view message, Level level, std::source_location location);
        Message* CreateStructuredMessage(std::string_view message, const StructuredData& data, Level level, std::source_location location);

        void EnqueueMessage(Message* message);
        void ProcessMessage(Message* logMessage);

        std::string m_name;
        std::atomic<Level> m_level;
        Format m_format;
        SinkList m_sinkList;
        std::atomic<uint64_t> m_droppedMessages{0};
        std::atomic<uint64_t> m_totalProcessed{0};

        friend class LoggerThreadPool;
    };
}
