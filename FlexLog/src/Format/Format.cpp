#include "Format.h"

std::string FlexLog::Format::FormatMessage(const Message& msg) const
{
    switch (m_logFormat)
    {
        case LogFormat::CloudWatch:     return m_cloudWatchFormatter.FormatMessage(msg);
        case LogFormat::Elasticsearch:  return m_elasticsearchFormatter.FormatMessage(msg);
        case LogFormat::GELF:           return m_gelfFormatter.FormatMessage(msg);
        case LogFormat::JSON:           return m_jsonFormatter.FormatMessage(msg);
        case LogFormat::Logstash:       return m_logstashFormatter.FormatMessage(msg);
        case LogFormat::OpenTelemetry:  return m_openTelemetryFormatter.FormatMessage(msg);
        case LogFormat::Splunk:         return m_splunkFormatter.FormatMessage(msg);
        case LogFormat::XML:            return m_xmlFormatter.FormatMessage(msg);
        case LogFormat::Pattern:        FLOG_FALLTHROUGH;
        default:                        return m_patternFormatter.FormatMessage(msg);
    }
}
