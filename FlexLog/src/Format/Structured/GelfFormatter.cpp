#include "GelfFormatter.h"

#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <cmath>

#include "Level.h"
#include "Platform.h"

FlexLog::GelfFormatter::GelfFormatter(const Options& options) :
    BaseStructuredFormatter(options),
    m_gelfOptions(options)
{
    if (m_options.hostname.empty())
        m_options.hostname = GetHostname();
}

std::string_view FlexLog::GelfFormatter::GetContentType() const
{
    return "application/json";
}

std::unique_ptr<FlexLog::StructuredFormatter> FlexLog::GelfFormatter::Clone() const
{
    return std::make_unique<GelfFormatter>(m_gelfOptions);
}

void FlexLog::GelfFormatter::EscapeString(std::ostream& os, std::string_view str) const
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

std::string FlexLog::GelfFormatter::FormatMessageImpl(const Message& message) const
{
    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    // Format as GELF JSON
    ss << "{" << nl;

    // Required GELF fields
    WriteIndent(ss, 1);
    ss << "\"version\": \"" << m_gelfOptions.version << "\"," << nl;

    WriteIndent(ss, 1);
    ss << "\"host\": \"" << m_options.hostname << "\"," << nl;

    // Short message is required - use the first line or truncate if needed
    WriteIndent(ss, 1);
    ss << "\"short_message\": \"";

    std::string shortMessage = std::string(message.message);
    size_t newlinePos = shortMessage.find('\n');
    if (newlinePos != std::string::npos)
        shortMessage = shortMessage.substr(0, newlinePos);

    // GELF spec recommends short_message <= 250 chars
    if (shortMessage.length() > 250)
        shortMessage = shortMessage.substr(0, 247) + "...";

    EscapeString(ss, shortMessage);
    ss << "\"," << nl;

    // Full message if it differs from short message
    if (message.message.length() != shortMessage.length())
    {
        WriteIndent(ss, 1);
        ss << "\"full_message\": \"";
        EscapeString(ss, message.message);
        ss << "\"," << nl;
    }

    // Timestamp in UNIX epoch with millisecond precision
    if (m_options.includeTimestamp)
    {
        WriteIndent(ss, 1);
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(message.timestamp.time_since_epoch()).count() / 1000.0;
        ss << "\"timestamp\": " << std::fixed << std::setprecision(3) << timestamp << "," << nl;
    }

    // Level (convert to syslog scale)
    if (m_options.includeLevel)
    {
        WriteIndent(ss, 1);
        ss << "\"level\": " << ConvertLevelToSyslogSeverity(message.level) << "," << nl;
    }

    // Facility (optional in GELF)
    if (m_gelfOptions.useFacility)
    {
        WriteIndent(ss, 1);
        ss << "\"facility\": \"" << m_gelfOptions.facility << "\"," << nl;
    }

    // Additional fields - must be prefixed with _ for GELF compatibility

    // Logger name
    if (m_options.includeLogger)
    {
        WriteIndent(ss, 1);
        ss << "\"_logger\": \"";
        EscapeString(ss, message.name);
        ss << "\"," << nl;
    }

    // Application info
    WriteIndent(ss, 1);
    ss << "\"_application\": \"" << m_options.applicationName << "\"," << nl;

    WriteIndent(ss, 1);
    ss << "\"_environment\": \"" << m_options.environment << "\"," << nl;

    // Source location
    if (m_options.includeSourceLocation)
    {
        WriteIndent(ss, 1);
        ss << "\"_file\": \"" 
            << std::filesystem::path(message.sourceLocation.file_name()).filename().string() 
            << "\"," << nl;

        WriteIndent(ss, 1);
        ss << "\"_line\": " << message.sourceLocation.line() << "," << nl;

        WriteIndent(ss, 1);
        ss << "\"_function\": \"" << message.sourceLocation.function_name() << "\"," << nl;
    }

    // Process info
    if (m_options.includeProcessInfo)
    {
        WriteIndent(ss, 1);
        ss << "\"_process_id\": \"" << GetProcessId() << "\"," << nl;

        WriteIndent(ss, 1);
        ss << "\"_process_name\": \"" << GetProcessName() << "\"," << nl;
    }

    // Thread ID
    if (m_options.includeThreadId)
    {
        WriteIndent(ss, 1);
        ss << "\"_thread_id\": \"" << GetThreadId() << "\"," << nl;
    }

    // Tags
    if (!m_options.tags.empty())
    {
        WriteIndent(ss, 1);
        ss << "\"_tags\": [" << nl;

        for (size_t i = 0; i < m_options.tags.size(); ++i)
        {
            WriteIndent(ss, 2);
            ss << "\"" << m_options.tags[i] << "\"";

            if (i < m_options.tags.size() - 1)
                ss << ",";

            ss << nl;
        }

        WriteIndent(ss, 1);
        ss << "]," << nl;
    }

    // Structured data fields
    if (!message.structuredData.IsEmpty())
    {
        const auto& fields = message.structuredData.GetFields();

        // Convert to _ prefixed fields as required by GELF
        for (const auto& [key, value] : fields)
        {
            std::string prefixedKey = "_" + key;

            WriteIndent(ss, 1);
            ss << "\"" << prefixedKey << "\": ";

            std::visit([&](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, std::nullptr_t>)
                {
                    ss << "null";
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    ss << "\"";
                    EscapeString(ss, arg);
                    ss << "\"";
                }
                else if constexpr (std::is_same_v<T, int64_t> || 
                    std::is_same_v<T, uint64_t>)
                {
                    ss << arg;
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    ss << std::fixed << arg;
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    ss << (arg ? "true" : "false");
                }
                else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>)
                {
                    // Convert to UNIX timestamp with millisecond precision
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        arg.time_since_epoch()).count();
                    ss << std::fixed << std::setprecision(3) << (ms / 1000.0);
                }
                else if constexpr (std::is_same_v<T, std::vector<std::string>> || 
                    std::is_same_v<T, std::vector<int64_t>>                    ||
                    std::is_same_v<T, std::vector<double>>                     ||
                    std::is_same_v<T, std::vector<bool>>)
                {
                    // Arrays need to be stringified for GELF
                    ss << "\"[";

                    for (size_t i = 0; i < arg.size(); ++i)
                    {
                        if (i > 0)
                            ss << ", ";

                        if constexpr (std::is_same_v<T, std::vector<std::string>>)
                        {
                            // Need to escape quotes within the string
                            std::stringstream tmp;
                            tmp << "\\\"";
                            EscapeString(tmp, arg[i]);
                            tmp << "\\\"";
                            ss << tmp.str();
                        }
                        else if constexpr (std::is_same_v<T, std::vector<bool>>)
                        {
                            ss << (arg[i] ? "true" : "false");
                        }
                        else
                        {
                            ss << arg[i];
                        }
                    }

                    ss << "]\"";
                }
            }, value);

            ss << "," << nl;
        }
    }

    for (auto it = m_options.userData.begin(); it != m_options.userData.end(); ++it)
    {
        std::string prefixedKey = "_" + it->first;

        WriteIndent(ss, 1);
        ss << "\"" << prefixedKey << "\": \"" << it->second << "\"";

        if (std::next(it) != m_options.userData.end())
            ss << ",";

        ss << nl;
    }

    // Close the JSON object
    ss << "}";

    std::string result = ss.str();

    // Compress if requested
    if (m_gelfOptions.useCompression)
        return CompressGelfMessage(result);

    return result;
}

