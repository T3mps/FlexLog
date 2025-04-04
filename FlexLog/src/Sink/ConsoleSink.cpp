#include "ConsoleSink.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iostream>
#include <locale>
#include <regex>
#include <sstream>

#include "Message.h"
#include "Platform.h"

#ifdef FLOG_PLATFORM_WINDOWS
    #include <Windows.h>
    #include <io.h>
    #define ISATTY _isatty
    #define FILENO _fileno
#else
    #include <unistd.h>
    #define ISATTY isatty
    #define FILENO fileno
#endif

namespace
{
    std::string GetEnvVar(const char* name)
    {
        const char* val = std::getenv(name);
        return val ? std::string(val) : std::string();
    }

    bool IsTerminal(const std::ostream* stream)
    {
        if (!stream)
            return false;

        FILE* file = nullptr;
        if (stream == &std::cout)
            file = stdout;
        else if (stream == &std::cerr)
            file = stderr;
        else
            return false;

        return ISATTY(FILENO(file)) != 0;
    }

    std::string SanitizeText(std::string_view text, bool preserveNewlines = true)
    {
        std::string result;
        result.reserve(text.size());

        for (char c : text)
        {
            if (c == '\n' && preserveNewlines)
                result.push_back(c);
            else if (std::iscntrl(static_cast<unsigned char>(c)) && c != '\t')
                continue; // Skip control characters except tabs
            else
                result.push_back(c);
        }

        return result;
    }
}

FlexLog::ConsoleSink::ConsoleSink(const Options& options) : m_options(options)
{
    DetectTerminalCapabilities();
#ifdef FLOG_PLATFORM_WINDOWS
    InitializeWindowsTerminal();
#endif
}

void FlexLog::ConsoleSink::Output(const Message& msg, const Format& format)
{
    if (!m_outputStream)
        return;

    try
    {
        std::string formattedMessage = format(msg);
        if (formattedMessage.empty())
            return;

        if (formattedMessage.length() > m_options.maxMessageLength)
            formattedMessage = formattedMessage.substr(0, m_options.maxMessageLength - 4) + "...";

        formattedMessage = SanitizeForTerminal(formattedMessage);

        if (!formattedMessage.empty() && formattedMessage.back() != '\n')
            formattedMessage += FLOG_NEWLINE;

        std::ostream* targetStream = m_outputStream;
        if (msg.level >= Level::Error && m_errorStream)
            targetStream = m_errorStream;

        std::lock_guard<std::mutex> lock(m_outputMutex);
        WriteToStream(*targetStream, formattedMessage);
    }
    catch (const std::exception& e)
    {
        m_errorCount.fetch_add(1, std::memory_order_relaxed);

        try
        {
            std::string errorMsg = "ConsoleSink error: ";
            errorMsg += e.what();
            errorMsg += FLOG_NEWLINE;

            std::cerr << errorMsg;
        }
        catch (...) 
        {
            // Last resort, if even this fails, we can't do much
        }
    }
}

void FlexLog::ConsoleSink::Flush()
{
    m_outputStream->flush();
    m_errorStream->flush();
}

void FlexLog::ConsoleSink::SetOutputStream(std::ostream& stream)
{
    std::lock_guard<std::mutex> lock(m_outputMutex);
    m_outputStream = &stream;
    DetectTerminalCapabilities();
}

void FlexLog::ConsoleSink::SetErrorStream(std::ostream& stream)
{
    std::lock_guard<std::mutex> lock(m_outputMutex);
    m_errorStream = &stream;
}

void FlexLog::ConsoleSink::ForceTerminalCapabilities(const TerminalCapabilities& capabilities)
{
    std::lock_guard<std::mutex> lock(m_outputMutex);
    m_terminalCapabilities = capabilities;
}

