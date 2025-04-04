#pragma once

#include "BaseStructuredFormatter.h"

namespace FlexLog
{
    class ElasticsearchFormatter : public BaseStructuredFormatter
    {
    public:
        struct Options : public CommonFormatterOptions
        {
            std::string indexNameTemplate = "{application}-{date}";
            std::string docType = "_doc";
            bool useBulkFormat = false;

            Options& SetIndexTemplate(std::string_view indexTemplate)
            {
                indexNameTemplate = indexTemplate;
                return *this;
            }

            Options& SetDocType(std::string_view type)
            {
                docType = type;
                return *this;
            }

            Options& SetBulkFormat(bool bulk)
            {
                useBulkFormat = bulk;
                return *this;
            }
        };

        explicit ElasticsearchFormatter(const Options& options = Options());

        std::string_view GetContentType() const override;
        std::unique_ptr<StructuredFormatter> Clone() const override;

    protected:
        void EscapeString(std::ostream& os, std::string_view str) const override;
        std::string FormatMessageImpl(const Message& message) const override;
        std::string FormatStructuredDataImpl(const StructuredData& data) const override;

        std::string GenerateIndexName() const;
        std::string FormatBulkLine(const Message& message) const;

        Options m_elasticOptions;
    };
}
