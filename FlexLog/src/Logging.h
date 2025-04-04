#pragma once

#include <format>
#include <source_location>
#include <utility>

#include "Common.h"
#include "Core/LoggerThreadPool.h"
#include "Core/MessagePool.h"
#include "Core/MessageQueue.h"
#include "Core/Result.h"
#include "Core/StringStorage.h"
#include "Format/Format.h"
#include "Level.h"
#include "LoggingService.h"
#include "LogManager.h"
#include "Message.h"
#include "Sink/ConsoleSink.h"
#include "Sink/FileSink.h"
#include "Sink/Sink.h"
#include "Format/Structured/BaseStructuredFormatter.h"
#include "Format/Structured/CloudWatchFormatter.h"
#include "Format/Structured/ElasticsearchFormatter.h"
#include "Format/Structured/GelfFormatter.h"
#include "Format/Structured/JsonFormatter.h"
#include "Format/Structured/LogstashFormatter.h"
#include "Format/Structured/OpenTelemetryFormatter.h"
#include "Format/Structured/SplunkFormatter.h"
#include "Format/Structured/StructuredData.h"
#include "Format/Structured/StructuredFormatter.h"
#include "Format/Structured/XmlFormatter.h"

//=============================================================================
// Core Logging Macros (Internal Implementation)
//=============================================================================

#define FLOG_INTERNAL_TRACE(message, location) ::FlexLog::LogManager::GetInstance().GetDefaultLogger().Trace(message, location)
#define FLOG_INTERNAL_DEBUG(message, location) ::FlexLog::LogManager::GetInstance().GetDefaultLogger().Debug(message, location)
#define FLOG_INTERNAL_INFO(message, location)  ::FlexLog::LogManager::GetInstance().GetDefaultLogger().Info(message, location)
#define FLOG_INTERNAL_WARN(message, location)  ::FlexLog::LogManager::GetInstance().GetDefaultLogger().Warn(message, location)
#define FLOG_INTERNAL_ERROR(message, location) ::FlexLog::LogManager::GetInstance().GetDefaultLogger().Error(message, location)
#define FLOG_INTERNAL_FATAL(message, location) ::FlexLog::LogManager::GetInstance().GetDefaultLogger().Fatal(message, location)

#define FLOG_INTERNAL_TRACE_LOGGER(loggerName, message, location) ::FlexLog::LogManager::GetInstance().GetLogger(loggerName).Trace(message, location)
#define FLOG_INTERNAL_DEBUG_LOGGER(loggerName, message, location) ::FlexLog::LogManager::GetInstance().GetLogger(loggerName).Debug(message, location)
#define FLOG_INTERNAL_INFO_LOGGER(loggerName, message, location)  ::FlexLog::LogManager::GetInstance().GetLogger(loggerName).Info(message, location)
#define FLOG_INTERNAL_WARN_LOGGER(loggerName, message, location)  ::FlexLog::LogManager::GetInstance().GetLogger(loggerName).Warn(message, location)
#define FLOG_INTERNAL_ERROR_LOGGER(loggerName, message, location) ::FlexLog::LogManager::GetInstance().GetLogger(loggerName).Error(message, location)
#define FLOG_INTERNAL_FATAL_LOGGER(loggerName, message, location) ::FlexLog::LogManager::GetInstance().GetLogger(loggerName).Fatal(message, location)

//=============================================================================
// Build Configuration Macros - For conditional logging
//=============================================================================

// Define FLOG_DISABLE_LOGS to completely disable logging in release builds
// #define FLOG_DISABLE_LOGS

// Define FLOG_DISABLE_TRACE to disable trace logs in release builds
#if (defined(FLOG_DIST) || defined(FLOG_RELEASE)) && !defined(FLOG_DISABLE_TRACE)
    #define FLOG_DISABLE_TRACE
#endif

// Define FLOG_DISABLE_DEBUG to disable debug logs in release builds
#if (defined(FLOG_DIST) || defined(FLOG_RELEASE)) && !defined(FLOG_DISABLE_DEBUG)
    #define FLOG_DISABLE_DEBUG
#endif

//=============================================================================
// Conditional Compilation - Will replace disabled logs with no-ops
//=============================================================================

