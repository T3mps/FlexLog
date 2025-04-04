#include "PatternFormatter.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace FlexLog::Internal
{
    // StringBuilder with pre-allocated buffer
    class StackStringBuilder
    {
    public:
        StackStringBuilder(char* buffer, size_t capacity) : m_buffer(buffer), m_capacity(capacity), m_size(0) {}

        void Append(std::string_view str)
        {
            const size_t remaining = m_capacity - m_size;
            const size_t copySize = std::min(remaining, str.size());
            std::memcpy(m_buffer + m_size, str.data(), copySize);
            m_size += copySize;
        }

        std::string_view View() const { return std::string_view(m_buffer, m_size); }
        size_t Size() const { return m_size; }

    private:
        char* m_buffer;
        size_t m_capacity;
        size_t m_size;
    };

    struct TimestampFormatter { static void FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view timeFormat); };
    struct LevelFormatter     { static void FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view timeFormat); };
    struct NameFormatter      { static void FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view timeFormat); };
    struct MessageFormatter   { static void FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view timeFormat); };
    struct SourceFormatter    { static void FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view timeFormat); };
    struct FunctionFormatter  { static void FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view timeFormat); };
    struct LineFormatter      { static void FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view timeFormat); };

    void TimestampFormatter::FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view timeFormat)
    {
        auto time = std::chrono::system_clock::to_time_t(msg.timestamp);

        std::tm tm_buf{};
#if defined(FLOG_PLATFORM_WINDOWS)
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif

        char buffer[64];
        const auto len = std::strftime(buffer, sizeof(buffer), timeFormat.data(), &tm_buf);
        builder.Append(std::string_view(buffer, len));
    }

    void LevelFormatter::FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view)
    {
        std::string_view levelStr = LevelToString(static_cast<Level>(msg.level));
        builder.Append(levelStr);
    }

    void NameFormatter::FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view)
    {
        builder.Append(msg.name);
    }

    void MessageFormatter::FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view)
    {
        builder.Append(msg.message);
    }

    void SourceFormatter::FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view)
    {
        std::string filename = std::filesystem::path(msg.sourceLocation.file_name()).filename().string();
        builder.Append(filename);
    }

    void FunctionFormatter::FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view)
    {
        builder.Append(msg.sourceLocation.function_name());
    }

    void LineFormatter::FormatTo(StackStringBuilder& builder, const Message& msg, std::string_view)
    {
        char buffer[16];
        int len = snprintf(buffer, sizeof(buffer), "%u", msg.sourceLocation.line());
        builder.Append(std::string_view(buffer, len));
    }
}

FlexLog::TokenType FlexLog::Token::GetType(std::string_view token)
{
    static const std::unordered_map<std::string_view, TokenType> tokenTypes =
    {
        { TIMESTAMP, TokenType::Timestamp },
        { LEVEL,     TokenType::Level },
        { NAME,      TokenType::Name },
        { MESSAGE,   TokenType::Message },
        { SOURCE,    TokenType::Source },
        { FUNCTION,  TokenType::Function },
        { LINE,      TokenType::Line }
    };

    auto it = tokenTypes.find(token);
    if (it != tokenTypes.end())
        return it->second;

    if (token.size() > 9 && token.substr(0, 8) == "{custom:" && token.back() == '}')
        return TokenType::Custom;

    return TokenType::Literal;
}

FlexLog::PatternFormatter::PatternFormatter(std::string_view pattern) : m_pattern(pattern)
{
    m_formatInfo.pattern = std::string(pattern);

    if (pattern == FormatPatterns::DEFAULT)
        m_formatFunc = &FormatWithDefaultPattern;
    else if (pattern == FormatPatterns::SIMPLE)
        m_formatFunc = &FormatWithSimplePattern;
    else if (pattern == FormatPatterns::DETAILED)
        m_formatFunc = &FormatWithDetailedPattern;
    else
    {
        // For custom patterns or runtime-added patterns, parse at runtime
        m_formatFunc = nullptr;
        ParsePattern();
    }
}

std::string FlexLog::PatternFormatter::FormatMessage(const Message& msg) const
{
    // If we have a compile-time formatter, use it
    if (m_formatFunc)
        return m_formatFunc(msg, m_formatInfo.timeFormat);

    // Otherwise, use runtime formatting
    std::string result;
    result.reserve(256); // Reserve a reasonable size

    for (const auto& fragment : m_fragments)
    {
        if (fragment.type == TokenType::Literal)
        {
            result += fragment.data;
        }
        else if (fragment.type == TokenType::Custom && fragment.customFormatter)
        {
            result += fragment.customFormatter(msg);
        }
        else
        {
            result += FormatToken(fragment.type, fragment.data, msg);
        }
    }

    return result;
}

void FlexLog::PatternFormatter::SetPattern(std::string_view pattern)
{
    m_pattern = std::string(pattern);
    m_formatInfo.pattern = m_pattern;

    // Try to use compile-time formatters when possible
    if (pattern == FormatPatterns::DEFAULT)
        m_formatFunc = &FormatWithDefaultPattern;
    else if (pattern == FormatPatterns::SIMPLE)
        m_formatFunc = &FormatWithSimplePattern;
    else if (pattern == FormatPatterns::DETAILED)
        m_formatFunc = &FormatWithDetailedPattern;
    else
    {
        // For custom patterns, we'll need to parse at runtime
        m_formatFunc = nullptr;
        ParsePattern();
    }
}

