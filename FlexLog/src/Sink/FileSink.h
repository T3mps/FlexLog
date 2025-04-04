#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "Common.h"
#include "Sink.h"

namespace FlexLog
{
    enum class RotationRule
    {
        None,       // No rotation
        Size,       // Rotate based on file size
        Time,       // Rotate based on time (daily, hourly, etc.)
        SizeAndTime // Rotate based on either size or time
    };

    enum class RotationTimeUnit
    {
        Minute,
        Hour,
        Day,
        Week,
        Month,
        Year
    };

    class FileSink : public Sink
    {
    public:
        struct Options
        {
            std::string filePath;

            bool createDir = true;         // Create directory if it doesn't exist
            bool truncateOnOpen = false;   // Truncate file when opening
            bool autoFlush = false;        // Flush after every write
            size_t bufferSize = 8192;      // Buffer size for file writing
            std::string lineEnding = FLOG_NEWLINE; // Line ending to use

            bool enableRotation = false;   // Whether to enable file rotation
            RotationRule rotationRule = RotationRule::Size;
            uint64_t maxFileSize = 10 * 1024 * 1024; // 10 MB default
            RotationTimeUnit timeUnit = RotationTimeUnit::Day;
            uint32_t timeValue = 1;        // Rotate every N time units
            uint32_t maxFiles = 5;         // Maximum number of rotated files to keep
            std::string rotationPattern = "{basename}.{timestamp}.{ext}"; // Pattern for rotated files
            bool compressRotatedFiles = false; // Compress rotated files

            bool enableFileLock = false;

            Options& SetFilePath(std::string_view path) { filePath = path; return *this; }
            Options& SetCreateDir(bool value) { createDir = value; return *this; }
            Options& SetTruncateOnOpen(bool value) { truncateOnOpen = value; return *this; }
            Options& SetAutoFlush(bool value) { autoFlush = value; return *this; }
            Options& SetBufferSize(size_t size) { bufferSize = size; return *this; }
            Options& SetLineEnding(std::string_view ending) { lineEnding = ending; return *this; }

            Options& EnableRotation(bool enable = true) { enableRotation = enable; return *this; }
            Options& SetRotationRule(RotationRule rule) { rotationRule = rule; return *this; }
            Options& SetMaxFileSize(uint64_t size) { maxFileSize = size; return *this; }
            Options& SetTimeRotation(RotationTimeUnit unit, uint32_t value = 1) { timeUnit = unit; timeValue = value; return *this; }
            Options& SetMaxFiles(uint32_t count) { maxFiles = count; return *this; }
            Options& SetRotationPattern(std::string_view pattern) { rotationPattern = pattern; return *this; }
            Options& EnableCompression(bool enable = true) { compressRotatedFiles = enable; return *this; }

            Options& EnableFileLock(bool enable = true) { enableFileLock = enable; return *this; }
        };

        explicit FileSink(const Options& options = Options());
        ~FileSink() override;

        void Output(const Message& msg, const Format& format) override;
        void Flush() override;

        bool ReOpen(); // Reopen the file (useful after rotation)
        void Close();  // Explicitly close the file

        const Options& GetOptions() const { return m_options; }
        bool IsOpen();
        uint64_t GetCurrentFileSize() const { return m_currentFileSize; }

    private:
        bool OpenFile();
        void CloseFile();
        bool ShouldRotate() const;
        void RotateFile();
        std::string FormatRotatedFilename() const;
        bool CreateDirectoryIfNeeded();
        void PruneOldFiles();
        bool CompressFile(const std::filesystem::path& filePath);

        std::chrono::system_clock::time_point CalculateNextRotationTime() const;
        bool IsTimeToRotate() const;

        bool AcquireFileLock();
        void ReleaseFileLock();

        Options m_options;
        std::ofstream m_file;
        std::mutex m_mutex;

        uint64_t m_currentFileSize = 0;
        std::chrono::system_clock::time_point m_lastRotationTime;
        std::chrono::system_clock::time_point m_nextRotationTime;

#ifdef FLOG_PLATFORM_WINDOWS
        void* m_fileLockHandle = nullptr;
#else
        int m_fileLockFd = -1;
#endif

        bool m_initialized = false;
    };
}
