#include "ElasticsearchFormatter.h"

#include <sstream>
#include <iomanip>
#include <filesystem>
#include <thread>

#include "Level.h"
#include "Platform.h"

FlexLog::ElasticsearchFormatter::ElasticsearchFormatter(const Options& options) :
    BaseStructuredFormatter(options),
    m_elasticOptions(options)
{
    if (m_options.hostname.empty())
        m_options.hostname = GetHostname();
}

std::string_view FlexLog::ElasticsearchFormatter::GetContentType() const
{
    return "application/json";
}

std::unique_ptr<FlexLog::StructuredFormatter> FlexLog::ElasticsearchFormatter::Clone() const
{
    return std::make_unique<ElasticsearchFormatter>(m_elasticOptions);
}

void FlexLog::ElasticsearchFormatter::EscapeString(std::ostream& os, std::string_view str) const
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

std::string FlexLog::ElasticsearchFormatter::FormatMessageImpl(const Message& message) const
{
    if (m_elasticOptions.useBulkFormat)
        return FormatBulkLine(message);

    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    ss << "{" << nl;

    // Timestamp (required by Elasticsearch)
    if (m_options.includeTimestamp)
    {
        WriteIndent(ss, 1);
        ss << "\"@timestamp\": \"" << FormatTimestamp(message.timestamp) << "\"," << nl;
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
        ss << "\"logger_name\": \"";
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

    // Service information
    WriteIndent(ss, 1);
    ss << "\"application\": \"" << m_options.applicationName << "\"," << nl;
    WriteIndent(ss, 1);
    ss << "\"environment\": \"" << m_options.environment << "\"," << nl;

    if (!m_options.serviceName.empty())
    {
        WriteIndent(ss, 1);
        ss << "\"service\": {" << nl;
        WriteIndent(ss, 2);
        ss << "\"name\": \"" << m_options.serviceName << "\"," << nl;
        WriteIndent(ss, 2);
        ss << "\"version\": \"" << m_options.serviceVersion << "\"" << nl;
        WriteIndent(ss, 1);
        ss << "}," << nl;
    }

    // Host information
    WriteIndent(ss, 1);
    ss << "\"host\": \"" << m_options.hostname << "\"," << nl;

    // Source location
    if (m_options.includeSourceLocation)
    {
        WriteIndent(ss, 1);
        ss << "\"log\": {" << nl;
        WriteIndent(ss, 2);
        ss << "\"origin\": {" << nl;
        WriteIndent(ss, 3);
        ss << "\"file\": \"" 
            << std::filesystem::path(message.sourceLocation.file_name()).filename().string() 
            << "\"," << nl;
        WriteIndent(ss, 3);
        ss << "\"function\": \"" << message.sourceLocation.function_name() << "\"," << nl;
        WriteIndent(ss, 3);
        ss << "\"line\": " << message.sourceLocation.line() << nl;
        WriteIndent(ss, 2);
        ss << "}" << nl;
        WriteIndent(ss, 1);
        ss << "}," << nl;
    }

    // Process and thread info
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
        ss << "\"thread\": {" << nl;
        WriteIndent(ss, 2);
        ss << "\"id\": \"" << GetThreadId() << "\"" << nl;
        WriteIndent(ss, 1);
        ss << "}," << nl;
    }

    // Tags if any
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

    // Close the JSON object
    ss << "}";

    return ss.str();
}

std::string FlexLog::ElasticsearchFormatter::FormatStructuredDataImpl(const StructuredData& data) const
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
    {
        std::sort(keys.begin(), keys.end());
    }

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
                std::is_same_v<T, std::vector<int64_t>> ||
                std::is_same_v<T, std::vector<double>> ||
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

