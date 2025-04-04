#pragma once

#include "BaseStructuredFormatter.h"

namespace FlexLog
{
    class LogstashFormatter : public BaseStructuredFormatter
    {
    public:
        struct Options : public CommonFormatterOptions
        {
            // Logstash-specific options
            std::string logstashType = "flex_log";
            bool includeLogstashTags = true;

            Options& SetType(std::string_view type)
            {
                logstashType = type;
                return *this;
            }

            Options& SetIncludeTags(bool include)
            {
                includeLogstashTags = include;
                return *this;
            }
        };

        explicit LogstashFormatter(const Options& options = Options());

        std::string_view GetContentType() const override;
        std::unique_ptr<StructuredFormatter> Clone() const override;

    protected:
        void EscapeString(std::ostream& os, std::string_view str) const override;
        std::string FormatMessageImpl(const Message& message) const override;
        std::string FormatStructuredDataImpl(const StructuredData& data) const override;

        Options m_logstashOptions;
    };
}
