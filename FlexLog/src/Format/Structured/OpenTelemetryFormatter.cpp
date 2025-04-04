#include "OpenTelemetryFormatter.h"

#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <random>

#include "Level.h"

FlexLog::OpenTelemetryFormatter::OpenTelemetryFormatter(const Options& options) :
    BaseStructuredFormatter(options),
    m_otelOptions(options)
{
    if (m_options.hostname.empty())
        m_options.hostname = GetHostname();
}

std::string_view FlexLog::OpenTelemetryFormatter::GetContentType() const
{
    return "application/json";
}

std::unique_ptr<FlexLog::StructuredFormatter> FlexLog::OpenTelemetryFormatter::Clone() const
{
    return std::make_unique<OpenTelemetryFormatter>(m_otelOptions);
}

void FlexLog::OpenTelemetryFormatter::EscapeString(std::ostream& os, std::string_view str) const
{
    for (char c : str)
    {
        switch (c)
        {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\b': os << "\\b";  break;
            case '\f': os << "\\f";  break;
            case '\n': os << "\\n";  break;
            case '\r': os << "\\r";  break;
            case '\t': os << "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 32)
                {
                    // Control characters
                    os << "\\u"
                        << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(c)
                        << std::dec;
                }
                else
                {
                    os << c;
                }
                break;
        }
    }
}