std::string FlexLog::ElasticsearchFormatter::GenerateIndexName() const
{
    std::string result = m_elasticOptions.indexNameTemplate;

    // Replace {application} placeholder
    size_t appPos = result.find("{application}");
    if (appPos != std::string::npos)
        result.replace(appPos, 13, m_options.applicationName);

    // Replace {date} placeholder
    size_t datePos = result.find("{date}");
    if (datePos != std::string::npos)
    {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y.%m.%d");
        result.replace(datePos, 6, ss.str());
    }

    return result;
}

std::string FlexLog::ElasticsearchFormatter::FormatBulkLine(const Message& message) const
{
    // The Elasticsearch bulk API requires two lines per document:
    // 1. Action and metadata
    // 2. Document source

    std::stringstream ss;

    // 1. Action line
    ss << "{\"index\":{\"_index\":\"" << GenerateIndexName() << "\"";

    if (!m_elasticOptions.docType.empty())
    {
        ss << ",\"_type\":\"" << m_elasticOptions.docType << "\"";
    }

    ss << "}}\n";

    // 2. Document source (without pretty printing)
    ss << "{";

    // Timestamp (required by Elasticsearch)
    ss << "\"@timestamp\":\"" << FormatTimestamp(message.timestamp) << "\",";

    // Message content
    ss << "\"message\":\"";
    EscapeString(ss, message.message);
    ss << "\",";

    // Logger name
    ss << "\"logger_name\":\"";
    EscapeString(ss, message.name);
    ss << "\",";

    // Log level
    ss << "\"level\":\"" << LevelToString(message.level) << "\",";
    ss << "\"level_value\":" << static_cast<int>(message.level) << ",";

    // Other fields
    ss << "\"application\":\"" << m_options.applicationName << "\",";
    ss << "\"environment\":\"" << m_options.environment << "\",";
    ss << "\"host\":\"" << m_options.hostname << "\"";

    // Add structured data if present
    if (!message.structuredData.IsEmpty())
    {
        ss << ",\"data\":";

        // For bulk format, use compact JSON without indentation
        CommonFormatterOptions tmpOptions = m_options;
        tmpOptions.prettyPrint = false;

        std::stringstream dataStr;
        for (const auto& [key, value] : message.structuredData.GetFields())
        {
            if (dataStr.tellp() > 0)
                dataStr << ",";

            dataStr << "\"" << key << "\":";

            std::visit([&](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, std::nullptr_t>)
                {
                    dataStr << "null";
                }
                else if constexpr (std::is_same_v<T, std::string>)
                {
                    dataStr << "\"";
                    EscapeString(dataStr, arg);
                    dataStr << "\"";
                }
                else if constexpr (std::is_same_v<T, int64_t> || 
                    std::is_same_v<T, uint64_t>)
                {
                    dataStr << arg;
                }
                else if constexpr (std::is_same_v<T, double>)
                {
                    dataStr << std::fixed << arg;
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    dataStr << (arg ? "true" : "false");
                }
                else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>)
                {
                    dataStr << "\"" << FormatTimestamp(arg) << "\"";
                }
                else if constexpr (std::is_same_v<T, std::vector<std::string>> || 
                    std::is_same_v<T, std::vector<int64_t>>                    ||
                    std::is_same_v<T, std::vector<double>>                     ||
                    std::is_same_v<T, std::vector<bool>>)
                {
                    dataStr << "[";

                    for (size_t i = 0; i < arg.size(); ++i)
                    {
                        if (i > 0)
                            dataStr << ",";

                        if constexpr (std::is_same_v<T, std::vector<std::string>>)
                        {
                            dataStr << "\"";
                            EscapeString(dataStr, arg[i]);
                            dataStr << "\"";
                        }
                        else if constexpr (std::is_same_v<T, std::vector<bool>>)
                        {
                            dataStr << (arg[i] ? "true" : "false");
                        }
                        else
                        {
                            dataStr << arg[i];
                        }
                    }

                    dataStr << "]";
                }
            }, value);
        }

        ss << "{" << dataStr.str() << "}";
    }

    // Close the document
    ss << "}\n";

    return ss.str();
}