void FlexLog::ConsoleSink::DetectTerminalCapabilities()
{
    m_terminalCapabilities = TerminalCapabilities{};

    std::string forceColor = GetEnvVar("FORCE_COLOR");
    std::string noColor = GetEnvVar("NO_COLOR");
    std::string colorTerm = GetEnvVar("COLORTERM");
    std::string term = GetEnvVar("TERM");

    if (!forceColor.empty() && forceColor != "0")
    {
        m_terminalCapabilities.supportsColor = true;
    }
    else if (!noColor.empty())
    {
        m_terminalCapabilities.supportsColor = false;
    }
    else
    {
#ifdef FLOG_PLATFORM_WINDOWS
        // Check for common Windows environments that support colors
        std::string termProgram = GetEnvVar("TERM_PROGRAM");
        bool isVsCode = (termProgram == "vscode");
        bool isWindowsTerminal = !GetEnvVar("WT_SESSION").empty();
        bool isMinTTY = !term.empty() && (term.find("xterm") != std::string::npos || term.find("mintty") != std::string::npos);

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr)
        {
            DWORD mode = 0;
            if (GetConsoleMode(hOut, &mode))
            {
                SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
                m_terminalCapabilities.supportsColor = true;
                m_terminalCapabilities.colorDepth = 1; // Basic 16-color support

                // Windows 10 Anniversary Update and later supports 24-bit color
                if (isVsCode || isWindowsTerminal || isMinTTY)
                {
                    m_terminalCapabilities.colorDepth = 3; // 24-bit color for modern terminals
                }
                else
                {
                    OSVERSIONINFOEX osvi{};
                    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
#pragma warning(disable: 4996) // GetVersionEx is deprecated but useful here
                    if (GetVersionEx(reinterpret_cast<OSVERSIONINFO*>(&osvi)) && (osvi.dwMajorVersion >= 10 && osvi.dwBuildNumber >= 14393))
                        m_terminalCapabilities.colorDepth = 3; // 24-bit color
#pragma warning(default: 4996)
                }
            }
        }
        else
        {
            // Check if we're in MinTTY or similar
            m_terminalCapabilities.supportsColor = isVsCode || isWindowsTerminal || isMinTTY || 
                (!term.empty() && term != "dumb");
        }
#else
        // Unix/Linux/macOS terminal detection
        m_terminalCapabilities.supportsColor = IsTerminal(m_outputStream) && !term.empty() && term != "dumb";
#endif
    }


    m_terminalCapabilities.terminalType = term;

    if (m_terminalCapabilities.supportsColor)
    {
        if (colorTerm == "truecolor" || colorTerm == "24bit")
        {
            m_terminalCapabilities.supportsRGB = true;
            m_terminalCapabilities.colorDepth = 3; // 24-bit color
        }
        else if (term.find("256color") != std::string::npos)
        {
            m_terminalCapabilities.colorDepth = 2; // 8-bit color (256 colors)
        }

        std::string lang = GetEnvVar("LANG");
        std::string lcAll = GetEnvVar("LC_ALL");

        m_terminalCapabilities.supportsUnicode = 
            (lang.find("UTF-8") != std::string::npos || 
                lang.find("utf8") != std::string::npos ||
                lcAll.find("UTF-8") != std::string::npos ||
                lcAll.find("utf8") != std::string::npos);

#ifdef FLOG_PLATFORM_WINDOWS
        m_terminalCapabilities.supportsUnicode |= (GetConsoleOutputCP() == CP_UTF8);
#endif
    }
}

void FlexLog::ConsoleSink::InitializeWindowsTerminal()
{
#ifdef FLOG_PLATFORM_WINDOWS
    if (m_terminalCapabilities.supportsColor)
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);

        if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr)
        {
            DWORD mode = 0;
            if (GetConsoleMode(hOut, &mode))
            {
                if (!SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                {
                    m_terminalCapabilities.supportsColor = false;
                    m_terminalCapabilities.supportsRGB = false;
                }
            }
        }

        if (hErr != INVALID_HANDLE_VALUE && hErr != nullptr && hErr != hOut)
        {
            DWORD mode = 0;
            if (GetConsoleMode(hErr, &mode))
            {
                if (!SetConsoleMode(hErr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                    m_errorCount.fetch_add(1, std::memory_order_relaxed);
            }
        }

        if (m_options.unicodeEnabled)
            SetConsoleOutputCP(CP_UTF8);
    }
#endif
}

std::string FlexLog::ConsoleSink::SanitizeForTerminal(std::string_view text) const
{
    if (!m_terminalCapabilities.supportsUnicode || !m_options.unicodeEnabled)
    {
        std::string result;
        result.reserve(text.size());

        for (char c : text)
        {
            if ((c >= 32 && c < 127) || c == '\n' || c == '\r' || c == '\t')
                result.push_back(c);
            else if (static_cast<unsigned char>(c) >= 128) // Unicode replacement
                result.append("?");
        }
        return result;
    }
    else
    {
        return SanitizeText(text);
    }
}

void FlexLog::ConsoleSink::WriteToStream(std::ostream& stream, std::string_view text)
{
    try
    {
        stream << text;
        stream.flush();
    }
    catch (const std::exception&)
    {
        m_errorCount.fetch_add(1, std::memory_order_relaxed);
    }
}