std::string FlexLog::OpenTelemetryFormatter::FormatMessageImpl(const Message& message) const
{
    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    // Format as OpenTelemetry JSON
    ss << "{" << nl;

    // OpenTelemetry resource context
    WriteIndent(ss, 1);
    ss << "\"resource\": {" << nl;
    WriteIndent(ss, 2);
    ss << "\"attributes\": {" << nl;

    // Resource attributes - standard OpenTelemetry resource attributes
    WriteIndent(ss, 3);
    ss << "\"service.name\": \"" << m_options.serviceName << "\"," << nl;

    WriteIndent(ss, 3);
    ss << "\"service.namespace\": \"" << m_options.applicationName << "\"," << nl;

    if (!m_options.serviceVersion.empty())
    {
        WriteIndent(ss, 3);
        ss << "\"service.version\": \"" << m_options.serviceVersion << "\"," << nl;
    }

    WriteIndent(ss, 3);
    ss << "\"service.instance.id\": \"" << m_options.hostname << "\"," << nl;

    WriteIndent(ss, 3);
    ss << "\"deployment.environment\": \"" << m_options.environment << "\"";

    // Add additional resource attributes from options
    if (!m_options.userData.empty())
    {
        for (const auto& [key, value] : m_options.userData)
        {
            ss << "," << nl;
            WriteIndent(ss, 3);
            ss << "\"" << key << "\": \"" << value << "\"";
        }
    }

    ss << nl;
    WriteIndent(ss, 2);
    ss << "}," << nl;

    // Schema URL
    WriteIndent(ss, 2);
    ss << "\"schema_url\": \"" << m_otelOptions.schemaUrl << "\"" << nl;

    WriteIndent(ss, 1);
    ss << "}," << nl;

    // OpenTelemetry scope context
    WriteIndent(ss, 1);
    ss << "\"scope\": {" << nl;
    WriteIndent(ss, 2);
    ss << "\"name\": \"" << m_otelOptions.instrumentationScope << "\"," << nl;
    WriteIndent(ss, 2);
    ss << "\"version\": \"" << m_otelOptions.instrumentationVersion << "\"" << nl;
    WriteIndent(ss, 1);
    ss << "}," << nl;

    // The actual log record
    WriteIndent(ss, 1);
    ss << "\"logs\": [{" << nl;

    // Timestamp - nanoseconds since epoch
    if (m_options.includeTimestamp)
    {
        WriteIndent(ss, 2);
        auto timeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            message.timestamp.time_since_epoch()).count();
        ss << "\"time_unix_nano\": " << timeNs << "," << nl;
    }

    // Observed timestamp - when the log was processed (typically now)
    WriteIndent(ss, 2);
    auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    ss << "\"observed_time_unix_nano\": " << nowNs << "," << nl;

    // Severity number - OpenTelemetry defines a standard scale
    if (m_options.includeLevel && m_otelOptions.useOtelSeverityFormat)
    {
        WriteIndent(ss, 2);
        ss << "\"severity_number\": " << ConvertLevelToOtelSeverity(message.level) << "," << nl;
        WriteIndent(ss, 2);
        ss << "\"severity_text\": \"" << GetOtelSeverityText(message.level) << "\"," << nl;
    }
    else if (m_options.includeLevel)
    {
        WriteIndent(ss, 2);
        ss << "\"severity_text\": \"" << LevelToString(message.level) << "\"," << nl;
    }

    // Body - the log message content
    if (m_options.includeMessage)
    {
        WriteIndent(ss, 2);
        ss << "\"body\": {" << nl;
        WriteIndent(ss, 3);
        ss << "\"string_value\": \"";
        EscapeString(ss, message.message);
        ss << "\"" << nl;
        WriteIndent(ss, 2);
        ss << "}," << nl;
    }

    // Trace context if enabled
    if (m_otelOptions.includeTraceContext)
    {
        std::string traceId = !m_otelOptions.traceId.empty() ? m_otelOptions.traceId : GenerateTraceId();
        std::string spanId = !m_otelOptions.spanId.empty() ? m_otelOptions.spanId : GenerateSpanId();

        WriteIndent(ss, 2);
        ss << "\"trace_id\": \"" << traceId << "\"," << nl;
        WriteIndent(ss, 2);
        ss << "\"span_id\": \"" << spanId << "\"," << nl;
    }

    // Attributes - these are key-value pairs for the log record
    WriteIndent(ss, 2);
    ss << "\"attributes\": [" << nl;

    std::vector<std::pair<std::string, std::string>> attributes;

    // Logger name
    if (m_options.includeLogger)
        attributes.emplace_back("logger.name", message.name);

    // Source location
    if (m_options.includeSourceLocation)
    {
        attributes.emplace_back("code.filepath", std::filesystem::path(message.sourceLocation.file_name()).filename().string());
        attributes.emplace_back("code.lineno", std::to_string(message.sourceLocation.line()));
        attributes.emplace_back("code.function", message.sourceLocation.function_name());
    }

    // Process info
    if (m_options.includeProcessInfo)
    {
        attributes.emplace_back("process.pid", GetProcessId());
        attributes.emplace_back("process.executable.name", GetProcessName());
    }

    // Thread ID
    if (m_options.includeThreadId)
    {
        attributes.emplace_back("thread.id", GetThreadId());
    }

    // Add structured data fields as attributes
    if (!message.structuredData.IsEmpty())
    {
        const auto& fields = message.structuredData.GetFields();

        for (const auto& [key, value] : fields)
        {
            std::string stringValue;

            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, std::nullptr_t>)
                {
                    // Skip null values or add as special case
                    return;
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    stringValue = arg;
                }
                else if constexpr (std::is_same_v<T, int64_t> || 
                    std::is_same_v<T, uint64_t>)
                {
                    stringValue = std::to_string(arg);
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    stringValue = std::to_string(arg);
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    stringValue = arg ? "true" : "false";
                }
                else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>)
                {
                    auto time_t_value = std::chrono::system_clock::to_time_t(arg);
                    std::stringstream ts;
                    ts << std::put_time(std::gmtime(&time_t_value), "%FT%T.000Z");
                    stringValue = ts.str();
                }
                else if constexpr (std::is_same_v<T, std::vector<std::string>> || 
                    std::is_same_v<T, std::vector<int64_t>> ||
                    std::is_same_v<T, std::vector<double>> ||
                    std::is_same_v<T, std::vector<bool>>)
                {
                    // For vector types, serialize to a string representation
                    std::stringstream vs;
                    vs << "[";

                    for (size_t i = 0; i < arg.size(); ++i)
                    {
                        if (i > 0)
                            vs << ", ";

                        if constexpr (std::is_same_v<T, std::vector<std::string>>)
                            vs << "\"" << arg[i] << "\"";
                        else
                            vs << arg[i];
                    }

                    vs << "]";
                    stringValue = vs.str();
                }
                }, value);

            // Only add attribute if we have a value
            if (!stringValue.empty())
            {
                attributes.emplace_back(key, stringValue);
            }
        }
    }

    // Output all collected attributes
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        const auto& [key, value] = attributes[i];

        WriteIndent(ss, 3);
        ss << "{" << nl;
        WriteIndent(ss, 4);
        ss << "\"key\": \"" << key << "\"," << nl;
        WriteIndent(ss, 4);
        ss << "\"value\": {" << nl;
        WriteIndent(ss, 5);
        ss << "\"string_value\": \"";
        EscapeString(ss, value);
        ss << "\"" << nl;
        WriteIndent(ss, 4);
        ss << "}" << nl;
        WriteIndent(ss, 3);
        ss << "}";

        if (i < attributes.size() - 1)
            ss << ",";

        ss << nl;
    }

    WriteIndent(ss, 2);
    ss << "]" << nl;

    WriteIndent(ss, 1);
    ss << "}]" << nl;

    // Close the main object
    ss << "}";

    return ss.str();
}

