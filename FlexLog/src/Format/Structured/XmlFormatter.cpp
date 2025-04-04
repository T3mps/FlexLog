#include "XmlFormatter.h"

#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>

#include "Level.h"
#include "Platform.h"

FlexLog::XmlFormatter::XmlFormatter(const Options& options) :
    BaseStructuredFormatter(options),
    m_xmlOptions(options)
{
    if (m_options.hostname.empty())
        m_options.hostname = GetHostname();
}

std::string_view FlexLog::XmlFormatter::GetContentType() const
{
    return "application/xml";
}

std::unique_ptr<FlexLog::StructuredFormatter> FlexLog::XmlFormatter::Clone() const
{
    return std::make_unique<XmlFormatter>(m_xmlOptions);
}

void FlexLog::XmlFormatter::EscapeString(std::ostream& os, std::string_view str) const
{
    for (char c : str)
    {
        switch (c)
        {
            case '<':  os << "&lt;"; break;
            case '>':  os << "&gt;"; break;
            case '&':  os << "&amp;"; break;
            case '\'': os << "&apos;"; break;
            case '"':  os << "&quot;"; break;
            default:
                if (static_cast<unsigned char>(c) < 32 && c != '\t' && c != '\n' && c != '\r')
                {
                    // Control characters (except tabs, newlines and carriage returns)
                    os << "&#" << static_cast<int>(c) << ";";
                }
                else
                {
                    os << c;
                }
                break;
            }
    }
}

std::string FlexLog::XmlFormatter::FormatMessageImpl(const Message& message) const
{
    return FormatXml(message);
}

std::string FlexLog::XmlFormatter::FormatStructuredDataImpl(const StructuredData& data) const
{
    if (data.IsEmpty())
        return "<data/>";

    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    ss << "<data>" << nl;

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

        if (m_xmlOptions.useAttributes && 
            (std::holds_alternative<std::string>(value) ||
                std::holds_alternative<int64_t>(value)  ||
                std::holds_alternative<uint64_t>(value) ||
                std::holds_alternative<double>(value)   ||
                std::holds_alternative<bool>(value)))
        {
            WriteIndent(ss, 1);
            ss << "<" << m_xmlOptions.fieldElementName << " name=\"" << key << "\" value=\"";

            std::visit([&](auto&& arg)
            {
                using T = std::decay_t<decltype(arg)>;

                if constexpr (std::is_same_v<T, std::string>)
                {
                    EscapeString(ss, arg);
                }
                else if constexpr (std::is_same_v<T, int64_t> || 
                    std::is_same_v<T, uint64_t> ||
                    std::is_same_v<T, double>)
                {
                    ss << arg;
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    ss << (arg ? "true" : "false");
                }
            }, value);

            ss << "\"/>" << nl;
        }
        else
        {
            WriteIndent(ss, 1);
            ss << "<" << m_xmlOptions.fieldElementName << " name=\"" << key << "\">" << nl;

            WriteValue(ss, value, 2);

            WriteIndent(ss, 1);
            ss << "</" << m_xmlOptions.fieldElementName << ">" << nl;
        }
    }

    ss << "</data>";

    return ss.str();
}

