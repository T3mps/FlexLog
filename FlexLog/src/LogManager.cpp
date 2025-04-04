#include "LogManager.h"

#include <thread>
#include <algorithm>
#include <optional>

#include "Sink/ConsoleSink.h"

struct FlexLog::LogManager::LoggerEntry 
{
    std::string name;
    std::unique_ptr<Logger> logger;
    std::atomic<LoggerEntry*> next{nullptr};

    LoggerEntry(std::string n, std::unique_ptr<Logger> l) : name(std::move(n)), logger(std::move(l)) {}
};

class FlexLog::LogManager::LoggerMap 
{
public:
    static constexpr size_t NUM_BUCKETS = 1 << 8; // Power of 2 for efficient masking

    explicit LoggerMap(HazardPointerDomain* domain) : m_hazardDomain(domain)
    {
        for (auto& bucket : m_buckets)
        {
            bucket.store(nullptr, std::memory_order_relaxed);
        }
    }

    ~LoggerMap()
    {
        Clear();
    }

    size_t GetBucketIndex(std::string_view name) const
    {
        if (name.empty())
            return 0;

        // FNV-1a 64-bit hash with better avalanche
        constexpr uint64_t FNV_OFFSET_BASIS = 0xCBF29CE484222325ULL;
        constexpr uint64_t FNV_PRIME = 0x100000001B3ULL;

        uint64_t hash = FNV_OFFSET_BASIS;

        for (char c : name)
        {
            hash ^= static_cast<unsigned char>(c);
            hash *= FNV_PRIME;
        }

        // Apply a final mixing step for better avalanche effect
        hash ^= hash >> 32;

        // Since NUM_BUCKETS is a power of 2, we can use a bitmask
        return hash & (NUM_BUCKETS - 1);
    }

    std::optional<std::reference_wrapper<Logger>> Find(std::string_view name) const
    {
        if (name.empty())
            return std::nullopt;

        size_t bucketIndex = GetBucketIndex(name);

        HazardPointer hp(m_hazardDomain);
        LoggerEntry* current = m_buckets[bucketIndex].load(std::memory_order_acquire);

        while (current)
        {
            current = hp.Protect(current);

            // If protection failed or pointer changed, restart
            if (!current || current != m_buckets[bucketIndex].load(std::memory_order_acquire))
            {
                current = m_buckets[bucketIndex].load(std::memory_order_acquire);
                continue;
            }

            // Check if this is the logger we're looking for
            if (current->name == name)
                return std::optional<std::reference_wrapper<Logger>>{*(current->logger)};

            // Move to next entry
            LoggerEntry* next = current->next.load(std::memory_order_acquire);
            hp.Reset();
            current = next;
        }

        return std::nullopt;
    }

    bool Contains(std::string_view name) const
    {
        return Find(name).has_value();
    }

    Logger& Insert(std::string name, std::unique_ptr<Logger> logger)
    {
        size_t bucketIndex = GetBucketIndex(name);

        auto* entry = new LoggerEntry(std::move(name), std::move(logger));
        Logger& loggerRef = *(entry->logger);

        LoggerEntry* oldHead = m_buckets[bucketIndex].load(std::memory_order_acquire);
        do
        {
            entry->next.store(oldHead, std::memory_order_relaxed);
        }
        while (!m_buckets[bucketIndex].compare_exchange_weak(oldHead, entry, std::memory_order_release, std::memory_order_acquire));

        return loggerRef;
    }