std::string FlexLog::OpenTelemetryFormatter::FormatStructuredDataImpl(const StructuredData& data) const
{
    // Format as OpenTelemetry attributes
    if (data.IsEmpty())
        return "[]";

    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    ss << "[" << nl;

    const auto& fields = data.GetFields();

    // Sort keys if needed
    std::vector<std::string> keys;
    keys.reserve(fields.size());

    for (const auto& [key, _] : fields)
    {
        keys.push_back(key);
    }

    if (m_options.sortKeys)
        std::sort(keys.begin(), keys.end());

    // Format fields as OpenTelemetry attributes
    for (size_t i = 0; i < keys.size(); ++i)
    {
        const auto& key = keys[i];
        const auto& value = fields.at(key);

        if (!m_options.includeNullValues && std::holds_alternative<std::nullptr_t>(value))
            continue;

        WriteIndent(ss, 1);
        ss << "{" << nl;
        WriteIndent(ss, 2);
        ss << "\"key\": \"" << key << "\"," << nl;
        WriteIndent(ss, 2);
        ss << "\"value\": ";

        std::visit([&](auto&& arg)
        {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>)
            {
                ss << "{" << nl;
                WriteIndent(ss, 3);
                ss << "\"string_value\": \"null\"" << nl;
                WriteIndent(ss, 2);
                ss << "}";
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                ss << "{" << nl;
                WriteIndent(ss, 3);
                ss << "\"string_value\": \"";
                EscapeString(ss, arg);
                ss << "\"" << nl;
                WriteIndent(ss, 2);
                ss << "}";
            }
            else if constexpr (std::is_same_v<T, int64_t>)
            {
                ss << "{" << nl;
                WriteIndent(ss, 3);
                ss << "\"int_value\": " << arg << nl;
                WriteIndent(ss, 2);
                ss << "}";
            }
            else if constexpr (std::is_same_v<T, uint64_t>)
            {
                ss << "{" << nl;
                WriteIndent(ss, 3);
                ss << "\"int_value\": " << arg << nl;
                WriteIndent(ss, 2);
                ss << "}";
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                ss << "{" << nl;
                WriteIndent(ss, 3);
                ss << "\"double_value\": " << std::fixed << arg << nl;
                WriteIndent(ss, 2);
                ss << "}";
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                ss << "{" << nl;
                WriteIndent(ss, 3);
                ss << "\"bool_value\": " << (arg ? "true" : "false") << nl;
                WriteIndent(ss, 2);
                ss << "}";
            }
            else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>)
            {
                ss << "{" << nl;
                WriteIndent(ss, 3);
                ss << "\"string_value\": \"" << FormatTimestamp(arg) << "\"" << nl;
                WriteIndent(ss, 2);
                ss << "}";
            }
            else if constexpr (std::is_same_v<T, std::vector<std::string>> || 
                std::is_same_v<T, std::vector<int64_t>> ||
                std::is_same_v<T, std::vector<double>> ||
                std::is_same_v<T, std::vector<bool>>)
            {
                ss << "{" << nl;
                WriteIndent(ss, 3);
                ss << "\"array_value\": {" << nl;
                WriteIndent(ss, 4);
                ss << "\"values\": [" << nl;

                for (size_t j = 0; j < arg.size(); ++j)
                {
                    WriteIndent(ss, 5);

                    if constexpr (std::is_same_v<T, std::vector<std::string>>)
                    {
                        ss << "{" << nl;
                        WriteIndent(ss, 6);
                        ss << "\"string_value\": \"";
                        EscapeString(ss, arg[j]);
                        ss << "\"" << nl;
                        WriteIndent(ss, 5);
                        ss << "}";
                    }
                    else if constexpr (std::is_same_v<T, std::vector<int64_t>>)
                    {
                        ss << "{" << nl;
                        WriteIndent(ss, 6);
                        ss << "\"int_value\": " << arg[j] << nl;
                        WriteIndent(ss, 5);
                        ss << "}";
                    }
                    else if constexpr (std::is_same_v<T, std::vector<double>>)
                    {
                        ss << "{" << nl;
                        WriteIndent(ss, 6);
                        ss << "\"double_value\": " << std::fixed << arg[j] << nl;
                        WriteIndent(ss, 5);
                        ss << "}";
                    }
                    else if constexpr (std::is_same_v<T, std::vector<bool>>)
                    {
                        ss << "{" << nl;
                        WriteIndent(ss, 6);
                        ss << "\"bool_value\": " << (arg[j] ? "true" : "false") << nl;
                        WriteIndent(ss, 5);
                        ss << "}";
                    }

                    if (j < arg.size() - 1)
                        ss << ",";

                    ss << nl;
                }

                WriteIndent(ss, 4);
                ss << "]" << nl;
                WriteIndent(ss, 3);
                ss << "}" << nl;
                WriteIndent(ss, 2);
                ss << "}";
            }
        }, value);

        ss << nl;
        WriteIndent(ss, 1);
        ss << "}";

        if (i < keys.size() - 1)
            ss << ",";

        ss << nl;
    }

    ss << "]";

    return ss.str();
}

