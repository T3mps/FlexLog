#include "SplunkFormatter.h"

#include <sstream>
#include <iomanip>
#include <filesystem>

#include "Level.h"

FlexLog::SplunkFormatter::SplunkFormatter(const Options& options) : BaseStructuredFormatter(options), m_splunkOptions(options)
{
    // Initialize hostname if needed
    if (m_options.hostname.empty())
        m_options.hostname = GetHostname();

    // Set default source and sourcetype if not provided
    if (m_splunkOptions.source.empty())
        m_splunkOptions.source = m_options.applicationName;

    if (m_splunkOptions.sourceType.empty())
        m_splunkOptions.sourceType = "flex_log:log";
}

std::string_view FlexLog::SplunkFormatter::GetContentType() const
{
    return "application/json";
}

std::unique_ptr<FlexLog::StructuredFormatter> FlexLog::SplunkFormatter::Clone() const
{
    return std::make_unique<FlexLog::SplunkFormatter>(m_splunkOptions);
}

void FlexLog::SplunkFormatter::EscapeString(std::ostream& os, std::string_view str) const
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

std::string FlexLog::SplunkFormatter::FormatMessageImpl(const Message& message) const
{
    if (m_splunkOptions.useHEC)
        return FormatForHEC(message);
    return FormatForSplunkJson(message);
}

std::string FlexLog::SplunkFormatter::FormatStructuredDataImpl(const StructuredData& data) const
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
                ss << "\"" << FormatTimestamp(arg) << "\"";
            }
            else if constexpr (std::is_same_v<T, std::vector<std::string>> || 
                std::is_same_v<T, std::vector<int64_t>> ||
                std::is_same_v<T, std::vector<double>> ||
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

std::string FlexLog::SplunkFormatter::FormatForHEC(const Message& message) const
{
    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    // Format for Splunk HTTP Event Collector (HEC)
    ss << "{" << nl;

    // Required HEC fields

    // Event time in epoch seconds
    if (m_options.includeTimestamp)
    {
        WriteIndent(ss, 1);
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            message.timestamp.time_since_epoch()).count() / 1000.0;
        ss << "\"time\": " << std::fixed << std::setprecision(3) << timestamp << "," << nl;
    }

    // Source, sourcetype, and index (optional)
    WriteIndent(ss, 1);
    ss << "\"source\": \"" << m_splunkOptions.source << "\"," << nl;

    WriteIndent(ss, 1);
    ss << "\"sourcetype\": \"" << m_splunkOptions.sourceType << "\"," << nl;

    if (!m_splunkOptions.index.empty())
    {
        WriteIndent(ss, 1);
        ss << "\"index\": \"" << m_splunkOptions.index << "\"," << nl;
    }

    // Host
    WriteIndent(ss, 1);
    ss << "\"host\": \"" << m_options.hostname << "\"," << nl;

    // Event data
    WriteIndent(ss, 1);
    ss << "\"event\": {" << nl;

    // Message content
    if (m_options.includeMessage)
    {
        WriteIndent(ss, 2);
        ss << "\"message\": \"";
        EscapeString(ss, message.message);
        ss << "\"," << nl;
    }

    // Logger name
    if (m_options.includeLogger)
    {
        WriteIndent(ss, 2);
        ss << "\"logger_name\": \"";
        EscapeString(ss, message.name);
        ss << "\"," << nl;
    }

    // Log level
    if (m_options.includeLevel)
    {
        WriteIndent(ss, 2);
        ss << "\"level\": \"" << LevelToString(message.level) << "\"," << nl;
        WriteIndent(ss, 2);
        ss << "\"level_value\": " << static_cast<int>(message.level) << "," << nl;
    }

    // Application information
    WriteIndent(ss, 2);
    ss << "\"application\": \"" << m_options.applicationName << "\"," << nl;

    WriteIndent(ss, 2);
    ss << "\"environment\": \"" << m_options.environment << "\"," << nl;

    // Source location
    if (m_options.includeSourceLocation)
    {
        WriteIndent(ss, 2);
        ss << "\"file\": \"" 
            << std::filesystem::path(message.sourceLocation.file_name()).filename().string() 
            << "\"," << nl;

        WriteIndent(ss, 2);
        ss << "\"line\": " << message.sourceLocation.line() << "," << nl;

        WriteIndent(ss, 2);
        ss << "\"function\": \"" << message.sourceLocation.function_name() << "\"," << nl;
    }

    // Process and thread info
    if (m_options.includeProcessInfo)
    {
        WriteIndent(ss, 2);
        ss << "\"process_id\": \"" << GetProcessId() << "\"," << nl;

        WriteIndent(ss, 2);
        ss << "\"process_name\": \"" << GetProcessName() << "\"," << nl;
    }

    if (m_options.includeThreadId)
    {
        WriteIndent(ss, 2);
        ss << "\"thread_id\": \"" << GetThreadId() << "\"," << nl;
    }

    // Tags
    if (!m_options.tags.empty())
    {
        WriteIndent(ss, 2);
        ss << "\"tags\": [" << nl;

        for (size_t i = 0; i < m_options.tags.size(); ++i)
        {
            WriteIndent(ss, 3);
            ss << "\"" << m_options.tags[i] << "\"";

            if (i < m_options.tags.size() - 1)
                ss << ",";

            ss << nl;
        }

        WriteIndent(ss, 2);
        ss << "]," << nl;
    }

    // Structured data
    if (!message.structuredData.IsEmpty())
    {
        const auto& fields = message.structuredData.GetFields();

        for (const auto& [key, value] : fields)
        {
            WriteIndent(ss, 2);
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
                    ss << "\"" << FormatTimestamp(arg) << "\"";
                }
                else if constexpr (std::is_same_v<T, std::vector<std::string>> || 
                    std::is_same_v<T, std::vector<int64_t>>                    ||
                    std::is_same_v<T, std::vector<double>>                     ||
                    std::is_same_v<T, std::vector<bool>>)
                {
                    ss << "[" << nl;

                    for (size_t i = 0; i < arg.size(); ++i)
                    {
                        WriteIndent(ss, 3);

                        if constexpr (std::is_same_v<T, std::vector<std::string>>)
                        {
                            ss << "\"";
                            EscapeString(ss, arg[i]);
                            ss << "\"";
                        }
                        else if constexpr (std::is_same_v<T, std::vector<bool>>)
                        {
                            ss << (arg[i] ? "true" : "false");
                        }
                        else
                        {
                            ss << arg[i];
                        }

                        if (i < arg.size() - 1)
                            ss << ",";

                        ss << nl;
                    }

                    WriteIndent(ss, 2);
                    ss << "]";
                }
            }, value);

            ss << "," << nl;
        }
    }

    // Additional fields
    for (auto it = m_options.userData.begin(); it != m_options.userData.end(); ++it)
    {
        WriteIndent(ss, 2);
        ss << "\"" << it->first << "\": \"" << it->second << "\"";

        if (std::next(it) != m_options.userData.end())
            ss << ",";

        ss << nl;
    }

    // Close the event object
    WriteIndent(ss, 1);
    ss << "}" << nl;

    // Close the main object
    ss << "}";

    return ss.str();
}

