#pragma once

namespace FlexLog
{
    enum class LogFormat
    {
        Pattern = 0,    // Pattern-based format

        CloudWatch,     // AWS CloudWatch format
        Elasticsearch,  // Elasticsearch format
        GELF,           // Graylog Extended Log Format
        JSON,           // Standard JSON
        Logstash,       // Logstash-compatible JSON
        OpenTelemetry,  // OpenTelemetry format
        Splunk,         // Splunk HEC format
        XML             // Standard XML
    };
}
