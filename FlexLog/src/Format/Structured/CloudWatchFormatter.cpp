#include "CloudWatchFormatter.h"

#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>

#include "Level.h"

FlexLog::CloudWatchFormatter::CloudWatchFormatter(const Options& options) :
    BaseStructuredFormatter(options),
    m_cwOptions(options)
{
    if (m_options.hostname.empty())
        m_options.hostname = GetHostname();

    if (m_cwOptions.logStreamName.empty())
        m_cwOptions.logStreamName = m_options.hostname;
}

std::string_view FlexLog::CloudWatchFormatter::GetContentType() const
{
    return "application/json";
}

std::unique_ptr<FlexLog::StructuredFormatter> FlexLog::CloudWatchFormatter::Clone() const
{
    return std::make_unique<CloudWatchFormatter>(m_cwOptions);
}

void FlexLog::CloudWatchFormatter::EscapeString(std::ostream& os, std::string_view str) const
{
    for (char c : str)
    {
        switch (c)
        {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\b': os << "\\b"; break;
            case '\f': os << "\\f"; break;
            case '\n': os << "\\n"; break;
            case '\r': os << "\\r"; break;
            case '\t': os << "\\t"; break;
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

std::string FlexLog::CloudWatchFormatter::FormatMessageImpl(const Message& message) const
{
    return FormatForCloudWatch(message);
}

std::string FlexLog::CloudWatchFormatter::FormatStructuredDataImpl(const StructuredData& data) const
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

    // Format fields
    for (size_t i = 0; i < keys.size(); ++i)
    {
        const auto& key = keys[i];
        const auto& value = fields.at(key);

        if (!m_options.includeNullValues && std::holds_alternative<std::nullptr_t>(value))
            continue;

        WriteIndent(ss, 1);
        ss << "\"" << key << "\": ";

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
                ss << "\"" << GetIsoTimestamp(arg) << "\"";
            }
            else if constexpr (std::is_same_v<T, std::vector<std::string>> || 
                std::is_same_v<T, std::vector<int64_t>>                    ||
                std::is_same_v<T, std::vector<double>>                     ||
                std::is_same_v<T, std::vector<bool>>)
            {
                ss << "[" << nl;

                for (size_t j = 0; j < arg.size(); ++j)
                {
                    WriteIndent(ss, 2);

                    if constexpr (std::is_same_v<T, std::vector<std::string>>)
                    {
                        ss << "\"";
                        EscapeString(ss, arg[j]);
                        ss << "\"";
                    }
                    else if constexpr (std::is_same_v<T, std::vector<bool>>)
                    {
                        ss << (arg[j] ? "true" : "false");
                    }
                    else
                    {
                        ss << arg[j];
                    }

                    if (j < arg.size() - 1)
                        ss << ",";

                    ss << nl;
                }

                WriteIndent(ss, 1);
                ss << "]";
            }
        }, value);

        if (i < keys.size() - 1)
            ss << ",";

        ss << nl;
    }

    ss << "}";

    return ss.str();
}