std::string FlexLog::SplunkFormatter::FormatForSplunkJson(const Message& message) const
{
    // Regular JSON format - similar to regular JSON formatter but with Splunk-specific fields
    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    ss << "{" << nl;

    // Timestamp
    if (m_options.includeTimestamp)
    {
        WriteIndent(ss, 1);
        ss << "\"timestamp\": \"" << FormatTimestamp(message.timestamp) << "\"," << nl;

        // Also include Splunk-friendly epoch time
        WriteIndent(ss, 1);
        auto epochTime = std::chrono::duration_cast<std::chrono::milliseconds>(message.timestamp.time_since_epoch()).count() / 1000.0;
        ss << "\"time\": " << std::fixed << std::setprecision(3) << epochTime << "," << nl;
    }

    // Message content
    if (m_options.includeMessage)
    {
        WriteIndent(ss, 1);
        ss << "\"message\": \"";
        EscapeString(ss, message.message);
        ss << "\"," << nl;
    }

    // Splunk metadata
    WriteIndent(ss, 1);
    ss << "\"source\": \"" << m_splunkOptions.source << "\"," << nl;

    WriteIndent(ss, 1);
    ss << "\"sourcetype\": \"" << m_splunkOptions.sourceType << "\"," << nl;

    if (!m_splunkOptions.index.empty())
    {
        WriteIndent(ss, 1);
        ss << "\"index\": \"" << m_splunkOptions.index << "\"," << nl;
    }

    WriteIndent(ss, 1);
    ss << "\"host\": \"" << m_options.hostname << "\"," << nl;

    // Logger and level
    if (m_options.includeLogger)
    {
        WriteIndent(ss, 1);
        ss << "\"logger\": \"";
        EscapeString(ss, message.name);
        ss << "\"," << nl;
    }

    if (m_options.includeLevel)
    {
        WriteIndent(ss, 1);
        ss << "\"level\": \"" << LevelToString(message.level) << "\"," << nl;
        WriteIndent(ss, 1);
        ss << "\"severity\": " << static_cast<int>(message.level) << "," << nl;
    }

    // Application information
    WriteIndent(ss, 1);
    ss << "\"application\": \"" << m_options.applicationName << "\"," << nl;

    WriteIndent(ss, 1);
    ss << "\"environment\": \"" << m_options.environment << "\"," << nl;

    // Source location
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
    if (m_options.includeProcessInfo || m_options.includeThreadId)
    {
        WriteIndent(ss, 1);
        ss << "\"process\": {" << nl;

        if (m_options.includeProcessInfo)
        {
            WriteIndent(ss, 2);
            ss << "\"pid\": \"" << GetProcessId() << "\"," << nl;
            WriteIndent(ss, 2);
            ss << "\"name\": \"" << GetProcessName() << "\"";

            if (m_options.includeThreadId)
                ss << ",";

            ss << nl;
        }

        if (m_options.includeThreadId)
        {
            WriteIndent(ss, 2);
            ss << "\"thread_id\": \"" << GetThreadId() << "\"" << nl;
        }

        WriteIndent(ss, 1);
        ss << "}," << nl;
    }

    // Structured data
    if (!message.structuredData.IsEmpty())
    {
        WriteIndent(ss, 1);
        ss << "\"data\": " << FormatStructuredDataImpl(message.structuredData) << "," << nl;
    }

    // Additional fields
    for (auto it = m_options.userData.begin(); it != m_options.userData.end(); ++it)
    {
        WriteIndent(ss, 1);
        ss << "\"" << it->first << "\": \"" << it->second << "\"";

        if (std::next(it) != m_options.userData.end())
            ss << ",";

        ss << nl;
    }

    // Close main object
    ss << "}";

    return ss.str();
}