std::string FlexLog::GelfFormatter::FormatStructuredDataImpl(const StructuredData& data) const
{
    if (data.IsEmpty())
        return "{}";

    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    ss << "{" << nl;

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

    // Format fields with _ prefix as required by GELF
    for (size_t i = 0; i < keys.size(); ++i)
    {
        const auto& key = keys[i];
        const auto& value = fields.at(key);

        if (!m_options.includeNullValues && std::holds_alternative<std::nullptr_t>(value))
            continue;

        std::string prefixedKey = "_" + key;

        WriteIndent(ss, 1);
        ss << "\"" << prefixedKey << "\": ";

        std::visit([&](auto&& arg)
        {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>)
            {
                ss << "null";
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                ss << "\"";
                EscapeString(ss, arg);
                ss << "\"";
            }
            else if constexpr (std::is_same_v<T, int64_t> || 
                std::is_same_v<T, uint64_t>)
            {
                ss << arg;
            }
            else if constexpr (std::is_same_v<T, double>)
            {
                ss << std::fixed << arg;
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                ss << (arg ? "true" : "false");
            }
            else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>)
            {
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    arg.time_since_epoch()).count();
                ss << std::fixed << std::setprecision(3) << (ms / 1000.0);
            }
            else if constexpr (std::is_same_v<T, std::vector<std::string>> || 
                std::is_same_v<T, std::vector<int64_t>> ||
                std::is_same_v<T, std::vector<double>> ||
                std::is_same_v<T, std::vector<bool>>)
            {
                ss << "\"[";

                for (size_t j = 0; j < arg.size(); ++j)
                {
                    if (j > 0)
                        ss << ", ";

                    if constexpr (std::is_same_v<T, std::vector<std::string>>)
                    {
                        std::stringstream tmp;
                        tmp << "\\\"";
                        EscapeString(tmp, arg[j]);
                        tmp << "\\\"";
                        ss << tmp.str();
                    }
                    else if constexpr (std::is_same_v<T, std::vector<bool>>)
                    {
                        ss << (arg[j] ? "true" : "false");
                    }
                    else
                    {
                        ss << arg[j];
                    }
                }

                ss << "]\"";
            }
        }, value);

        if (i < keys.size() - 1)
            ss << ",";

        ss << nl;
    }

    ss << "}";

    return ss.str();
}

int FlexLog::GelfFormatter::ConvertLevelToSyslogSeverity(Level level) const
{
    // Convert FlexLog log levels to syslog severity levels
    // Syslog severity: 0=Emergency, 1=Alert, 2=Critical, 3=Error, 
    // 4=Warning, 5=Notice, 6=Informational, 7=Debug
    switch (level)
    {
        case Level::Trace:  return 7; // Debug
        case Level::Debug:  return 7; // Debug
        case Level::Info:   return 6; // Informational
        case Level::Warn:   return 4; // Warning
        case Level::Error:  return 3; // Error
        case Level::Fatal:  return 2; // Critical
        default:            return 6; // Informational as default
    }
}

std::string FlexLog::GelfFormatter::CompressGelfMessage(const std::string& message) const
{
    // TODO: use zlib to compress
    // For now, we'll just return the original message
    return message;
}
