#pragma once

#include "BaseStructuredFormatter.h"
#include <string>

namespace FlexLog
{
    class JsonFormatter : public BaseStructuredFormatter
    {
    public:
        struct Options : public CommonFormatterOptions
        {
            // JSON-specific options
            bool useFlatStructure = false;  // Whether to flatten nested structures
            bool useIsoTimestamps = true;   // Use ISO-8601 timestamps
            int precision = 6;              // Decimal precision for floating point numbers

            Options& SetFlatStructure(bool flat)
            {
                useFlatStructure = flat;
                return *this;
            }

            Options& SetTimestampFormat(bool iso)
            {
                useIsoTimestamps = iso;
                return *this;
            }

            Options& SetPrecision(int prec)
            {
                precision = prec;
                return *this;
            }
        };

        explicit JsonFormatter(const Options& options = Options());

        std::string_view GetContentType() const override;
        std::unique_ptr<StructuredFormatter> Clone() const override;

    protected:
        void EscapeString(std::ostream& os, std::string_view str) const override;
        std::string FormatMessageImpl(const Message& message) const override;
        std::string FormatStructuredDataImpl(const StructuredData& data) const override;

    private:
        void WriteValue(std::ostream& os, const StructuredData::FieldValue& value) const;
        std::string FormatTimestampAsJson(const std::chrono::system_clock::time_point& timestamp) const;

        Options m_jsonOptions;
    };
}
