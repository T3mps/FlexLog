#pragma once

#include "BaseStructuredFormatter.h"
#include <string>

namespace FlexLog
{
    class SplunkFormatter : public BaseStructuredFormatter
    {
    public:
        struct Options : public CommonFormatterOptions
        {
            // Splunk-specific options
            bool useHEC = true;        // Format for HTTP Event Collector
            std::string source;        // Splunk source field
            std::string sourceType;    // Splunk sourcetype field
            std::string index;         // Splunk index to write to

            Options& SetHEC(bool enable)
            {
                useHEC = enable;
                return *this;
            }

            Options& SetSource(std::string_view src)
            {
                source = src;
                return *this;
            }

            Options& SetSourceType(std::string_view type)
            {
                sourceType = type;
                return *this;
            }

            Options& SetIndex(std::string_view idx)
            {
                index = idx;
                return *this;
            }
        };

        explicit SplunkFormatter(const Options& options = Options());

        std::string_view GetContentType() const override;
        std::unique_ptr<StructuredFormatter> Clone() const override;

    protected:
        void EscapeString(std::ostream& os, std::string_view str) const override;
        std::string FormatMessageImpl(const Message& message) const override;
        std::string FormatStructuredDataImpl(const StructuredData& data) const override;

        // Format for HTTP Event Collector
        std::string FormatForHEC(const Message& message) const;

        // Format for standard Splunk JSON format
        std::string FormatForSplunkJson(const Message& message) const;

        Options m_splunkOptions;
    };
}
