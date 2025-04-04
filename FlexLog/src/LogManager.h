#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Core/AtomicString.h"
#include "Core/HazardPointer.h"
#include "Core/LoggerThreadPool.h"
#include "Core/MessagePool.h"
#include "Core/Result.h"
#include "Logger.h"
#include "Sink/Sink.h"

namespace FlexLog
{
    class LogManager
    {
    public:
        static LogManager& GetInstance();

        Result<void> Initialize();
        Result<void> Shutdown(bool waitForCompletion = true, std::chrono::milliseconds timeout = std::chrono::seconds(5));

        Logger& RegisterLogger(std::string_view name);
        Logger& GetLogger(std::string_view name);
        Logger& GetDefaultLogger(); 
        bool HasLogger(std::string_view name) const;
        void RemoveLogger(std::string_view name);

        void RegisterSink(std::shared_ptr<Sink> sink);

        template<typename SinkType, typename... Args>
        void RegisterGlobalSink(Args&&... args)
        {
            RegisterSink(std::make_shared<SinkType>(std::forward<Args>(args)...));
        }

        void SetDefaultLevel(Level level);
        Level GetDefaultLevel() const;
        void SetDefaultFormat(LogFormat formatInfo);

        void SetThreadPoolSize(size_t size);
        size_t GetThreadPoolSize() const;
        bool ResizeThreadPool(size_t newSize);
        LoggerThreadPool& GetThreadPool();
        MessagePool& GetMessagePool();

        void ShutdownAll();
        void ResetAll();

        void SetDefaultLoggerName(std::string_view name);
        std::string GetDefaultLoggerName() const;

    private:
        enum class LogManagerState
        {
            Uninitialized,
            Initializing, 
            Running,
            ShuttingDown,
            ShutDown
        };

        LogManager();
        ~LogManager();

        LogManager(const LogManager&) = delete;
        LogManager& operator=(const LogManager&) = delete;
        LogManager(LogManager&&) = delete;
        LogManager& operator=(LogManager&&) = delete;

        struct LoggerEntry;
        class LoggerMap;

        bool CheckAndTransitionState(LogManagerState expectedState, LogManagerState newState);
        const char* GetStateName(LogManagerState state) const;

        void CreateDefaultLogger();
        void EnsureThreadPoolInitialized();
        void EnsureMessagePoolInitialized();

        std::atomic<LogManagerState> m_state{LogManagerState::Uninitialized};

        std::unique_ptr<LoggerMap> m_loggerMap;
        RCUList<std::shared_ptr<Sink>> m_globalSinks;

        std::atomic<Level> m_defaultLevel{Level::Info};
        std::atomic<LogFormat> m_defaultFormat{LogFormat::Pattern};
        std::unique_ptr<AtomicString> m_defaultLoggerName;

        std::atomic<LoggerThreadPool*> m_threadPool{nullptr};
        std::unique_ptr<MessagePool> m_messagePool;
        std::unique_ptr<HazardPointerDomain> m_hazardDomain;

        std::atomic<uint64_t> m_configVersion{0};
    };
}