std::string FlexLog::CloudWatchFormatter::FormatForCloudWatch(const Message& message) const
{
    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    // AWS CloudWatch Logs Insights format
    ss << "{" << nl;

    // Timestamp - CloudWatch expects ISO8601 format
    if (m_options.includeTimestamp)
    {
        WriteIndent(ss, 1);
        ss << "\"timestamp\": \"" << GetIsoTimestamp(message.timestamp) << "\"," << nl;
    }

    // AWS CloudWatch metadata
    WriteIndent(ss, 1);
    ss << "\"logGroup\": \"" << m_cwOptions.logGroupName << "\"," << nl;

    WriteIndent(ss, 1);
    ss << "\"logStream\": \"" << m_cwOptions.logStreamName << "\"," << nl;

    // Include plain text message if requested
    if (m_cwOptions.includePlainTextMessage && m_options.includeMessage)
    {
        WriteIndent(ss, 1);
        ss << "\"message\": \"";
        EscapeString(ss, message.message);
        ss << "\"," << nl;
    }

    // Host information
    WriteIndent(ss, 1);
    ss << "\"host\": \"" << m_options.hostname << "\"," << nl;

    // Log level
    if (m_options.includeLevel)
    {
        WriteIndent(ss, 1);
        ss << "\"level\": \"" << LevelToString(message.level) << "\"," << nl;
        WriteIndent(ss, 1);
        ss << "\"levelValue\": " << static_cast<int>(message.level) << "," << nl;
    }

    // Logger name
    if (m_options.includeLogger)
    {
        WriteIndent(ss, 1);
        ss << "\"logger\": \"";
        EscapeString(ss, message.name);
        ss << "\"," << nl;
    }

    // Application information
    WriteIndent(ss, 1);
    ss << "\"app\": \"" << m_options.applicationName << "\"," << nl;

    WriteIndent(ss, 1);
    ss << "\"env\": \"" << m_options.environment << "\"," << nl;

    // Source location information
    if (m_options.includeSourceLocation)
    {
        WriteIndent(ss, 1);
        ss << "\"location\": {" << nl;

        WriteIndent(ss, 2);
        ss << "\"file\": \"" 
            << std::filesystem::path(message.sourceLocation.file_name()).filename().string() 
            << "\"," << nl;

        WriteIndent(ss, 2);
        ss << "\"line\": " << message.sourceLocation.line() << "," << nl;

        WriteIndent(ss, 2);
        ss << "\"function\": \"" << message.sourceLocation.function_name() << "\"" << nl;

        WriteIndent(ss, 1);
        ss << "}," << nl;
    }

    // Process and thread info
    if (m_options.includeProcessInfo)
    {
        WriteIndent(ss, 1);
        ss << "\"process\": {" << nl;

        WriteIndent(ss, 2);
        ss << "\"id\": \"" << GetProcessId() << "\"," << nl;

        WriteIndent(ss, 2);
        ss << "\"name\": \"" << GetProcessName() << "\"" << nl;

        WriteIndent(ss, 1);
        ss << "}," << nl;
    }

    if (m_options.includeThreadId)
    {
        WriteIndent(ss, 1);
        ss << "\"threadId\": \"" << GetThreadId() << "\"," << nl;
    }

    // Tags
    if (!m_options.tags.empty())
    {
        WriteIndent(ss, 1);
        ss << "\"tags\": [" << nl;

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

    // Structured data
    if (!message.structuredData.IsEmpty())
    {
        WriteIndent(ss, 1);
        ss << "\"data\": " << FormatStructuredDataImpl(message.structuredData) << "," << nl;
    }

    // User data
    for (auto it = m_options.userData.begin(); it != m_options.userData.end(); ++it)
    {
        WriteIndent(ss, 1);
        ss << "\"" << it->first << "\": \"" << it->second << "\"";

        if (std::next(it) != m_options.userData.end())
            ss << ",";

        ss << nl;
    }

    // Metadata - CloudWatch format often includes metadata
    WriteIndent(ss, 1);
    ss << "\"@metadata\": {" << nl;
    WriteIndent(ss, 2);
    ss << "\"service\": \"flex_log-logger\"," << nl;
    WriteIndent(ss, 2);
    ss << "\"version\": \"1.0\"" << nl;
    WriteIndent(ss, 1);
    ss << "}" << nl;

    // Close the main object
    ss << "}";

    return ss.str();
}

std::string FlexLog::CloudWatchFormatter::GetIsoTimestamp(const std::chrono::system_clock::time_point& timestamp) const
{
    // Create ISO 8601 timestamp with milliseconds
    // Format: YYYY-MM-DDThh:mm:ss.sssZ
    auto time_t_now = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%FT%T");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';

    return ss.str();
}
