#pragma once

// StructuredData.h
#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <chrono>

namespace FlexLog
{
    class StructuredData;

   class StructuredData
    {
    public:
        using FieldValue = std::variant
        <
            std::nullptr_t,
            std::string,
            int64_t,
            uint64_t,
            double,
            bool,
            std::chrono::system_clock::time_point,
            std::vector<std::string>,
            std::vector<int64_t>,
            std::vector<double>,
            std::vector<bool>
        >;

        StructuredData() = default;

        bool operator==(const StructuredData& other) const;
        bool operator!=(const StructuredData& other) const;

        StructuredData& Add(std::string_view key, std::nullptr_t);
        StructuredData& Add(std::string_view key, std::string_view value);
        StructuredData& Add(std::string_view key, const char* value);
        StructuredData& Add(std::string_view key, int64_t value);
        StructuredData& Add(std::string_view key, int32_t value);
        StructuredData& Add(std::string_view key, uint64_t value);
        StructuredData& Add(std::string_view key, uint32_t value);
        StructuredData& Add(std::string_view key, double value);
        StructuredData& Add(std::string_view key, float value);
        StructuredData& Add(std::string_view key, bool value);
        StructuredData& Add(std::string_view key, std::chrono::system_clock::time_point value);

        StructuredData& Add(std::string_view key, const std::vector<std::string>& values);
        StructuredData& Add(std::string_view key, const std::vector<int64_t>& values);
        StructuredData& Add(std::string_view key, const std::vector<double>& values);
        StructuredData& Add(std::string_view key, const std::vector<bool>& values);

        std::optional<FieldValue> Get(std::string_view key) const;

        bool HasField(std::string_view key) const;

        bool Remove(std::string_view key);

        void Clear();

        const std::unordered_map<std::string, FieldValue>& GetFields() const;

        StructuredData& Merge(const StructuredData& other);

        bool IsEmpty() const;

    private:
        std::unordered_map<std::string, FieldValue> m_fields;
    };
}
