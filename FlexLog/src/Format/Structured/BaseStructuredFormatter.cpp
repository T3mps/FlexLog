#include "BaseStructuredFormatter.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

FlexLog::BaseStructuredFormatter::BaseStructuredFormatter(const CommonFormatterOptions& options) : m_options(options)
{
    // Initialize hostname if empty
    if (m_options.hostname.empty())
        m_options.hostname = GetHostname();
}

std::string FlexLog::BaseStructuredFormatter::FormatMessage(const Message& message) const
{
    return FormatMessageImpl(message);
}

#ifdef FLOG_PLATFORM_WINDOWS
    #include <Windows.h>
#endif

std::string FlexLog::BaseStructuredFormatter::FormatStructuredData(const StructuredData& data) const
{
    return FormatStructuredDataImpl(data);
}

void FlexLog::BaseStructuredFormatter::SetOptions(const CommonFormatterOptions& options)
{
    m_options = options;

    // Initialize hostname if empty
    if (m_options.hostname.empty())
        m_options.hostname = GetHostname();
}

std::string FlexLog::BaseStructuredFormatter::FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const
{
    auto time_t_now = std::chrono::system_clock::to_time_t(timestamp);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        timestamp.time_since_epoch() % std::chrono::seconds(1));

    std::stringstream ss;

    // Handle special case for ISO8601 with microseconds
    if (m_options.timeFormat == "%FT%T.%fZ")
    {
        ss << std::put_time(std::gmtime(&time_t_now), "%FT%T");
        ss << '.' << std::setfill('0') << std::setw(6) << microseconds.count() << 'Z';
    }
    else
    {
        std::string format = m_options.timeFormat;

        // Replace %f with microseconds if present
        size_t pos = format.find("%f");
        if (pos != std::string::npos)
        {
            std::string microsStr = std::to_string(microseconds.count());
            microsStr = std::string(6 - microsStr.length(), '0') + microsStr;
            format.replace(pos, 2, microsStr);
        }

        ss << std::put_time(std::gmtime(&time_t_now), format.c_str());
    }

    return ss.str();
}

std::string FlexLog::BaseStructuredFormatter::FormatSourceLocation(const std::source_location& location) const
{
    std::stringstream ss;
    ss << std::filesystem::path(location.file_name()).filename().string()
        << ":" << location.line()
        << " [" << location.function_name() << "]";
    return ss.str();
}

std::string FlexLog::BaseStructuredFormatter::GetProcessId() const
{
#ifdef FLOG_PLATFORM_WINDOWS
    return std::to_string(GetCurrentProcessId());
#else
    return std::to_string(getpid());
#endif
}

std::string FlexLog::BaseStructuredFormatter::GetProcessName() const
{
#ifdef FLOG_PLATFORM_WINDOWS
    char processName[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, processName, MAX_PATH);
    return std::filesystem::path(processName).filename().string();
#else
    char processName[1024] = {0};
    sprintf(processName, "/proc/%d/cmdline", getpid());

    FILE* cmdFile = fopen(processName, "r");
    if (cmdFile)
    {
        size_t read = fread(processName, 1, sizeof(processName) - 1, cmdFile);
        fclose(cmdFile);

        if (read > 0)
        {
            processName[read] = '\0';
            return std::filesystem::path(processName).filename().string();
        }
    }

    return "unknown";
#endif
}

std::string FlexLog::BaseStructuredFormatter::GetThreadId() const
{
    std::ostringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

std::string FlexLog::BaseStructuredFormatter::GetHostname() const
{
    char hostname[256];
#ifdef FLOG_PLATFORM_WINDOWS
    DWORD size = sizeof(hostname);
    if (GetComputerNameA(hostname, &size))
        return std::string(hostname, size);
#else
    if (gethostname(hostname, sizeof(hostname)) == 0)
        return std::string(hostname);
#endif
    return "unknown";
}

void FlexLog::BaseStructuredFormatter::WriteIndent(std::ostream& os, int level) const
{
    if (m_options.prettyPrint)
        os << std::string(level * m_options.indentSize, ' ');
}
