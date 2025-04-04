#pragma once

#include "StructuredFormatter.h"

#include <chrono>
#include <string>
#include <ostream>
#include <unordered_map>
#include <thread>
#include <vector>

namespace FlexLog
{
    struct CommonFormatterOptions
    {
        // Service/application information
        std::string applicationName = "flex_log";
        std::string environment = "production";
        std::string hostname;
        std::string serviceName;
        std::string serviceVersion = "1.0.0";

        // Content formatting
        bool prettyPrint = false;
        int indentSize = 2;
        bool includeNullValues = true;
        bool sortKeys = false;
        std::string timeFormat = "%FT%T.%fZ"; // ISO 8601 with microseconds

        // Field inclusion options
        bool includeTimestamp = true;
        bool includeLevel = true;
        bool includeLogger = true;
        bool includeMessage = true;
        bool includeSourceLocation = true;
        bool includeProcessInfo = false;
        bool includeThreadId = false;

        // Custom fields
        std::unordered_map<std::string, std::string> userData;
        std::vector<std::string> tags;

        // Methods for builder pattern
        CommonFormatterOptions& SetApplication(std::string_view app, std::string_view env = "production")
        {
            applicationName = app;
            environment = env;
            return *this;
        }

        CommonFormatterOptions& SetService(std::string_view name, std::string_view version = "1.0.0")
        {
            serviceName = name;
            serviceVersion = version;
            return *this;
        }

        CommonFormatterOptions& SetHost(std::string_view host)
        {
            hostname = host;
            return *this;
        }

        CommonFormatterOptions& SetPrettyPrint(bool enable, int indent = 2)
        {
            prettyPrint = enable;
            indentSize = indent;
            return *this;
        }

        CommonFormatterOptions& SetTimeFormat(std::string_view format)
        {
            timeFormat = format;
            return *this;
        }

        CommonFormatterOptions& SetFieldInclusion(bool timestamp, bool level, bool logger, 
            bool message, bool sourceLocation)
        {
            includeTimestamp = timestamp;
            includeLevel = level;
            includeLogger = logger;
            includeMessage = message;
            includeSourceLocation = sourceLocation;
            return *this;
        }

        CommonFormatterOptions& SetProcessInfo(bool include)
        {
            includeProcessInfo = include;
            return *this;
        }

        CommonFormatterOptions& SetThreadId(bool include)
        {
            includeThreadId = include;
            return *this;
        }

        CommonFormatterOptions& AddField(std::string_view key, std::string_view value)
        {
            userData[std::string(key)] = std::string(value);
            return *this;
        }

        CommonFormatterOptions& AddTag(std::string_view tag)
        {
            tags.push_back(std::string(tag));
            return *this;
        }
    };

    class BaseStructuredFormatter : public StructuredFormatter
    {
    public:
        explicit BaseStructuredFormatter(const CommonFormatterOptions& options = {});
        virtual ~BaseStructuredFormatter() = default;

        // Format a regular message (structured or not)
        std::string FormatMessage(const Message& message) const override;

        // Format only the structured data portion
        std::string FormatStructuredData(const StructuredData& data) const override;

        // Get content type for HTTP headers
        std::string_view GetContentType() const override = 0;

        std::unique_ptr<StructuredFormatter> Clone() const override = 0;

        const CommonFormatterOptions& GetOptions() const { return m_options; }
        void SetOptions(const CommonFormatterOptions& options);

    protected:
        virtual std::string FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
        virtual std::string FormatSourceLocation(const std::source_location& location) const;
        virtual std::string GetProcessId() const;
        virtual std::string GetProcessName() const;
        virtual std::string GetThreadId() const;
        virtual std::string GetHostname() const;

        void WriteIndent(std::ostream& os, int level) const;

        virtual void EscapeString(std::ostream& os, std::string_view str) const = 0;

        virtual std::string FormatMessageImpl(const Message& message) const = 0;
        virtual std::string FormatStructuredDataImpl(const StructuredData& data) const = 0;

        CommonFormatterOptions m_options;
    };
}