#if defined(FLOG_DISABLE_LOGS)
    #undef FLOG_INTERNAL_TRACE
    #undef FLOG_INTERNAL_DEBUG
    #undef FLOG_INTERNAL_INFO
    #undef FLOG_INTERNAL_WARN
    #undef FLOG_INTERNAL_ERROR
    #undef FLOG_INTERNAL_FATAL
    #undef FLOG_INTERNAL_TRACE_LOGGER
    #undef FLOG_INTERNAL_DEBUG_LOGGER
    #undef FLOG_INTERNAL_INFO_LOGGER
    #undef FLOG_INTERNAL_WARN_LOGGER
    #undef FLOG_INTERNAL_ERROR_LOGGER
    #undef FLOG_INTERNAL_FATAL_LOGGER

    #define FLOG_INTERNAL_TRACE(message, location) ((void)0)
    #define FLOG_INTERNAL_DEBUG(message, location) ((void)0)
    #define FLOG_INTERNAL_INFO(message, location)  ((void)0)
    #define FLOG_INTERNAL_WARN(message, location)  ((void)0)
    #define FLOG_INTERNAL_ERROR(message, location) ((void)0)
    #define FLOG_INTERNAL_FATAL(message, location) ((void)0)
    #define FLOG_INTERNAL_TRACE_LOGGER(logger, message, location) ((void)0)
    #define FLOG_INTERNAL_DEBUG_LOGGER(logger, message, location) ((void)0)
    #define FLOG_INTERNAL_INFO_LOGGER(logger, message, location)  ((void)0)
    #define FLOG_INTERNAL_WARN_LOGGER(logger, message, location)  ((void)0)
    #define FLOG_INTERNAL_ERROR_LOGGER(logger, message, location) ((void)0)
    #define FLOG_INTERNAL_FATAL_LOGGER(logger, message, location) ((void)0)

#elif defined(FLOG_DISABLE_TRACE)
    // Disable only trace logs
    #undef FLOG_INTERNAL_TRACE
    #undef FLOG_INTERNAL_TRACE_LOGGER
    #define FLOG_INTERNAL_TRACE(message, location) ((void)0)
    #define FLOG_INTERNAL_TRACE_LOGGER(logger, message, location) ((void)0)

#elif defined(FLOG_DISABLE_DEBUG)
    // Disable only debug logs
    #undef FLOG_INTERNAL_DEBUG
    #undef FLOG_INTERNAL_DEBUG_LOGGER
    #define FLOG_INTERNAL_DEBUG(message, location) ((void)0)
    #define FLOG_INTERNAL_DEBUG_LOGGER(logger, message, location) ((void)0)
#endif

//=============================================================================
// Public API
//=============================================================================

