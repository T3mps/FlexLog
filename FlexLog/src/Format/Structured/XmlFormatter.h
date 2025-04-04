#pragma once

#include "BaseStructuredFormatter.h"
#include <string>

namespace FlexLog
{
    class XmlFormatter : public BaseStructuredFormatter
    {
    public:
        struct Options : public CommonFormatterOptions
        {
            // XML-specific options
            bool useAttributes = false;  // Whether to use attributes for simple values
            std::string rootElementName = "log";
            std::string fieldElementName = "field";
            bool includeXmlDeclaration = true;
            bool useCDATA = true;  // Use CDATA for text content

            Options& SetUseAttributes(bool use)
            {
                useAttributes = use;
                return *this;
            }

            Options& SetRootElement(std::string_view rootName)
            {
                rootElementName = rootName;
                return *this;
            }

            Options& SetFieldElement(std::string_view fieldName)
            {
                fieldElementName = fieldName;
                return *this;
            }

            Options& SetXmlDeclaration(bool include)
            {
                includeXmlDeclaration = include;
                return *this;
            }

            Options& SetUseCDATA(bool use)
            {
                useCDATA = use;
                return *this;
            }
        };

        explicit XmlFormatter(const Options& options = Options());

        std::string_view GetContentType() const override;
        std::unique_ptr<StructuredFormatter> Clone() const override;

    protected:
        void EscapeString(std::ostream& os, std::string_view str) const override;
        std::string FormatMessageImpl(const Message& message) const override;
        std::string FormatStructuredDataImpl(const StructuredData& data) const override;

    private:
        std::string FormatXml(const Message& message) const;
        void WriteValue(std::ostream& os, const StructuredData::FieldValue& value, int indentLevel) const;
        std::string WrapInCDATA(std::string_view text) const;
        std::string EscapeXml(std::string_view text) const;

        Options m_xmlOptions;
    };
}
