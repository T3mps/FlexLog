#pragma once

#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Common.h"
#include "Level.h"
#include "Message.h"
#include "Sink.h"

namespace FlexLog
{
    struct TerminalCapabilities
    {
        bool supportsColor = false;
        bool supportsRGB = false;
        bool supportsUnicode = false;
        int colorDepth = 0;  // 0=none, 1=4bit, 2=8bit, 3=24bit
        std::string terminalType;
    };

    class ConsoleSink : public Sink
    {
    public:
        struct Options
        {
            bool unicodeEnabled = true;
            size_t maxMessageLength = 16384;

            Options& SetUnicodeEnabled(bool enabled)
            {
                unicodeEnabled = enabled;
                return *this;
            }

            Options& SetMaxMessageLength(size_t length)
            {
                maxMessageLength = length;
                return *this;
            }
        };

        explicit ConsoleSink(const Options& options = Options());
        ~ConsoleSink() override = default;

        void Output(const Message& msg, const Format& format) override;
        void Flush() override;

        [[nodiscard]] const TerminalCapabilities& GetTerminalCapabilities() const { return m_terminalCapabilities; }
        void ForceTerminalCapabilities(const TerminalCapabilities& capabilities);

        const Options& GetOptions() const { return m_options; }

        void SetOutputStream(std::ostream& stream);
        void SetErrorStream(std::ostream& stream);

        [[nodiscard]] uint64_t GetErrorCount() const { return m_errorCount.load(std::memory_order_relaxed); }
        [[nodiscard]] bool HasErrors() const { return m_errorCount > 0; }
        void ResetErrors() { m_errorCount.store(0, std::memory_order_relaxed); }

    private:
        void DetectTerminalCapabilities();
        
        void InitializeWindowsTerminal();

        std::string SanitizeForTerminal(std::string_view text) const;

        void WriteToStream(std::ostream& stream, std::string_view text);

        Options m_options;

        TerminalCapabilities m_terminalCapabilities;

        std::ostream* m_outputStream = &std::cout;
        std::ostream* m_errorStream = &std::cerr;
        std::mutex m_outputMutex;

        std::atomic<uint64_t> m_errorCount{0};
    };
};
