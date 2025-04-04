# FlexLog 🌐

## Overview

FlexLog is a cutting-edge logging library designed to provide developers with unparalleled flexibility and performance in log management. Born from the demanding requirements of modern software development, FlexLog adapts to your project's unique logging needs while maintaining exceptional efficiency.

### 🚀 Core Philosophy

At its heart, FlexLog is built on a simple yet powerful idea: logging should be as flexible as the applications it serves. Whether you're developing a high-performance game engine, a complex enterprise system, or a small utility application, FlexLog offers the tools to craft precisely the logging strategy you need.

### 🌟 Key Features

#### Versatile Logging Formats
FlexLog supports a wide array of logging formats, ensuring compatibility with various logging ecosystems:
- Pattern-based logging
- JSON
- XML
- CloudWatch
- Elasticsearch
- GELF (Graylog Extended Log Format)
- Logstash
- OpenTelemetry
- Splunk

#### Performance-Driven Architecture
- Lock-free data structures
- Thread-safe message processing
- Efficient message pooling
- Minimal runtime overhead
- Compile-time log level optimization

#### Adaptive Configuration
- Dynamic log level management
- Flexible sink configuration
- Structured logging support
- Custom log formatting
- Runtime log reconfiguration

#### Intelligent Logging Capabilities
- Detailed source location tracking
- Thread and process information capture
- Rich structured data embedding
- Compile-time log elimination for release builds

## Installation

### Prerequisites

- C++17 or later
- CMake 3.12+
- Supported Compilers:
  - GCC 9+
  - Clang 8+
  - MSVC 19.20+

## Quick Start Guide

### Basic Logging

```cpp
#include <FlexLog/Logger.h>

int main() {
    // Initialize log manager
    auto& logManager = Arcane::LogManager::GetInstance();
    logManager.Initialize();

    // Simple logging
    ARC_INFO("Application started successfully");
    ARC_ERROR("Connection failed: {}", errorDetails);

    // Named logger for specific contexts
    ARC_INFO_LOGGER("network", "Connection established");

    logManager.Shutdown();
}
```

### Advanced Configuration

```cpp
// Configure global log level
logManager.SetDefaultLevel(Arcane::Level::Debug);

// Create a file sink with intelligent rotation
auto fileSink = std::make_shared<Arcane::FileSink>(
    Arcane::FileSink::Options()
        .SetFilePath("app.log")
        .EnableRotation()
        .SetMaxFileSize(10 * 1024 * 1024)  // 10 MB max file size
        .SetTimeRotation(Arcane::RotationTimeUnit::Day)
);
logManager.RegisterSink(fileSink);

// Configure JSON formatter with rich metadata
auto jsonFormatter = std::make_shared<Arcane::JsonFormatter>(
    Arcane::JsonFormatter::Options()
        .SetPrettyPrint(true)
        .SetApplication("MyApp", "production")
        .SetProcessInfo(true)
        .SetThreadId(true)
);
fileSink->SetFormatter(jsonFormatter);
```

### Structured Logging

```cpp
// Embed rich contextual information with structured logs
StructuredData userData;
userData.Add("user_id", 12345)
        .Add("action", "login")
        .Add("ip_address", "192.168.1.100")
        .Add("timestamp", std::chrono::system_clock::now());

ARC_INFO_WITH_DATA("User authentication attempt", userData);
```

## Performance Deep Dive

FlexLog is engineered with performance at its core:
- Lock-free concurrent message queues minimize synchronization overhead
- Thread-local message caching reduces contention
- Compile-time log level elimination removes unnecessary runtime checks
- Efficient memory pooling reduces allocation pressure
- Minimal abstraction penalty ensures near-zero logging overhead

## Platform Support

Supported on:
- Windows
- Linux
- macOS
- Additional POSIX-compliant systems

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines on:
- Reporting issues
- Suggesting enhancements
- Submitting pull requests

## License

Distributed under the MIT License. See [LICENSE](LICENSE) for more details.

## Philosophy Behind FlexLog

Logging should never be a burden on performance. It should seamlessly integrate into your workflow, provide insights when you need them, and stay out of your way when you don't. FlexLog is a commitment to powerful logging; a flexible tool in every developer's arsenal.

---

**Status**: Active Development 🚧
Feedback, stars, and contributions are immensely appreciated! 🌟