std::string FlexLog::XmlFormatter::FormatXml(const Message& message) const
{
    std::stringstream ss;
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    // XML declaration
    if (m_xmlOptions.includeXmlDeclaration)
        ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << nl;

    // Root element
    ss << "<" << m_xmlOptions.rootElementName;

    // Add attributes if using attribute style
    if (m_xmlOptions.useAttributes)
    {
        if (m_options.includeTimestamp)
            ss << " timestamp=\"" << FormatTimestamp(message.timestamp) << "\"";

        if (m_options.includeLevel)
        {
            ss << " level=\"" << LevelToString(message.level) << "\"";
            ss << " level_value=\"" << static_cast<int>(message.level) << "\"";
        }

        ss << " application=\"" << m_options.applicationName << "\"";
        ss << " environment=\"" << m_options.environment << "\"";
        ss << " host=\"" << m_options.hostname << "\"";
    }

    ss << ">" << nl;

    // Elements for each field
    if (!m_xmlOptions.useAttributes || !m_options.includeTimestamp)
    {
        WriteIndent(ss, 1);
        ss << "<timestamp>" << FormatTimestamp(message.timestamp) << "</timestamp>" << nl;
    }

    if (m_options.includeMessage)
    {
        WriteIndent(ss, 1);
        ss << "<message>";

        if (m_xmlOptions.useCDATA)
            ss << WrapInCDATA(message.message);
        else
            EscapeString(ss, message.message);

        ss << "</message>" << nl;
    }

    if (m_options.includeLogger)
    {
        WriteIndent(ss, 1);
        ss << "<logger>";
        EscapeString(ss, message.name);
        ss << "</logger>" << nl;
    }

    if (!m_xmlOptions.useAttributes || !m_options.includeLevel)
    {
        WriteIndent(ss, 1);
        ss << "<level>" << LevelToString(message.level) << "</level>" << nl;
        WriteIndent(ss, 1);
        ss << "<level_value>" << static_cast<int>(message.level) << "</level_value>" << nl;
    }

    if (!m_xmlOptions.useAttributes)
    {
        WriteIndent(ss, 1);
        ss << "<application>" << m_options.applicationName << "</application>" << nl;
        WriteIndent(ss, 1);
        ss << "<environment>" << m_options.environment << "</environment>" << nl;
        WriteIndent(ss, 1);
        ss << "<host>" << m_options.hostname << "</host>" << nl;
    }

    // Source location
    if (m_options.includeSourceLocation)
    {
        WriteIndent(ss, 1);
        ss << "<location>" << nl;

        WriteIndent(ss, 2);
        ss << "<file>" << std::filesystem::path(message.sourceLocation.file_name()).filename().string() << "</file>" << nl;

        WriteIndent(ss, 2);
        ss << "<line>" << message.sourceLocation.line() << "</line>" << nl;

        WriteIndent(ss, 2);
        ss << "<function>" << message.sourceLocation.function_name() << "</function>" << nl;

        WriteIndent(ss, 1);
        ss << "</location>" << nl;
    }

    // Process info
    if (m_options.includeProcessInfo)
    {
        WriteIndent(ss, 1);
        ss << "<process>" << nl;

        WriteIndent(ss, 2);
        ss << "<id>" << GetProcessId() << "</id>" << nl;

        WriteIndent(ss, 2);
        ss << "<name>" << GetProcessName() << "</name>" << nl;

        WriteIndent(ss, 1);
        ss << "</process>" << nl;
    }

    // Thread ID
    if (m_options.includeThreadId)
    {
        WriteIndent(ss, 1);
        ss << "<thread_id>" << GetThreadId() << "</thread_id>" << nl;
    }

    // Tags
    if (!m_options.tags.empty())
    {
        WriteIndent(ss, 1);
        ss << "<tags>" << nl;

        for (const auto& tag : m_options.tags)
        {
            WriteIndent(ss, 2);
            ss << "<tag>" << tag << "</tag>" << nl;
        }

        WriteIndent(ss, 1);
        ss << "</tags>" << nl;
    }

    // Structured data
    if (!message.structuredData.IsEmpty())
    {
        WriteIndent(ss, 1);
        ss << FormatStructuredDataImpl(message.structuredData) << nl;
    }

    // User data
    if (!m_options.userData.empty())
    {
        WriteIndent(ss, 1);
        ss << "<user_data>" << nl;

        for (const auto& [key, value] : m_options.userData)
        {
            WriteIndent(ss, 2);
            ss << "<" << key << ">" << value << "</" << key << ">" << nl;
        }

        WriteIndent(ss, 1);
        ss << "</user_data>" << nl;
    }

    // Close root element
    ss << "</" << m_xmlOptions.rootElementName << ">";

    return ss.str();
}

