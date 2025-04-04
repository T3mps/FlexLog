#pragma once

#include <cstdint>
#include <source_location>
#include <string>
#include <string_view>

#include "Common.h"
#include "Level.h"

namespace FlexLog
{
	class LoggingService
	{
	public:
		LoggingService() = default;
		virtual ~LoggingService() = default;

        virtual FLOG_FORCE_INLINE bool Log(std::string_view msg, Level level, const std::source_location& location = std::source_location::current()) = 0;

        virtual FLOG_FORCE_INLINE bool Log(std::string_view msg, const StructuredData& data, Level level, const std::source_location& location = std::source_location::current()) = 0;

        virtual FLOG_FORCE_INLINE bool Trace(std::string_view msg, const std::source_location& location = std::source_location::current()) { return Log(msg, Level::Trace, location); }
        virtual FLOG_FORCE_INLINE bool Debug(std::string_view msg, const std::source_location& location = std::source_location::current()) { return Log(msg, Level::Debug, location); }
        virtual FLOG_FORCE_INLINE bool Info(std::string_view msg, const std::source_location& location = std::source_location::current())  { return Log(msg, Level::Info, location); }
        virtual FLOG_FORCE_INLINE bool Warn(std::string_view msg, const std::source_location& location = std::source_location::current())  { return Log(msg, Level::Warn, location); }
        virtual FLOG_FORCE_INLINE bool Error(std::string_view msg, const std::source_location& location = std::source_location::current()) { return Log(msg, Level::Error, location); }
        virtual FLOG_FORCE_INLINE bool Fatal(std::string_view msg, const std::source_location& location = std::source_location::current()) { return Log(msg, Level::Fatal, location); }

        virtual FLOG_FORCE_INLINE bool Trace(std::string_view msg, const StructuredData& data, const std::source_location& location = std::source_location::current()) { return Log(msg, data, Level::Trace, location); }
        virtual FLOG_FORCE_INLINE bool Debug(std::string_view msg, const StructuredData& data, const std::source_location& location = std::source_location::current()) { return Log(msg, data, Level::Debug, location); }
        virtual FLOG_FORCE_INLINE bool Info(std::string_view msg, const StructuredData& data, const std::source_location& location = std::source_location::current())  { return Log(msg, data, Level::Info, location); }
        virtual FLOG_FORCE_INLINE bool Warn(std::string_view msg, const StructuredData& data, const std::source_location& location = std::source_location::current())  { return Log(msg, data, Level::Warn, location); }
        virtual FLOG_FORCE_INLINE bool Error(std::string_view msg, const StructuredData& data, const std::source_location& location = std::source_location::current()) { return Log(msg, data, Level::Error, location); }
        virtual FLOG_FORCE_INLINE bool Fatal(std::string_view msg, const StructuredData& data, const std::source_location& location = std::source_location::current()) { return Log(msg, data, Level::Fatal, location); }
	};
}
