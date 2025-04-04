#pragma once

#include <string>

#include "BaseStructuredFormatter.h"

namespace FlexLog
{
    // Graylog Extended Log Format (GELF) implementation
    // Spec: https://docs.graylog.org/docs/gelf
    class GelfFormatter : public BaseStructuredFormatter
    {
    public:
        struct Options : public CommonFormatterOptions
        {
            // GELF-specific options
            std::string version = "1.1";  // GELF spec version (usually "1.1")
            bool useCompression = false;  // Whether to compress the output
            bool useFacility = true;      // Whether to include facility field
            std::string facility = "flex_log-logger";  // Facility identifier

            Options& SetVersion(std::string_view ver)
            {
                version = ver;
                return *this;
            }

            Options& SetCompression(bool compress)
            {
                useCompression = compress;
                return *this;
            }

            Options& SetFacility(bool use, std::string_view fac = "flex_log-logger")
            {
                useFacility = use;
                facility = fac;
                return *this;
            }
        };

        explicit GelfFormatter(const Options& options = Options());

        std::string_view GetContentType() const override;
        std::unique_ptr<StructuredFormatter> Clone() const override;

    protected:
        void EscapeString(std::ostream& os, std::string_view str) const override;
        std::string FormatMessageImpl(const Message& message) const override;
        std::string FormatStructuredDataImpl(const StructuredData& data) const override;

        // Converts level to GELF/Syslog severity (0-7)
        int ConvertLevelToSyslogSeverity(Level level) const;

        // Implements GELF compression if needed
        std::string CompressGelfMessage(const std::string& message) const;

        Options m_gelfOptions;
    };
}
