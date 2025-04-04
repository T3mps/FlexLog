#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace FlexLog
{
    enum class Level : uint8_t
    {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
        Off
    };

    static std::array<const std::string, 7> LEVEL_STRINGS =
    {
        "TRACE",
        "DEBUG",
        "INFO",
        "WARN",
        "ERROR",
        "FATAL",
        "OFF"
    };

    constexpr std::string_view LevelToString(const Level level) { return LEVEL_STRINGS[static_cast<size_t>(level)]; }

    inline constexpr bool operator>=(Level lhs, Level rhs)  { return static_cast<uint8_t>(lhs) >= static_cast<uint8_t>(rhs); }
    inline constexpr bool operator<=(Level lhs, Level rhs)  { return static_cast<uint8_t>(lhs) <= static_cast<uint8_t>(rhs); }
    inline constexpr bool operator>(Level lhs, Level rhs)   { return static_cast<uint8_t>(lhs) > static_cast<uint8_t>(rhs); }
    inline constexpr bool operator<(Level lhs, Level rhs)   { return static_cast<uint8_t>(lhs) < static_cast<uint8_t>(rhs); }
    inline constexpr bool operator==(Level lhs, Level rhs)  { return static_cast<uint8_t>(lhs) == static_cast<uint8_t>(rhs); }
    inline constexpr bool operator!=(Level lhs, Level rhs)  { return static_cast<uint8_t>(lhs) != static_cast<uint8_t>(rhs); }
}
