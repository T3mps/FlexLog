#include "StructuredData.h"

bool FlexLog::StructuredData::operator==(const StructuredData& other) const
{
    return m_fields == other.m_fields;
}

bool FlexLog::StructuredData::operator!=(const StructuredData& other) const
{
    return !(*this == other);
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, std::nullptr_t)
{
    m_fields[std::string(key)] = nullptr;
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, std::string_view value)
{
    m_fields[std::string(key)] = std::string(value);
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, const char* value)
{
    m_fields[std::string(key)] = std::string(value);
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, int64_t value)
{
    m_fields[std::string(key)] = value;
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, int32_t value)
{
    return Add(key, static_cast<int64_t>(value));
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, uint64_t value)
{
    m_fields[std::string(key)] = value;
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, uint32_t value)
{
    return Add(key, static_cast<uint64_t>(value));
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, double value)
{
    m_fields[std::string(key)] = value;
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, float value)
{
    return Add(key, static_cast<double>(value));
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, bool value)
{
    m_fields[std::string(key)] = value;
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, std::chrono::system_clock::time_point value)
{
    m_fields[std::string(key)] = value;
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, const std::vector<std::string>& values)
{
    m_fields[std::string(key)] = values;
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, const std::vector<int64_t>& values)
{
    m_fields[std::string(key)] = values;
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, const std::vector<double>& values)
{
    m_fields[std::string(key)] = values;
    return *this;
}

FlexLog::StructuredData& FlexLog::StructuredData::Add(std::string_view key, const std::vector<bool>& values)
{
    m_fields[std::string(key)] = values;
    return *this;
}

std::optional<FlexLog::StructuredData::FieldValue> FlexLog::StructuredData::Get(std::string_view key) const
{
    auto it = m_fields.find(std::string(key));
    if (it != m_fields.end())
        return it->second;
    return std::nullopt;
}

bool FlexLog::StructuredData::HasField(std::string_view key) const
{
    return m_fields.find(std::string(key)) != m_fields.end();
}

bool FlexLog::StructuredData::Remove(std::string_view key)
{
    return m_fields.erase(std::string(key)) > 0;
}

void FlexLog::StructuredData::Clear()
{
    m_fields.clear();
}

const std::unordered_map<std::string, FlexLog::StructuredData::FieldValue>& FlexLog::StructuredData::GetFields() const
{
    return m_fields;
}

FlexLog::StructuredData& FlexLog::StructuredData::Merge(const StructuredData& other)
{
    for (const auto& [key, value] : other.m_fields)
    {
        m_fields[key] = value;
    }
    return *this;
}

bool FlexLog::StructuredData::IsEmpty() const
{
    return m_fields.empty();
}
