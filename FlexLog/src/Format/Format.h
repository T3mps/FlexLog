#pragma once

#include <string>

#include "Common.h"
#include "LogFormat.h"
#include "Message.h"
#include "PatternFormatter.h"
#include "Structured/CloudWatchFormatter.h"
#include "Structured/ElasticsearchFormatter.h"
#include "Structured/GelfFormatter.h"
#include "Structured/JsonFormatter.h"
#include "Structured/LogstashFormatter.h"
#include "Structured/OpenTelemetryFormatter.h"
#include "Structured/SplunkFormatter.h"
#include "Structured/XmlFormatter.h"

namespace FlexLog
{
    class Format
    {
    public:
        Format() = default;
        ~Format() = default;

        std::string operator()(const Message& msg) const { return FormatMessage(msg); }
        
        std::string FormatMessage(const Message& msg) const;

        LogFormat GetLogFormat() const          { return m_logFormat; }
        void SetLogFormat(LogFormat logFormat)  { m_logFormat = logFormat; }

        PatternFormatter& GetPatternFormatter()                         { return m_patternFormatter; }
        const PatternFormatter& GetPatternFormatter()             const { return m_patternFormatter; }
        
        CloudWatchFormatter& GetCloudWatchFormatter()                   { return m_cloudWatchFormatter; }
        const CloudWatchFormatter& GetCloudWatchFormatter()       const { return m_cloudWatchFormatter; }
        
        ElasticsearchFormatter& GetElasticsearchFormatter()             { return m_elasticsearchFormatter; }
        const ElasticsearchFormatter& GetElasticsearchFormatter() const { return m_elasticsearchFormatter; }
        
        GelfFormatter& GetGelfFormatter()                               { return m_gelfFormatter; }
        const GelfFormatter& GetGelfFormatter()                   const { return m_gelfFormatter; }
        
        JsonFormatter& GetJsonFormatter()                               { return m_jsonFormatter; }
        const JsonFormatter& GetJsonFormatter()                   const { return m_jsonFormatter; }
        
        LogstashFormatter& GetLogstashFormatter()                       { return m_logstashFormatter; }
        const LogstashFormatter& GetLogstashFormatter()           const { return m_logstashFormatter; }
        
        OpenTelemetryFormatter& GetOpenTelemetryFormatter()             { return m_openTelemetryFormatter; }
        const OpenTelemetryFormatter& GetOpenTelemetryFormatter() const { return m_openTelemetryFormatter; }
        
        SplunkFormatter& GetSplunkFormatter()                           { return m_splunkFormatter; }
        const SplunkFormatter& GetSplunkFormatter()               const { return m_splunkFormatter; }
        
        XmlFormatter& GetXmlFormatter()                                 { return m_xmlFormatter; }
        const XmlFormatter& GetXmlFormatter()                     const { return m_xmlFormatter; }

    private:
        LogFormat m_logFormat = LogFormat::Pattern;

        PatternFormatter m_patternFormatter;

        CloudWatchFormatter m_cloudWatchFormatter;
        ElasticsearchFormatter m_elasticsearchFormatter;
        GelfFormatter m_gelfFormatter;
        JsonFormatter m_jsonFormatter;
        LogstashFormatter m_logstashFormatter;
        OpenTelemetryFormatter m_openTelemetryFormatter;
        SplunkFormatter m_splunkFormatter;
        XmlFormatter m_xmlFormatter;
    };
}