void FlexLog::XmlFormatter::WriteValue(std::ostream& os, const StructuredData::FieldValue& value, int indentLevel) const
{
    const bool pretty = m_options.prettyPrint;
    const std::string nl = pretty ? "\n" : "";

    std::visit([&](auto&& arg)
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::nullptr_t>)
        {
            WriteIndent(os, indentLevel);
            os << "<null/>" << nl;
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            WriteIndent(os, indentLevel);
            os << "<string>";

            if (m_xmlOptions.useCDATA)
                os << WrapInCDATA(arg);
            else
                EscapeString(os, arg);

            os << "</string>" << nl;
        }
        else if constexpr (std::is_same_v<T, int64_t>)
        {
            WriteIndent(os, indentLevel);
            os << "<int>" << arg << "</int>" << nl;
        }
        else if constexpr (std::is_same_v<T, uint64_t>)
        {
            WriteIndent(os, indentLevel);
            os << "<uint>" << arg << "</uint>" << nl;
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            WriteIndent(os, indentLevel);
            os << "<double>" << std::fixed << arg << "</double>" << nl;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            WriteIndent(os, indentLevel);
            os << "<bool>" << (arg ? "true" : "false") << "</bool>" << nl;
        }
        else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>)
        {
            WriteIndent(os, indentLevel);
            os << "<datetime>" << FormatTimestamp(arg) << "</datetime>" << nl;
        }
        else if constexpr (std::is_same_v<T, std::vector<std::string>>)
        {
            WriteIndent(os, indentLevel);
            os << "<array type=\"string\">" << nl;

            for (const auto& item : arg)
            {
                WriteIndent(os, indentLevel + 1);
                os << "<item>";

                if (m_xmlOptions.useCDATA)
                    os << WrapInCDATA(item);
                else
                    EscapeString(os, item);

                os << "</item>" << nl;
            }

            WriteIndent(os, indentLevel);
            os << "</array>" << nl;
        }
        else if constexpr (std::is_same_v<T, std::vector<int64_t>>)
        {
            WriteIndent(os, indentLevel);
            os << "<array type=\"int\">" << nl;

            for (const auto& item : arg)
            {
                WriteIndent(os, indentLevel + 1);
                os << "<item>" << item << "</item>" << nl;
            }

            WriteIndent(os, indentLevel);
            os << "</array>" << nl;
        }
        else if constexpr (std::is_same_v<T, std::vector<double>>)
        {
            WriteIndent(os, indentLevel);
            os << "<array type=\"double\">" << nl;

            for (const auto& item : arg)
            {
                WriteIndent(os, indentLevel + 1);
                os << "<item>" << std::fixed << item << "</item>" << nl;
            }

            WriteIndent(os, indentLevel);
            os << "</array>" << nl;
        }
        else if constexpr (std::is_same_v<T, std::vector<bool>>)
        {
            WriteIndent(os, indentLevel);
            os << "<array type=\"bool\">" << nl;

            for (const auto& item : arg)
            {
                WriteIndent(os, indentLevel + 1);
                os << "<item>" << (item ? "true" : "false") << "</item>" << nl;
            }

            WriteIndent(os, indentLevel);
            os << "</array>" << nl;
        }
    }, value);
}

std::string FlexLog::XmlFormatter::WrapInCDATA(std::string_view text) const
{
    std::string result = "<![CDATA[";
    result += text;

    // CDATA sections cannot contain the sequence "]]>", so we need to split it
    // if it appears in the text
    size_t pos = 0;
    while ((pos = result.find("]]>", pos)) != std::string::npos)
    {
        // Replace "]]>" with "]]]]><![CDATA[>"
        result.replace(pos, 3, "]]]]><![CDATA[>");
        pos += 13; // Move past the replacement
    }

    result += "]]>";
    return result;
}

std::string FlexLog::XmlFormatter::EscapeXml(std::string_view text) const
{
    std::stringstream ss;
    EscapeString(ss, text);
    return ss.str();
}
