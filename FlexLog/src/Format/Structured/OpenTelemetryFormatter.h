#pragma once

#include "BaseStructuredFormatter.h"
#include <string>

namespace FlexLog
{
    class OpenTelemetryFormatter : public BaseStructuredFormatter
    {
    public:
        struct Options : public CommonFormatterOptions
        {
            // OpenTelemetry-specific options
            std::string schemaUrl = "https://opentelemetry.io/schemas/1.18.0";
            bool useOtelSeverityFormat = true;
            std::string instrumentationScope = "flex_log-logger";
            std::string instrumentationVersion = "1.0.0";
            bool includeTraceContext = false; // Include trace ID & span ID
            std::string traceId; // Optional default trace ID
            std::string spanId;  // Optional default span ID

            // Builder methods
            Options& SetSchemaUrl(std::string_view url)
            {
                schemaUrl = url;
                return *this;
            }

            Options& SetOtelSeverity(bool enable)
            {
                useOtelSeverityFormat = enable;
                return *this;
            }

            Options& SetInstrumentation(std::string_view scope, std::string_view version = "1.0.0")
            {
                instrumentationScope = scope;
                instrumentationVersion = version;
                return *this;
            }

            Options& SetTraceContext(bool include, std::string_view trace = "", std::string_view span = "")
            {
                includeTraceContext = include;
                traceId = trace;
                spanId = span;
                return *this;
            }
        };

        explicit OpenTelemetryFormatter(const Options& options = Options());

        std::string_view GetContentType() const override;
        std::unique_ptr<StructuredFormatter> Clone() const override;

    protected:
        void EscapeString(std::ostream& os, std::string_view str) const override;
        std::string FormatMessageImpl(const Message& message) const override;
        std::string FormatStructuredDataImpl(const StructuredData& data) const override;

        // Convert FlexLog level to OpenTelemetry severity number (1-24)
        int ConvertLevelToOtelSeverity(Level level) const;

        // Get OpenTelemetry severity text based on level
        std::string GetOtelSeverityText(Level level) const;

        // Generate random trace ID and span ID if needed
        std::string GenerateTraceId() const;
        std::string GenerateSpanId() const;

        Options m_otelOptions;
    };
}
