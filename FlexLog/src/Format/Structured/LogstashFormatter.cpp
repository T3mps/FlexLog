#include "LogstashFormatter.h"

#include <sstream>
#include <iomanip>
#include <thread>
#include <filesystem>

#include "Level.h"

FlexLog::LogstashFormatter::LogstashFormatter(const Options& options) :
    BaseStructuredFormatter(options),
    m_logstashOptions(options)
{
    if (m_options.hostname.empty())
        m_options.hostname = GetHostname();
}

std::string_view FlexLog::LogstashFormatter::GetContentType() const
{
    return "application/json";
}

std::unique_ptr<FlexLog::StructuredFormatter> FlexLog::LogstashFormatter::Clone() const
{
    return std::make_unique<LogstashFormatter>(m_logstashOptions);
}

void FlexLog::LogstashFormatter::EscapeString(std::ostream& os, std::string_view str) const
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

std::string FlexLog::LogstashFormatter::FormatMessageImpl(const Message& message) const
{
    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    ss << "{" << nl;

    // Standard Logstash/ELK fields
    if (m_options.includeTimestamp)
    {
        WriteIndent(ss, 1);
        ss << "\"@timestamp\": \"" << FormatTimestamp(message.timestamp) << "\"," << nl;
    }

    WriteIndent(ss, 1);
    ss << "\"@version\": \"1\"," << nl;

    if (m_options.includeMessage)
    {
        WriteIndent(ss, 1);
        ss << "\"message\": \"";
        EscapeString(ss, message.message);
        ss << "\"," << nl;
    }

    WriteIndent(ss, 1);
    ss << "\"type\": \"" << m_logstashOptions.logstashType << "\"," << nl;

    // Host information
    WriteIndent(ss, 1);
    ss << "\"host\": \"" << m_options.hostname << "\"," << nl;

    // Logger and level
    if (m_options.includeLogger)
    {
        WriteIndent(ss, 1);
        ss << "\"logger_name\": \"";
        EscapeString(ss, message.name);
        ss << "\"," << nl;
    }

    if (m_options.includeLevel)
    {
        WriteIndent(ss, 1);
        ss << "\"level\": \"" << LevelToString(message.level) << "\"," << nl;
        WriteIndent(ss, 1);
        ss << "\"level_value\": " << static_cast<int>(message.level) << "," << nl;
    }

    // Application information
    WriteIndent(ss, 1);
    ss << "\"application\": \"" << m_options.applicationName << "\"," << nl;
    WriteIndent(ss, 1);
    ss << "\"environment\": \"" << m_options.environment << "\"," << nl;

    // Tags if enabled
    if (m_logstashOptions.includeLogstashTags && !m_options.tags.empty())
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

    // Process and thread information
    if (m_options.includeProcessInfo)
    {
        WriteIndent(ss, 1);
        ss << "\"process\": {" << nl;
        WriteIndent(ss, 2);
        ss << "\"pid\": " << GetProcessId() << "," << nl;
        WriteIndent(ss, 2);
        ss << "\"name\": \"" << GetProcessName() << "\"" << nl;
        WriteIndent(ss, 1);
        ss << "}," << nl;
    }

    if (m_options.includeThreadId)
    {
        WriteIndent(ss, 1);
        ss << "\"thread_id\": \"" << GetThreadId() << "\"," << nl;
    }

    // Structured data
    if (!message.structuredData.IsEmpty())
    {
        WriteIndent(ss, 1);
        ss << "\"structured_data\": " << FormatStructuredDataImpl(message.structuredData) << "," << nl;
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

    // Close the JSON object
    ss << "}";

    return ss.str();
}

std::string FlexLog::LogstashFormatter::FormatStructuredDataImpl(const StructuredData& data) const
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

                for (size_t j = 0; j < arg.size(); ++j)
                {
                    WriteIndent(ss, 3);

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

                WriteIndent(ss, 2);
                ss << "]";
            }
        }, value);

        if (i < keys.size() - 1)
            ss << ",";

        ss << nl;
    }

    WriteIndent(ss, 1);
    ss << "}";

    return ss.str();
}