void FlexLog::PatternFormatter::RegisterCustomFormatter(std::string_view token, CustomFormatter formatter)
{
    if (formatter)
    {
        std::string tokenStr(token);
        m_customFormatters[tokenStr] = std::move(formatter);

        // Need to revert to runtime formatting
        m_formatFunc = nullptr;

        // Re-parse the pattern to update any existing custom tokens
        ParsePattern();
    }
}

void FlexLog::PatternFormatter::ParsePattern()
{
    m_fragments.clear();
    m_fragments.reserve(m_formatInfo.fragmentCapacity);

    size_t pos = 0;
    size_t lastPos = 0;

    while ((pos = m_pattern.find('{', lastPos)) != std::string::npos)
    {
        // Add literal text before the token
        if (pos > lastPos)
        {
            Fragment literal;
            literal.type = TokenType::Literal;
            literal.data = m_pattern.substr(lastPos, pos - lastPos);
            m_fragments.push_back(std::move(literal));
        }

        size_t endPos = m_pattern.find('}', pos);
        if (endPos == std::string::npos)
            break;

        std::string tokenStr = m_pattern.substr(pos, endPos - pos + 1);
        TokenType type = Token::GetType(tokenStr);

        Fragment token;
        token.type = type;

        if (type == TokenType::Custom)
        {
            // Extract the custom token name (everything between {custom: and })
            std::string customTokenName = tokenStr.substr(8, tokenStr.size() - 9);
            token.data = customTokenName;

            // Find the custom formatter if registered
            auto it = m_customFormatters.find(customTokenName);
            if (it != m_customFormatters.end())
                token.customFormatter = it->second;
        }
        else if (type == TokenType::Literal)
        {
            // If we didn't recognize the token, treat it as literal text
            token.data = tokenStr;
        }

        m_fragments.push_back(std::move(token));
        lastPos = endPos + 1;
    }

    // Add any remaining literal text
    if (lastPos < m_pattern.length())
    {
        Fragment literal;
        literal.type = TokenType::Literal;
        literal.data = m_pattern.substr(lastPos);
        m_fragments.push_back(std::move(literal));
    }
}

std::string FlexLog::PatternFormatter::FormatToken(TokenType type, const std::string& tokenData, const Message& msg) const
{
    switch (type)
    {
    case TokenType::Timestamp:
    {
        auto time = std::chrono::system_clock::to_time_t(msg.timestamp);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), m_formatInfo.timeFormat.data());
        return ss.str();
    }
    case TokenType::Level:      return std::string(LevelToString(static_cast<Level>(msg.level)));
    case TokenType::Name:       return std::string(msg.name);
    case TokenType::Message:    return std::string(msg.message);
    case TokenType::Source:     return std::filesystem::path(msg.sourceLocation.file_name()).filename().string();
    case TokenType::Function:   return msg.sourceLocation.function_name();
    case TokenType::Line:       return std::to_string(msg.sourceLocation.line());
    case TokenType::Custom:
    {
        auto it = m_customFormatters.find(tokenData);
        if (it != m_customFormatters.end() && it->second)
            return it->second(msg);
        return "[unknown custom token: " + tokenData + "]";
    }
    case TokenType::Literal: 
    default:                    return tokenData;
    }
}

std::string FlexLog::PatternFormatter::FormatWithDefaultPattern(const Message& msg, std::string_view timeFormat)
{
    // Format: "[{timestamp}] [{level}] [{name} @ {function}] - {message}"
    constexpr size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    Internal::StackStringBuilder builder(buffer, BUFFER_SIZE);

    builder.Append("[");
    Internal::TimestampFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append("] [");
    Internal::LevelFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append("] [");
    Internal::NameFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append(" @ ");
    Internal::FunctionFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append("] - ");
    Internal::MessageFormatter::FormatTo(builder, msg, timeFormat);

    return std::string(builder.View());
}

std::string FlexLog::PatternFormatter::FormatWithSimplePattern(const Message& msg, std::string_view timeFormat)
{
    // Format: "[{timestamp}] [{level}] [{name}] - {message}"
    constexpr size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    Internal::StackStringBuilder builder(buffer, BUFFER_SIZE);

    builder.Append("[");
    Internal::TimestampFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append("] [");
    Internal::LevelFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append("] [");
    Internal::NameFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append("] - ");
    Internal::MessageFormatter::FormatTo(builder, msg, timeFormat);

    return std::string(builder.View());
}

std::string FlexLog::PatternFormatter::FormatWithDetailedPattern(const Message& msg, std::string_view timeFormat)
{
    // Format: "[{timestamp}] [{level}] [{name} @ {function}] [{source}:{line}] - {message}"
    constexpr size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    Internal::StackStringBuilder builder(buffer, BUFFER_SIZE);

    builder.Append("[");
    Internal::TimestampFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append("] [");
    Internal::LevelFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append("] [");
    Internal::NameFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append(" @ ");
    Internal::FunctionFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append("] [");
    Internal::SourceFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append(":");
    Internal::LineFormatter::FormatTo(builder, msg, timeFormat);
    builder.Append("] - ");
    Internal::MessageFormatter::FormatTo(builder, msg, timeFormat);

    return std::string(builder.View());
}

std::string FlexLog::DefaultFormatter::Format(const Message& msg, std::string_view timeFormat) const
{
    return PatternFormatter::FormatWithDefaultPattern(msg, timeFormat);
}

std::string FlexLog::SimpleFormatter::Format(const Message& msg, std::string_view timeFormat) const
{
    return PatternFormatter::FormatWithSimplePattern(msg, timeFormat);
}

std::string FlexLog::DetailedFormatter::Format(const Message& msg, std::string_view timeFormat) const
{
    return PatternFormatter::FormatWithDetailedPattern(msg, timeFormat);
}