    bool Remove(std::string_view name)
    {
        size_t bucketIndex = GetBucketIndex(name);

        HazardPointer hpCurrent(m_hazardDomain);
        HazardPointer hpNext(m_hazardDomain);

        LoggerEntry* prev = nullptr;
        LoggerEntry* current = m_buckets[bucketIndex].load(std::memory_order_acquire);

        while (current)
        {
            // Protect current from reclamation
            current = hpCurrent.Protect(current);

            // If head has changed and we're at the head, restart
            if (prev == nullptr && current != m_buckets[bucketIndex].load(std::memory_order_acquire))
            {
                current = m_buckets[bucketIndex].load(std::memory_order_acquire);
                continue;
            }

            // Get next entry with hazard pointer protection
            LoggerEntry* next = current->next.load(std::memory_order_acquire);
            next = hpNext.Protect(next);

            if (current->name == name)
            {
                // Found the entry to remove
                if (prev == nullptr)
                {
                    // Entry is at head of list
                    if (m_buckets[bucketIndex].compare_exchange_strong(current, next, std::memory_order_release, std::memory_order_acquire))
                    {
                        // Successfully removed, schedule the node for deletion
                        hpCurrent.Reset();
                        m_hazardDomain->RetireNode(current);
                        return true;
                    }
                    // CAS failed, retry from beginning
                    current = m_buckets[bucketIndex].load(std::memory_order_acquire);
                    continue;
                }
                else
                {
                    // Entry is in middle/end of list
                    if (prev->next.compare_exchange_strong(current, next, std::memory_order_release, std::memory_order_acquire))
                    {
                        // Successfully removed, schedule the node for deletion
                        hpCurrent.Reset();
                        m_hazardDomain->RetireNode(current);
                        return true;
                    }
                    // CAS failed, retry from beginning
                    prev = nullptr;
                    current = m_buckets[bucketIndex].load(std::memory_order_acquire);
                    continue;
                }
            }

            // Move to next entry
            prev = current;
            hpCurrent = std::move(hpNext);
            current = next;
            hpNext = HazardPointer(m_hazardDomain);
        }

        return false; // Entry not found
    }

    void Clear()
    {
        for (auto& bucket : m_buckets)
        {
            LoggerEntry* current = bucket.exchange(nullptr, std::memory_order_acquire);
            while (current)
            {
                LoggerEntry* next = current->next.load(std::memory_order_acquire);
                delete current;
                current = next;
            }
        }
    }

private:
    std::array<std::atomic<LoggerEntry*>, NUM_BUCKETS> m_buckets;
    HazardPointerDomain* m_hazardDomain;
};

FlexLog::LogManager& FlexLog::LogManager::GetInstance()
{
    static LogManager instance;
    return instance;
}

FlexLog::LogManager::LogManager() :
    m_hazardDomain(std::make_unique<HazardPointerDomain>()),
    m_defaultLoggerName(std::make_unique<AtomicString>("main")),
    m_messagePool(nullptr),
    m_state(LogManagerState::Uninitialized)
{}

FlexLog::LogManager::~LogManager()
{
    ShutdownAll();
}

FlexLog::Result<void> FlexLog::LogManager::Initialize()
{
    static std::once_flag initFlag;
    static Result<void> initResult = Ok();

    try
    {
        std::call_once(initFlag, [&]()
        {
            try
            {
                if (!CheckAndTransitionState(LogManagerState::Uninitialized, LogManagerState::Initializing))
                {
                    LogManagerState currentState = m_state.load(std::memory_order_acquire);
                    initResult = Err<void>(1, std::string("Cannot initialize LogManager: already in state ") + GetStateName(currentState));
                    return;
                }

                m_messagePool = std::make_unique<MessagePool>();
                if (!m_messagePool)
                {
                    m_state.store(LogManagerState::Uninitialized, std::memory_order_release);
                    throw std::runtime_error("Failed to create message pool");
                }

                m_loggerMap = std::make_unique<LoggerMap>(m_hazardDomain.get());
                if (!m_loggerMap)
                {
                    m_state.store(LogManagerState::Uninitialized, std::memory_order_release);
                    throw std::runtime_error("Failed to create logger map");
                }

                const size_t threadCount = std::max<size_t>(1, std::thread::hardware_concurrency() / 2);
                auto* threadPool = new(std::nothrow) LoggerThreadPool(threadCount);
                if (!threadPool)
                {
                    m_state.store(LogManagerState::Uninitialized, std::memory_order_release);
                    throw std::runtime_error("Failed to create thread pool");
                }

                m_threadPool.store(threadPool, std::memory_order_release);

                CreateDefaultLogger();

                m_state.store(LogManagerState::Running, std::memory_order_release);
            }
            catch (const std::exception& e)
            {
                m_state.store(LogManagerState::Uninitialized, std::memory_order_release);
                ShutdownAll();
                initResult = Err<void>(1, std::string("LogManager initialization failed: ") + e.what());
            }
        });
    }
    catch (const std::system_error& e)
    {
        return Err<void>(2, std::string("LogManager initialization error in call_once: ") + e.what());
    }

    return initResult;
}

