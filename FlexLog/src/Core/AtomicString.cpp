#pragma once

#include <cstring>

#include "AtomicString.h"

FlexLog::AtomicString::AtomicString()
{
    m_buffer[0] = '\0';
}

FlexLog::AtomicString::AtomicString(std::string_view str)
{
    Store(str);
}

void FlexLog::AtomicString::Store(std::string_view str)
{
    size_t len = std::min(str.length(), MAX_LENGTH - 1);
    std::memcpy(m_buffer, str.data(), len);
    m_buffer[len] = '\0';
    m_length.store(len, std::memory_order_release);
}

std::string FlexLog::AtomicString::Load() const
{
    size_t len = m_length.load(std::memory_order_acquire);
    return std::string(m_buffer, len);
}

bool FlexLog::AtomicString::Compare(std::string_view str) const
{
    size_t len = m_length.load(std::memory_order_acquire);
    if (len != str.length())
        return false;
    return std::memcmp(m_buffer, str.data(), len) == 0;
}
