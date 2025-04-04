#include "JsonFormatter.h"

#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>

#include "Level.h"
#include "Platform.h"

FlexLog::JsonFormatter::JsonFormatter(const Options& options) :
    BaseStructuredFormatter(options),
    m_jsonOptions(options)
{
    if (m_options.hostname.empty())
        m_options.hostname = GetHostname();
}

std::string_view FlexLog::JsonFormatter::GetContentType() const
{
    return "application/json";
}

std::unique_ptr<FlexLog::StructuredFormatter> FlexLog::JsonFormatter::Clone() const
{
    return std::make_unique<JsonFormatter>(m_jsonOptions);
}

void FlexLog::JsonFormatter::EscapeString(std::ostream& os, std::string_view str) const
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

std::string FlexLog::JsonFormatter::FormatMessageImpl(const Message& message) const
{
    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    ss << "{" << nl;

    // Timestamp
    if (m_options.includeTimestamp)
    {
        WriteIndent(ss, 1);
        ss << "\"timestamp\": \"" << FormatTimestamp(message.timestamp) << "\"," << nl;
    }

    // Message content
    if (m_options.includeMessage)
    {
        WriteIndent(ss, 1);
        ss << "\"message\": \"";
        EscapeString(ss, message.message);
        ss << "\"," << nl;
    }

    // Logger name
    if (m_options.includeLogger)
    {
        WriteIndent(ss, 1);
        ss << "\"logger\": \"";
        EscapeString(ss, message.name);
        ss << "\"," << nl;
    }

    // Log level
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
    WriteIndent(ss, 1);
    ss << "\"host\": \"" << m_options.hostname << "\"," << nl;

    // Source location
    if (m_options.includeSourceLocation)
    {
        if (m_jsonOptions.useFlatStructure)
        {
            WriteIndent(ss, 1);
            ss << "\"file\": \"" 
                << std::filesystem::path(message.sourceLocation.file_name()).filename().string() 
                << "\"," << nl;
            WriteIndent(ss, 1);
            ss << "\"line\": " << message.sourceLocation.line() << "," << nl;
            WriteIndent(ss, 1);
            ss << "\"function\": \"" << message.sourceLocation.function_name() << "\"," << nl;
        }
        else
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
    }

    // Process and thread info
    if (m_options.includeProcessInfo)
    {
        if (m_jsonOptions.useFlatStructure)
        {
            WriteIndent(ss, 1);
            ss << "\"process_id\": \"" << GetProcessId() << "\"," << nl;
            WriteIndent(ss, 1);
            ss << "\"process_name\": \"" << GetProcessName() << "\"," << nl;
        }
        else
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
    }

    if (m_options.includeThreadId)
    {
        WriteIndent(ss, 1);
        ss << "\"thread_id\": \"" << GetThreadId() << "\"," << nl;
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
        if (m_jsonOptions.useFlatStructure)
        {
            // Add fields directly to root
            const auto& fields = message.structuredData.GetFields();

            for (const auto& [key, value] : fields)
            {
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
                        ss << std::fixed << std::setprecision(m_jsonOptions.precision) << arg;
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        ss << (arg ? "true" : "false");
                    }
                    else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>)
                    {
                        ss << "\"" << FormatTimestampAsJson(arg) << "\"";
                    }
                    else if constexpr (std::is_same_v<T, std::vector<std::string>> || 
                        std::is_same_v<T, std::vector<int64_t>> ||
                        std::is_same_v<T, std::vector<double>> ||
                        std::is_same_v<T, std::vector<bool>>)
                    {
                        ss << "[";

                        for (size_t i = 0; i < arg.size(); ++i)
                        {
                            if (i > 0)
                                ss << ", ";

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
                            else if constexpr (std::is_same_v<T, std::vector<double>>)
                            {
                                ss << std::fixed << std::setprecision(m_jsonOptions.precision) << arg[i];
                            }
                            else
                            {
                                ss << arg[i];
                            }
                        }

                        ss << "]";
                    }
                }, value);

                ss << "," << nl;
            }
        }
        else
        {
            WriteIndent(ss, 1);
            ss << "\"data\": " << FormatStructuredDataImpl(message.structuredData) << "," << nl;
        }
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

    // Remove trailing comma if needed
    std::string result = ss.str();
    size_t lastCommaPos = result.find_last_of(',');
    size_t lastBracePos = result.find_last_of('}');

    if (lastCommaPos != std::string::npos && 
        lastBracePos != std::string::npos && 
        lastCommaPos > lastBracePos)
    {
        // There's a trailing comma to remove
        result.erase(lastCommaPos, 1);
    }

    // Close the JSON object
    if (result.back() != '}')
    {
        result += "}";
    }

    return result;
}

std::string FlexLog::JsonFormatter::FormatStructuredDataImpl(const StructuredData& data) const
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

        WriteValue(ss, value);

        if (i < keys.size() - 1)
            ss << ",";

        ss << nl;
    }

    ss << "}";

    return ss.str();
}

void FlexLog::JsonFormatter::WriteValue(std::ostream& os, const StructuredData::FieldValue& value) const
{
    std::visit([&](auto&& arg)
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::nullptr_t>)
        {
            os << "null";
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            os << "\"";
            EscapeString(os, arg);
            os << "\"";
        }
        else if constexpr (std::is_same_v<T, int64_t> || 
            std::is_same_v<T, uint64_t>)
        {
            os << arg;
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            os << std::fixed << std::setprecision(m_jsonOptions.precision) << arg;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            os << (arg ? "true" : "false");
        }
        else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>)
        {
            os << "\"" << FormatTimestampAsJson(arg) << "\"";
        }
        else if constexpr (std::is_same_v<T, std::vector<std::string>> || 
            std::is_same_v<T, std::vector<int64_t>> ||
            std::is_same_v<T, std::vector<double>> ||
            std::is_same_v<T, std::vector<bool>>)
        {
            const bool pretty = m_options.prettyPrint;
            const std::string nl = pretty ? "\n" : "";

            os << "[" << nl;

            for (size_t i = 0; i < arg.size(); ++i)
            {
                if (pretty)
                    os << std::string(2 * m_options.indentSize, ' ');

                if constexpr (std::is_same_v<T, std::vector<std::string>>)
                {
                    os << "\"";
                    EscapeString(os, arg[i]);
                    os << "\"";
                }
                else if constexpr (std::is_same_v<T, std::vector<bool>>)
                {
                    os << (arg[i] ? "true" : "false");
                }
                else if constexpr (std::is_same_v<T, std::vector<double>>)
                {
                    os << std::fixed << std::setprecision(m_jsonOptions.precision) << arg[i];
                }
                else
                {
                    os << arg[i];
                }

                if (i < arg.size() - 1)
                    os << ",";

                os << nl;
            }

            if (pretty)
                os << std::string(m_options.indentSize, ' ');

            os << "]";
        }
    }, value);
}

std::string FlexLog::JsonFormatter::FormatTimestampAsJson(const std::chrono::system_clock::time_point& timestamp) const
{
    if (m_jsonOptions.useIsoTimestamps)
    {
        return FormatTimestamp(timestamp);
    }
    else
    {
        // Format as milliseconds since epoch
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();

        std::stringstream ss;
        ss << ms;
        return ss.str();
    }
}
