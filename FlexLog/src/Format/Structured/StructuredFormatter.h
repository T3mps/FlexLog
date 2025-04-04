#pragma once

#include <string>
#include <string_view>
#include <memory>

#include "Message.h"
#include "StructuredData.h"

namespace FlexLog
{
    class StructuredFormatter
    {
    public:
        virtual ~StructuredFormatter() = default;

        // Format the entire message including structured data
        virtual std::string FormatMessage(const Message& message) const = 0;

        // Format only the structured data portion
        virtual std::string FormatStructuredData(const StructuredData& data) const = 0;

        // Get the content type for HTTP/network transmissions
        virtual std::string_view GetContentType() const = 0;

        // Clone this formatter (for copying configured formatters)
        virtual std::unique_ptr<StructuredFormatter> Clone() const = 0;
    };
}
