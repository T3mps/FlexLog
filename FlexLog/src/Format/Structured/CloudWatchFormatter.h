#pragma once

#include "BaseStructuredFormatter.h"
#include <string>

namespace FlexLog
{
    class CloudWatchFormatter : public BaseStructuredFormatter
    {
    public:
        struct Options : public CommonFormatterOptions
        {
            // CloudWatch-specific options
            std::string logGroupName = "application-logs";
            std::string logStreamName;  // Default to hostname if empty
            bool includePlainTextMessage = true;  // Include message as plain text field

            Options& SetLogGroup(std::string_view group)
            {
                logGroupName = group;
                return *this;
            }

            Options& SetLogStream(std::string_view stream)
            {
                logStreamName = stream;
                return *this;
            }

            Options& SetIncludePlainText(bool include)
            {
                includePlainTextMessage = include;
                return *this;
            }
        };

        explicit CloudWatchFormatter(const Options& options = Options());

        std::string_view GetContentType() const override;
        std::unique_ptr<StructuredFormatter> Clone() const override;

    protected:
        void EscapeString(std::ostream& os, std::string_view str) const override;
        std::string FormatMessageImpl(const Message& message) const override;
        std::string FormatStructuredDataImpl(const StructuredData& data) const override;

        // Format in AWS CloudWatch Logs format
        std::string FormatForCloudWatch(const Message& message) const;

        // Utility to get ISO 8601 timestamp with milliseconds
        std::string GetIsoTimestamp(const std::chrono::system_clock::time_point& timestamp) const;

        Options m_cwOptions;
    };
}