// Default logger macros with early level check
#define FLOG_TRACE(...) \
    do { \
        auto& logger = ::FlexLog::LogManager::GetInstance().GetDefaultLogger(); \
        if (logger.IsLevelEnabled(::FlexLog::Level::Trace)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_TRACE(std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

#define FLOG_DEBUG(...) \
    do { \
        auto& logger = ::FlexLog::LogManager::GetInstance().GetDefaultLogger(); \
        if (logger.IsLevelEnabled(::FlexLog::Level::Debug)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_DEBUG(std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

#define FLOG_INFO(...) \
    do { \
        auto& logger = ::FlexLog::LogManager::GetInstance().GetDefaultLogger(); \
        if (logger.IsLevelEnabled(::FlexLog::Level::Info)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_INFO(std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

#define FLOG_WARN(...) \
    do { \
        auto& logger = ::FlexLog::LogManager::GetInstance().GetDefaultLogger(); \
        if (logger.IsLevelEnabled(::FlexLog::Level::Warn)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_WARN(std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

#define FLOG_ERROR(...) \
    do { \
        auto& logger = ::FlexLog::LogManager::GetInstance().GetDefaultLogger(); \
        if (logger.IsLevelEnabled(::FlexLog::Level::Error)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_ERROR(std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

#define FLOG_FATAL(...) \
    do { \
        auto& logger = ::FlexLog::LogManager::GetInstance().GetDefaultLogger(); \
        if (logger.IsLevelEnabled(::FlexLog::Level::Fatal)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_FATAL(std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

// Named logger macros with early level check
#define FLOG_TRACE_LOGGER(logger, ...) \
    do { \
        auto& loggerObj = ::FlexLog::LogManager::GetInstance().GetLogger(logger); \
        if (loggerObj.IsLevelEnabled(::FlexLog::Level::Trace)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_TRACE_LOGGER(logger, std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

#define FLOG_DEBUG_LOGGER(logger, ...) \
    do { \
        auto& loggerObj = ::FlexLog::LogManager::GetInstance().GetLogger(logger); \
        if (loggerObj.IsLevelEnabled(::FlexLog::Level::Debug)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_DEBUG_LOGGER(logger, std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

#define FLOG_INFO_LOGGER(logger, ...) \
    do { \
        auto& loggerObj = ::FlexLog::LogManager::GetInstance().GetLogger(logger); \
        if (loggerObj.IsLevelEnabled(::FlexLog::Level::Info)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_INFO_LOGGER(logger, std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

#define FLOG_WARN_LOGGER(logger, ...) \
    do { \
        auto& loggerObj = ::FlexLog::LogManager::GetInstance().GetLogger(logger); \
        if (loggerObj.IsLevelEnabled(::FlexLog::Level::Warn)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_WARN_LOGGER(logger, std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

#define FLOG_ERROR_LOGGER(logger, ...) \
    do { \
        auto& loggerObj = ::FlexLog::LogManager::GetInstance().GetLogger(logger); \
        if (loggerObj.IsLevelEnabled(::FlexLog::Level::Error)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_ERROR_LOGGER(logger, std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

#define FLOG_FATAL_LOGGER(logger, ...) \
    do { \
        auto& loggerObj = ::FlexLog::LogManager::GetInstance().GetLogger(logger); \
        if (loggerObj.IsLevelEnabled(::FlexLog::Level::Fatal)) { \
            auto loc = std::source_location::current(); \
            FLOG_INTERNAL_FATAL_LOGGER(logger, std::format(__VA_ARGS__), loc); \
        } \
    } while(0)

//=============================================================================
// Template-based API for type-safe logging (Alternative to macros)
//=============================================================================

namespace FlexLog::Log
{
    FLOG_FORCE_INLINE Logger& RegisterLogger(std::string_view name)
    {
        auto& logger = LogManager::GetInstance().RegisterLogger(name);
        return logger;
    }

    FLOG_FORCE_INLINE Logger& GetLogger(std::string_view name)
    {
        auto& logger = LogManager::GetInstance().RegisterLogger(name);
        return logger;
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Trace(std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Trace))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Trace(message, location);
        }
    }

    FLOG_FORCE_INLINE void Trace(std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Trace))
            logger.Trace(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Debug(std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Debug))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Debug(message, location);
        }
    }

    FLOG_FORCE_INLINE void Debug(std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Debug))
            logger.Debug(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Info(std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Info))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Info(message, location);
        }
    }

    FLOG_FORCE_INLINE void Info(std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Info))
            logger.Info(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Warn(std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Warn))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Warn(message, location);
        }
    }

    FLOG_FORCE_INLINE void Warn(std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Warn))
            logger.Warn(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Error(std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Error))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Error(message, location);
        }
    }

    FLOG_FORCE_INLINE void Error(std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Error))
            logger.Error(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Fatal(std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Fatal))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Fatal(message, location);
        }
    }

    FLOG_FORCE_INLINE void Fatal(std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetDefaultLogger();
        if (logger.IsLevelEnabled(Level::Fatal))
            logger.Fatal(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Trace(std::string_view loggerName, std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Trace))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Trace(message, location);
        }
    }

    FLOG_FORCE_INLINE void Trace(std::string_view loggerName, std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Trace))
            logger.Trace(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Debug(std::string_view loggerName, std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Debug))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Debug(message, location);
        }
    }

    FLOG_FORCE_INLINE void Debug(std::string_view loggerName, std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Debug))
            logger.Debug(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Info(std::string_view loggerName, std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Info))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Info(message, location);
        }
    }

    FLOG_FORCE_INLINE void Info(std::string_view loggerName, std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Info))
            logger.Info(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Warn(std::string_view loggerName, std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Warn))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Warn(message, location);
        }
    }

    FLOG_FORCE_INLINE void Warn(std::string_view loggerName, std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Warn))
            logger.Warn(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Error(std::string_view loggerName, std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Error))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Error(message, location);
        }
    }

    FLOG_FORCE_INLINE void Error(std::string_view loggerName, std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Error))
            logger.Error(msg, location);
    }

    template <typename... Args>
    FLOG_FORCE_INLINE void Fatal(std::string_view loggerName, std::format_string<Args...> fmt, Args&&... args, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Fatal))
        {
            std::string message = std::format(fmt, std::forward<Args>(args)...);
            logger.Fatal(message, location);
        }
    }

    FLOG_FORCE_INLINE void Fatal(std::string_view loggerName, std::string_view msg, std::source_location location = std::source_location::current())
    {
        auto& logger = LogManager::GetInstance().GetLogger(loggerName);
        if (logger.IsLevelEnabled(Level::Fatal))
            logger.Fatal(msg, location);
    }
} // namespace FlexLog::Log