FlexLog::Result<void> FlexLog::LogManager::Shutdown(bool waitForCompletion, std::chrono::milliseconds timeout)
{
    LogManagerState expectedState = LogManagerState::Running;
    if (!m_state.compare_exchange_strong(expectedState, LogManagerState::ShuttingDown, std::memory_order_acq_rel))
    {
        LogManagerState currentState = m_state.load(std::memory_order_acquire);

        if (currentState == LogManagerState::Uninitialized || currentState == LogManagerState::ShutDown)
            return Err<void>(1, std::string("LogManager not initialized or already shut down, current state: ") + GetStateName(currentState));
        else if (currentState == LogManagerState::Initializing)
            return Err<void>(1, "Cannot shutdown LogManager while it's being initialized");
        else if (currentState == LogManagerState::ShuttingDown)
            return Err<void>(1, "LogManager is already shutting down");
    }

    try
    {
        if (waitForCompletion)
        {
            auto* threadPool = m_threadPool.load(std::memory_order_acquire);
            if (threadPool)
                threadPool->Flush(timeout);
        }

        auto* threadPool = m_threadPool.exchange(nullptr, std::memory_order_acq_rel);
        if (threadPool)
        {
            try
            {
                threadPool->Shutdown(waitForCompletion, timeout);
            }
            catch (...)
            {
                // Ensure we still delete the thread pool even if shutdown throws
            }
            delete threadPool;
        }

        m_loggerMap->Clear();
        m_globalSinks.Clear();

        m_state.store(LogManagerState::ShutDown, std::memory_order_release);

        return Ok();
    }
    catch (const std::exception& e)
    {
        // On failure, try to restore Running state
        // Note: This is a best-effort approach; the manager might be in an inconsistent state
        m_state.store(LogManagerState::Running, std::memory_order_release);
        return Err<void>(2, std::string("LogManager shutdown failed: ") + e.what());
    }
}

FlexLog::Logger& FlexLog::LogManager::RegisterLogger(std::string_view name)
{
    LogManagerState currentState = m_state.load(std::memory_order_acquire);
    if (currentState != LogManagerState::Running || name.empty())
        throw std::runtime_error(std::string("Cannot register logger: ") + (name.empty() ? "Logger name cannot be empty" : std::string("LogManager in state ") + GetStateName(currentState)));

    auto existing = m_loggerMap->Find(name);
    if (existing)
        return existing->get();

    EnsureThreadPoolInitialized();

    auto logger = std::make_unique<Logger>(std::string(name), m_defaultLevel.load(std::memory_order_acquire));
    logger->GetFormat().SetLogFormat(m_defaultFormat.load(std::memory_order_acquire));

    auto sinkHandle = m_globalSinks.GetReadHandle();
    for (const auto& sink : sinkHandle.Items())
    {
        logger->RegisterSink(sink);
    }

    return m_loggerMap->Insert(std::string(name), std::move(logger));
}

FlexLog::Logger& FlexLog::LogManager::GetLogger(std::string_view name)
{
    auto logger = m_loggerMap->Find(name);
    if (!logger)
        logger = RegisterLogger(name);

    return logger->get();
}

FlexLog::Logger& FlexLog::LogManager::GetDefaultLogger()
{
    std::string defaultName = m_defaultLoggerName->Load();
    return GetLogger(defaultName);
}

