#include "FileSink.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifdef FLOG_PLATFORM_WINDOWS
    #include <Windows.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/file.h>
    #include <sys/stat.h>
#endif

#ifdef FLOG_ENABLE_LOGGER_FILE_COMPRESSION
    #include <zlib.h>
#endif

FlexLog::FileSink::FileSink(const Options& options) : m_options(options)
{
    // Initialize state
    m_lastRotationTime = std::chrono::system_clock::now();

    if (m_options.enableRotation && m_options.rotationRule == RotationRule::Time || m_options.rotationRule == RotationRule::SizeAndTime)
        m_nextRotationTime = CalculateNextRotationTime();

    if (!m_options.filePath.empty())
        m_initialized = OpenFile();
}

FlexLog::FileSink::~FileSink()
{
    CloseFile();
}

void FlexLog::FileSink::Output(const Message& msg, const Format& format)
{
    if (!m_initialized)
        return;

    try
    {
        std::string formattedMessage = format(msg);
        if (formattedMessage.empty())
            return;

        // Make sure the message ends with a line break
        if (formattedMessage.back() != '\n')
            formattedMessage += m_options.lineEnding;

        // Check rotation before writing
        bool needsReopen = false;

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_options.enableRotation && ShouldRotate())
            {
                RotateFile();
                needsReopen = true;
            }

            if (needsReopen && !m_file.is_open())
            {
                if (!OpenFile())
                    return;
            }

            if (m_file.is_open())
            {
                m_file.write(formattedMessage.data(), formattedMessage.size());
                m_currentFileSize += formattedMessage.size();

                if (m_options.autoFlush)
                    m_file.flush();
            }
        }
    }
    catch (const std::exception&)
    {
        // Log writing failed - we could add fallback behavior here
    }
}

void FlexLog::FileSink::Flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open())
        m_file.flush();
}

bool FlexLog::FileSink::ReOpen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    CloseFile();
    return OpenFile();
}

void FlexLog::FileSink::Close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    CloseFile();
}

bool FlexLog::FileSink::IsOpen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_file.is_open();
}

bool FlexLog::FileSink::OpenFile()
{
    if (m_options.createDir)
    {
        if (!CreateDirectoryIfNeeded())
            return false;
    }

    std::ios::openmode mode = std::ios::out;
    if (m_options.truncateOnOpen)
        mode |= std::ios::trunc;
    else
        mode |= std::ios::app;

    m_file.open(m_options.filePath, mode);

    if (!m_file)
        return false;

    if (m_options.bufferSize > 0)
    {
        static std::vector<char> buffer;
        buffer.resize(m_options.bufferSize);
        m_file.rdbuf()->pubsetbuf(buffer.data(), buffer.size());
    }

    if (m_options.enableFileLock && !AcquireFileLock())
    {
        CloseFile();
        return false;
    }

    m_file.seekp(0, std::ios::end);
    m_currentFileSize = static_cast<uint64_t>(m_file.tellp());

    return true;
}

void FlexLog::FileSink::CloseFile()
{
    if (m_file.is_open())
    {
        m_file.flush();
        m_file.close();
    }

    if (m_options.enableFileLock)
        ReleaseFileLock();

    m_currentFileSize = 0;
}

bool FlexLog::FileSink::ShouldRotate() const
{
    if (!m_options.enableRotation)
        return false;

    switch (m_options.rotationRule)
    {
        case RotationRule::Size:        return m_currentFileSize >= m_options.maxFileSize;
        case RotationRule::Time:        return IsTimeToRotate();
        case RotationRule::SizeAndTime: return m_currentFileSize >= m_options.maxFileSize || IsTimeToRotate();
        default:                        return false;
    }
}

void FlexLog::FileSink::RotateFile()
{
    m_file.close();

    if (m_options.enableFileLock)
        ReleaseFileLock();

    std::string rotatedFilename = FormatRotatedFilename();

    try
    {
        std::filesystem::rename(m_options.filePath, rotatedFilename);
    }
    catch (const std::filesystem::filesystem_error&)
    {
        // Failed to rename - could be due to file in use
        // Try to copy instead
        try
        {
            std::filesystem::copy_file(
                m_options.filePath, 
                rotatedFilename, 
                std::filesystem::copy_options::overwrite_existing
            );

            // If successful, truncate the original
            std::ofstream truncateFile(m_options.filePath, std::ios::trunc);
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // Even copy failed - can't rotate
        }
    }

    if (m_options.compressRotatedFiles)
        CompressFile(rotatedFilename);

    m_lastRotationTime = std::chrono::system_clock::now();

    if (m_options.rotationRule == RotationRule::Time || m_options.rotationRule == RotationRule::SizeAndTime)
        m_nextRotationTime = CalculateNextRotationTime();

    // Clean up old files if needed
    PruneOldFiles();

    // Reopen file
    OpenFile();
}

std::string FlexLog::FileSink::FormatRotatedFilename() const
{
    std::filesystem::path originalPath(m_options.filePath);
    std::string basename = originalPath.stem().string();
    std::string extension = originalPath.extension().string();

    // Generate timestamp string
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&time_t_now), "%Y%m%d-%H%M%S");

    // Format the filename using the pattern
    std::string result = m_options.rotationPattern;

    // Simple token replacement
    size_t pos;
    if ((pos = result.find("{basename}")) != std::string::npos)
        result.replace(pos, 10, basename);

    if ((pos = result.find("{timestamp}")) != std::string::npos)
        result.replace(pos, 11, timestamp.str());

    if ((pos = result.find("{ext}")) != std::string::npos)
        result.replace(pos, 5, extension.empty() ? "" : extension.substr(1)); // Remove leading dot

    // Check if we need to convert to absolute path
    if (std::filesystem::path(result).is_relative())
        return (originalPath.parent_path() / result).string();

    return result;
}

