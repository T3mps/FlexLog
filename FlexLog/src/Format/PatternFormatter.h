#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <functional>

#include "Common.h"
#include "Level.h"
#include "Message.h"

namespace FlexLog
{
    enum class TokenType : uint8_t
    {
        Literal,
        Timestamp,
        Level,
        Name,
        Message,
        Source,
        Function,
        Line,
        Custom
    };

    struct Token
    {
        static constexpr std::string_view TIMESTAMP = "{timestamp}";
        static constexpr std::string_view LEVEL = "{level}";
        static constexpr std::string_view NAME = "{name}";
        static constexpr std::string_view MESSAGE = "{message}";
        static constexpr std::string_view SOURCE = "{source}";
        static constexpr std::string_view FUNCTION = "{function}";
        static constexpr std::string_view LINE = "{line}";

        static TokenType GetType(std::string_view token);
    };

    struct FormatInfo
    {
        std::string pattern;
        std::string timeFormat = "%H:%M:%S";
        size_t fragmentCapacity = 32;
    };

    namespace FormatPatterns
    {
        inline constexpr std::string_view DEFAULT = "[{timestamp}] [{level}] [{name} @ {function}] - {message}";
        inline constexpr std::string_view SIMPLE = "[{timestamp}] [{level}] [{name}] - {message}";
        inline constexpr std::string_view DETAILED = "[{timestamp}] [{level}] [{name} @ {function}] [{source}:{line}] - {message}";
    }

    using CustomFormatter = std::function<std::string(const Message&)>;

    struct Fragment 
    {
        TokenType type;
        std::string data;
        CustomFormatter customFormatter;
    };

    class PatternFormatter
    {
    public:
        explicit PatternFormatter(std::string_view pattern = FormatPatterns::DEFAULT);

        std::string FormatMessage(const Message& message) const;

        const std::string& GetPattern() const { return m_pattern; }
        void SetPattern(std::string_view pattern);

        void SetTimeFormat(std::string_view timeFormat) { m_formatInfo.timeFormat = timeFormat; }

        const FormatInfo& GetFormatInfo() const { return m_formatInfo; }

        void RegisterCustomFormatter(std::string_view token, CustomFormatter formatter);

    private:
        void ParsePattern();
        std::string FormatToken(TokenType type, const std::string& tokenData, const Message& msg) const;

        using FormatFunction = std::string(*)(const Message&, std::string_view);
        static std::string FormatWithDefaultPattern(const Message& msg, std::string_view timeFormat);
        static std::string FormatWithSimplePattern(const Message& msg, std::string_view timeFormat);
        static std::string FormatWithDetailedPattern(const Message& msg, std::string_view timeFormat);

        std::string m_pattern;
        FormatInfo m_formatInfo;
        FormatFunction m_formatFunc = nullptr;
        std::vector<Fragment> m_fragments;
        std::unordered_map<std::string, CustomFormatter> m_customFormatters;

        friend class DefaultFormatter;
        friend class SimpleFormatter;
        friend class DetailedFormatter;
    };

    class DefaultFormatter
    {
    public:
        std::string Format(const Message& msg, std::string_view timeFormat = "%H:%M:%S") const;
    };

    class SimpleFormatter
    {
    public:
        std::string Format(const Message& msg, std::string_view timeFormat = "%H:%M:%S") const;
    };

    class DetailedFormatter
    {
    public:
        std::string Format(const Message& msg, std::string_view timeFormat = "%H:%M:%S") const;
    };
}