int FlexLog::OpenTelemetryFormatter::ConvertLevelToOtelSeverity(Level level) const
{
    // Convert FlexLog log levels to OpenTelemetry severity numbers
    // OTel defines 1-24 with: 1-4=TRACE, 5-8=DEBUG, 9-12=INFO, 13-16=WARN, 17-20=ERROR, 21-24=FATAL
    switch (level)
    {
        case Level::Trace:  return 3;  // TRACE3
        case Level::Debug:  return 7;  // DEBUG3
        case Level::Info:   return 11; // INFO3
        case Level::Warn:   return 15; // WARN3
        case Level::Error:  return 19; // ERROR3
        case Level::Fatal:  return 23; // FATAL3
        default:            return 11; // INFO3 as default
    }
}

std::string FlexLog::OpenTelemetryFormatter::GetOtelSeverityText(Level level) const
{
    // Convert FlexLog log levels to OpenTelemetry severity text
    switch (level)
    {
        case Level::Trace:  return "TRACE3";
        case Level::Debug:  return "DEBUG3";
        case Level::Info:   return "INFO3";
        case Level::Warn:   return "WARN3";
        case Level::Error:  return "ERROR3";
        case Level::Fatal:  return "FATAL3";
        default:            return "INFO3";
    }
}

std::string FlexLog::OpenTelemetryFormatter::GenerateTraceId() const
{
    // Generate a 16-byte (32 hex char) trace ID
    static thread_local std::mt19937_64 rng(std::random_device{}());
    static thread_local std::uniform_int_distribution<uint32_t> dist(0, 15);
    static const char* hexChars = "0123456789abcdef";

    std::string traceId(32, '0');

    for (size_t i = 0; i < 32; ++i)
    {
        traceId[i] = hexChars[dist(rng)];
    }

    return traceId;
}

std::string FlexLog::OpenTelemetryFormatter::GenerateSpanId() const
{
    // Generate an 8-byte (16 hex char) span ID
    static thread_local std::mt19937_64 rng(std::random_device{}());
    static thread_local std::uniform_int_distribution<uint32_t> dist(0, 15);
    static const char* hexChars = "0123456789abcdef";

    std::string spanId(16, '0');

    for (size_t i = 0; i < 16; ++i)
    {
        spanId[i] = hexChars[dist(rng)];
    }

    return spanId;
}
