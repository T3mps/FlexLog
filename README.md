# FlexLog

<p align="center">
  <img src="https://img.shields.io/badge/language-C%2B%2B20-blue.svg" alt="C++20">
  <img src="https://img.shields.io/badge/license-MPL-green.svg" alt="MPL License">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg" alt="Cross-Platform">
</p>

FlexLog is a high-performance, thread-safe, and feature-rich logging library for modern C++ applications. Designed with flexibility and performance in mind, it supports structured logging formats, multiple output sinks, and advanced configuration options.

## ✨ Features

- **High Performance** - Lock-free message queues and thread pooling for minimal impact on application performance
- **Thread Safety** - Concurrent logging from multiple threads with hazard pointers and atomic operations
- **Structured Logging** - Support for JSON, XML, GELF, CloudWatch, LogStash, Elasticsearch, OpenTelemetry, and Splunk formats
- **Multiple Sinks** - Console and file outputs with rotation support, with an extensible architecture for custom sinks
- **C++20 Features** - Uses the latest C++ features including std::source_location and std::format
- **Cross-Platform** - Works on Windows, Linux, and macOS
- **Configuration** - Flexible configuration API with sensible defaults
- **Memory Efficient** - Message pooling for reduced memory allocations
- **File Rotation** - Size-based and time-based log file rotation

## 📦 Installation

### Prerequisites

- C++20 compatible compiler
- [Premake5](https://premake.github.io/) (for building)

### Build from Source

```bash
git clone https://github.com/yourusername/flexlog.git
cd flexlog
premake5 vs2022    # For Visual Studio 2022
# OR
premake5 gmake2    # For GNU Make
# Then build using your generated build files
```

## 🚀 Quick Start

```cpp
#include "Logging.h"

int main() {
    // Initialize the logging system
    auto& logManager = FlexLog::LogManager::GetInstance();
    logManager.Initialize();
    
    // Basic logging
    FLOG_INFO("Hello, World!");
    
    // Formatted logging (using std::format)
    FLOG_INFO("The answer is {}", 42);
    
    // Log with structured data
    FlexLog::StructuredData data;
    data.Add("user_id", 12345);
    data.Add("action", "login");
    FLOG_INFO("User login", data);
    
    // Clean shutdown
    logManager.Shutdown();
    return 0;
}
```

## 📝 Documentation

### Basic Logging Macros

```cpp
FLOG_TRACE("Trace message");
FLOG_DEBUG("Debug message");
FLOG_INFO("Info message");
FLOG_WARN("Warning message");
FLOG_ERROR("Error message");
FLOG_FATAL("Fatal message");
```

### Named Loggers

```cpp
// Create or get a logger
auto& logger = logManager.RegisterLogger("network");
logger.SetLevel(FlexLog::Level::Debug);

// Use the logger with macros
FLOG_DEBUG_LOGGER("network", "Connection established with {}", "server-01");

// Or directly
logger.Debug("Connection established");
```

### Structured Logging

```cpp
FlexLog::StructuredData data;
data.Add("latitude", 37.7749);
data.Add("longitude", -122.4194);
data.Add("timestamp", std::chrono::system_clock::now());
data.Add("tags", std::vector<std::string>{"location", "gps"});

FLOG_INFO("Location update", data);
```

### Custom Sinks

```cpp
// File sink with rotation
auto& logger = logManager.GetLogger("app");
logger.EmplaceSink<FlexLog::FileSink>(FlexLog::FileSink::Options()
    .SetFilePath("logs/app.log")
    .EnableRotation(true)
    .SetMaxFileSize(10 * 1024 * 1024)  // 10 MB
    .SetTimeRotation(FlexLog::RotationTimeUnit::Day));

// Console sink
logger.EmplaceSink<FlexLog::ConsoleSink>();
```

### Formatting Options

```cpp
// Set JSON formatting for structured logs
auto& format = logger.GetFormat();
format.SetLogFormat(FlexLog::LogFormat::JSON);

// Configure a formatter
auto& jsonFormatter = format.GetJsonFormatter();
jsonFormatter.GetOptions().SetPrettyPrint(true);
```

## ⚙️ Configuration

FlexLog provides several configuration options:

### Log Manager Configuration

```cpp
auto& logManager = FlexLog::LogManager::GetInstance();

// Set default log level
logManager.SetDefaultLevel(FlexLog::Level::Debug);

// Configure thread pool
logManager.SetThreadPoolSize(4);

// Set default logger name
logManager.SetDefaultLoggerName("application");
```

### File Sink Options

```cpp
FlexLog::FileSink::Options options;
options.SetFilePath("logs/app.log")
       .SetCreateDir(true)
       .SetAutoFlush(true)
       .EnableRotation(true)
       .SetRotationRule(FlexLog::RotationRule::SizeAndTime)
       .SetMaxFileSize(10 * 1024 * 1024)  // 10 MB
       .SetTimeRotation(FlexLog::RotationTimeUnit::Day)
       .SetMaxFiles(7);
```

### Custom Pattern Formatting

```cpp
auto& format = logger.GetFormat();
auto& patternFormatter = format.GetPatternFormatter();
patternFormatter.SetPattern("[{timestamp}] [{level}] [{name}.{function}] - {message}");
patternFormatter.SetTimeFormat("%Y-%m-%d %H:%M:%S.%f");
```

## 📄 License

FlexLog is distributed under the Mozilla Public License 2.0 (MPL 2.0)

## 🤝 Contributing

Contributions are welcome! Feel free to open issues or submit pull requests.