bool FlexLog::LogManager::HasLogger(std::string_view name) const
{
    if (m_state.load(std::memory_order_acquire) != LogManagerState::Running || name.empty() || !m_loggerMap)
        return false;
    return m_loggerMap->Contains(name);
}

void FlexLog::LogManager::RemoveLogger(std::string_view name)
{
    // Don't allow removing the default logger
    if (m_state.load(std::memory_order_acquire) != LogManagerState::Running || m_defaultLoggerName->Compare(name))
        return;

    m_loggerMap->Remove(name);
}

void FlexLog::LogManager::RegisterSink(std::shared_ptr<Sink> sink)
{
    if (m_state.load(std::memory_order_acquire) != LogManagerState::Running || !sink)
        return;

    // Add to global sinks list
    m_globalSinks.Add(sink);

    // Add to all existing loggers
    // We need a separate way to iterate all loggers safely
    // For now, we'll skip adding to existing loggers (they can be updated manually if needed)
}

void FlexLog::LogManager::SetDefaultLevel(Level level)
{
    if (m_state.load(std::memory_order_acquire) != LogManagerState::Running)
        return;
    m_defaultLevel.store(level, std::memory_order_release);
    m_configVersion.fetch_add(1, std::memory_order_release);
}

FlexLog::Level FlexLog::LogManager::GetDefaultLevel() const
{
    return m_defaultLevel.load(std::memory_order_acquire);
}

void FlexLog::LogManager::SetDefaultFormat(LogFormat formatInfo)
{
    if (m_state.load(std::memory_order_acquire) != LogManagerState::Running)
        return;
    m_defaultFormat.store(formatInfo, std::memory_order_release);
    m_configVersion.fetch_add(1, std::memory_order_release);
}

void FlexLog::LogManager::SetThreadPoolSize(size_t size)
{
    if (m_state.load(std::memory_order_acquire) != LogManagerState::Running)
        return;

    auto* threadPool = m_threadPool.load(std::memory_order_acquire);
    if (threadPool)
    {
        threadPool->Resize(size);
    }
    else
    {
        threadPool = new LoggerThreadPool(size);

        LoggerThreadPool* expected = nullptr;
        if (!m_threadPool.compare_exchange_strong(expected, threadPool, std::memory_order_release, std::memory_order_acquire))
        {
            // Another thread initialized the pool, use that one
            delete threadPool;
        }
    }
}

size_t FlexLog::LogManager::GetThreadPoolSize() const
{
    auto* threadPool = m_threadPool.load(std::memory_order_acquire);
    if (threadPool)
        return threadPool->GetThreadCount();
    return 0;
}

bool FlexLog::LogManager::ResizeThreadPool(size_t newSize)
{
    if (m_state.load(std::memory_order_acquire) != LogManagerState::Running)
        return false;

    auto* threadPool = m_threadPool.load(std::memory_order_acquire);
    if (!threadPool)
    {
        threadPool = new LoggerThreadPool(newSize);

        LoggerThreadPool* expected = nullptr;
        if (!m_threadPool.compare_exchange_strong(expected, threadPool, std::memory_order_release, std::memory_order_acquire))
        {
            // Another thread initialized the pool, resize that one
            auto* existingPool = m_threadPool.load(std::memory_order_acquire);
            delete threadPool;
            return existingPool->Resize(newSize);
        }
        return true;
    }

    return threadPool->Resize(newSize);
}

FlexLog::LoggerThreadPool& FlexLog::LogManager::GetThreadPool()
{
    EnsureThreadPoolInitialized();
    return *m_threadPool.load(std::memory_order_acquire);
}

FlexLog::MessagePool& FlexLog::LogManager::GetMessagePool()
{
    EnsureMessagePoolInitialized();
    return *m_messagePool;
}