bool FlexLog::FileSink::CreateDirectoryIfNeeded()
{
    try
    {
        std::filesystem::path filePath(m_options.filePath);
        std::filesystem::path dir = filePath.parent_path();

        if (!dir.empty() && !std::filesystem::exists(dir))
            return std::filesystem::create_directories(dir);
        return true;
    }
    catch (const std::filesystem::filesystem_error&)
    {
        return false;
    }
}

void FlexLog::FileSink::PruneOldFiles()
{
    if (m_options.maxFiles == 0)
        return;

    try
    {
        std::filesystem::path basePath(m_options.filePath);
        std::filesystem::path dirPath = basePath.parent_path();
        std::string baseFilename = basePath.filename().string();

        // Collect all rotated files
        std::vector<std::filesystem::path> rotatedFiles;

        for (const auto& entry : std::filesystem::directory_iterator(dirPath))
        {
            const std::string& filename = entry.path().filename().string();

            // Simple heuristic - file starts with the base name but isn't the current log file
            if (filename.find(basePath.stem().string()) == 0 && filename != baseFilename)
                rotatedFiles.push_back(entry.path());
        }

        // Sort by modification time (oldest first)
        std::sort(rotatedFiles.begin(), rotatedFiles.end(), 
            [](const std::filesystem::path& a, const std::filesystem::path& b) {
                return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
            });

        // Remove excess files
        if (rotatedFiles.size() > m_options.maxFiles)
        {
            size_t filesToRemove = rotatedFiles.size() - m_options.maxFiles;
            for (size_t i = 0; i < filesToRemove; ++i)
            {
                std::filesystem::remove(rotatedFiles[i]);
            }
        }
    }
    catch (const std::filesystem::filesystem_error&)
    {
        // Failed to prune files - log error if possible
    }
}

bool FlexLog::FileSink::CompressFile(const std::filesystem::path& filePath)
{
#ifdef ENABLE_COMPRESSION
    // Implementation using zlib or similar
    // This is a placeholder - you'll need to implement actual compression
    return true;
#else
    // Compression not available
    return false;
#endif
}

std::chrono::system_clock::time_point FlexLog::FileSink::CalculateNextRotationTime() const
{
    auto now = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point nextTime;

    // Get current time components
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm timeInfo;

#ifdef FLOG_PLATFORM_WINDOWS
    localtime_s(&timeInfo, &currentTime);
#else
    localtime_r(&currentTime, &timeInfo);
#endif

    // Adjust based on rotation interval
    switch (m_options.timeUnit)
    {
        case RotationTimeUnit::Minute:
            timeInfo.tm_min += m_options.timeValue;
            timeInfo.tm_sec = 0;
            break;
        case RotationTimeUnit::Hour:
            timeInfo.tm_hour += m_options.timeValue;
            timeInfo.tm_min = 0;
            timeInfo.tm_sec = 0;
            break;
        case RotationTimeUnit::Day:
            timeInfo.tm_mday += m_options.timeValue;
            timeInfo.tm_hour = 0;
            timeInfo.tm_min = 0;
            timeInfo.tm_sec = 0;
            break;
        case RotationTimeUnit::Week:
            timeInfo.tm_mday += (7 * m_options.timeValue) - timeInfo.tm_wday;
            timeInfo.tm_hour = 0;
            timeInfo.tm_min = 0;
            timeInfo.tm_sec = 0;
            break;
        case RotationTimeUnit::Month:
            timeInfo.tm_mon += m_options.timeValue;
            timeInfo.tm_mday = 1;
            timeInfo.tm_hour = 0;
            timeInfo.tm_min = 0;
            timeInfo.tm_sec = 0;
            break;
        case RotationTimeUnit::Year:
            timeInfo.tm_year += m_options.timeValue;
            timeInfo.tm_mon = 0;
            timeInfo.tm_mday = 1;
            timeInfo.tm_hour = 0;
            timeInfo.tm_min = 0;
            timeInfo.tm_sec = 0;
            break;
    }

    time_t nextTimeT = mktime(&timeInfo);
    return std::chrono::system_clock::from_time_t(nextTimeT);
}

bool FlexLog::FileSink::IsTimeToRotate() const
{
    auto now = std::chrono::system_clock::now();
    return now >= m_nextRotationTime;
}

bool FlexLog::FileSink::AcquireFileLock()
{
#ifdef FLOG_PLATFORM_WINDOWS
    // Windows file locking implementation
    m_fileLockHandle = CreateFileA
    (
        m_options.filePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    return m_fileLockHandle != INVALID_HANDLE_VALUE;
#else
    // POSIX file locking implementation
    std::string lockFilePath = m_options.filePath + ".lock";

    m_fileLockFd = open(lockFilePath.c_str(), O_CREAT | O_RDWR, 0666);
    if (m_fileLockFd < 0)
        return false;

    struct flock fl;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    return fcntl(m_fileLockFd, F_SETLK, &fl) != -1;
#endif
}

void FlexLog::FileSink::ReleaseFileLock()
{
#ifdef FLOG_PLATFORM_WINDOWS
    if (m_fileLockHandle != nullptr && m_fileLockHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_fileLockHandle);
        m_fileLockHandle = nullptr;
    }
#else
    if (m_fileLockFd >= 0)
    {
        struct flock fl;
        fl.l_type = F_UNLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0;

        fcntl(m_fileLockFd, F_SETLK, &fl);
        close(m_fileLockFd);
        m_fileLockFd = -1;

        // Remove lock file
        std::string lockFilePath = m_options.filePath + ".lock";
        unlink(lockFilePath.c_str());
    }
#endif
}