void FlexLog::LogManager::ShutdownAll()
{
    LogManagerState currentState = m_state.load(std::memory_order_acquire);
    if (currentState == LogManagerState::ShutDown)
        return;

    LogManagerState expectedState = LogManagerState::Running;
    m_state.compare_exchange_strong(expectedState, LogManagerState::ShuttingDown, std::memory_order_acq_rel);

    auto* threadPool = m_threadPool.exchange(nullptr, std::memory_order_acquire);
    if (threadPool)
    {
        threadPool->Shutdown(true);
        delete threadPool;
    }

    m_loggerMap->Clear();
    m_globalSinks.Clear();

    m_state.store(LogManagerState::ShutDown, std::memory_order_release);
}

void FlexLog::LogManager::ResetAll()
{
    LogManagerState currentState = m_state.load(std::memory_order_acquire);
    if (currentState != LogManagerState::Running)
        return;

    ShutdownAll();
    m_state.store(LogManagerState::Uninitialized, std::memory_order_release);
    Initialize();
}

void FlexLog::LogManager::SetDefaultLoggerName(std::string_view name)
{
    if (m_state.load(std::memory_order_acquire) != LogManagerState::Running || name.empty())
        return;

    m_defaultLoggerName->Store(name);

    if (!HasLogger(name))
        CreateDefaultLogger();
}

std::string FlexLog::LogManager::GetDefaultLoggerName() const
{
    return m_defaultLoggerName->Load();
}

bool FlexLog::LogManager::CheckAndTransitionState(LogManagerState expectedState, LogManagerState newState)
{
    LogManagerState currentState = m_state.load(std::memory_order_acquire);
    if (currentState != expectedState)
        return false;

    return m_state.compare_exchange_strong(currentState, newState, std::memory_order_acq_rel);
}

const char* FlexLog::LogManager::GetStateName(LogManagerState state) const
{
    switch (state)
    {
    case LogManagerState::Uninitialized: return "Uninitialized";
    case LogManagerState::Initializing:  return "Initializing";
    case LogManagerState::Running:       return "Running";
    case LogManagerState::ShuttingDown:  return "ShuttingDown";
    case LogManagerState::ShutDown:      return "ShutDown";
    default:                             return "Unknown";
    }
}

void FlexLog::LogManager::CreateDefaultLogger()
{
    std::string name = m_defaultLoggerName->Load();

    auto logger = std::make_unique<Logger>(name, m_defaultLevel.load(std::memory_order_acquire));
    logger->SetLevel(m_defaultLevel.load(std::memory_order_acquire));
    logger->EmplaceSink<ConsoleSink>();
    logger->GetFormat().SetLogFormat(m_defaultFormat.load(std::memory_order_acquire));

    // Insert into map (will replace any existing logger with same name)
    m_loggerMap->Insert(name, std::move(logger));
}

void FlexLog::LogManager::EnsureThreadPoolInitialized()
{
    LogManagerState currentState = m_state.load(std::memory_order_acquire);
    if (currentState != LogManagerState::Initializing && currentState != LogManagerState::Running)
        return;

    auto* threadPool = m_threadPool.load(std::memory_order_acquire);
    if (!threadPool)
    {
        auto* newPool = new LoggerThreadPool(std::max<size_t>(1, std::thread::hardware_concurrency() / 2));

        LoggerThreadPool* expected = nullptr;
        if (!m_threadPool.compare_exchange_strong(expected, newPool, std::memory_order_release, std::memory_order_acquire))
        {
            // Another thread created the pool
            delete newPool;
        }
    }
}

void FlexLog::LogManager::EnsureMessagePoolInitialized()
{
    LogManagerState currentState = m_state.load(std::memory_order_acquire);
    if (currentState == LogManagerState::ShutDown)
        throw std::runtime_error(std::string("Cannot access message pool: LogManager in state ") + GetStateName(currentState));

    if (!m_messagePool)
    {
        m_messagePool = std::make_unique<MessagePool>();
        if (!m_messagePool)
            throw std::runtime_error("Failed to create message pool");
    }
}
