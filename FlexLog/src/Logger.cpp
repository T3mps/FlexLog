#include "Logger.h"

#include <chrono>

#include "Core/LoggerThreadPool.h"
#include "Core/MessagePool.h"
#include "LogManager.h"
#include "Message.h"
#include "Sink/Sink.h"

FlexLog::Logger::Logger(std::string name, Level level) :
    m_name(std::move(name)),
    m_level(level)
{}

bool FlexLog::Logger::Log(std::string_view msg, Level level, const std::source_location& location)
{
    if (msg.empty() || level < m_level || level >= Level::Off)
        return false;

    Message* logMessage = CreateMessage(msg, level, location);
    if (!logMessage)
        return false;

    EnqueueMessage(logMessage);
    return true;
}

bool FlexLog::Logger::Log(std::string_view msg, const StructuredData& data, Level level, const std::source_location& location)
{
    if (level < m_level || level >= Level::Off)
        return false;

    Message* logMessage = CreateStructuredMessage(msg, data, level, location);
    if (!logMessage)
        return false;

    EnqueueMessage(logMessage);
    return true;
}

void FlexLog::Logger::Flush()
{
    auto handle = m_sinkList.GetReadHandle();
    for (const auto& sink : handle.Items())
    {
        if (sink)
            sink->Flush();
    }
}

void FlexLog::Logger::RegisterSink(std::shared_ptr<Sink> sink)
{
    if (sink)
        m_sinkList.Add(std::move(sink));
}

void FlexLog::Logger::RegisterSinks(std::span<std::shared_ptr<Sink>> sinks)
{
    if (!sinks.empty())
        m_sinkList.AddRange(sinks);
}

std::vector<std::shared_ptr<FlexLog::Sink>> FlexLog::Logger::GetSinks()
{
    auto handle = m_sinkList.GetReadHandle();
    return std::vector<std::shared_ptr<Sink>>(handle.Items().begin(), handle.Items().end());
}

FlexLog::Message* FlexLog::Logger::CreateMessage(std::string_view message, Level level, std::source_location location)
{
    Message* poolMessage = LogManager::GetInstance().GetMessagePool().Acquire();
    if (!poolMessage)
    {
        m_droppedMessages.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    poolMessage->timestamp = std::chrono::system_clock::now();
    poolMessage->name = m_name;
    poolMessage->level = level;
    poolMessage->sourceLocation = location;
    poolMessage->messageStorage = StringStorage::Create(message);
    poolMessage->message = poolMessage->messageStorage.View();
    poolMessage->logger = this;

    return poolMessage;
}

FlexLog::Message* FlexLog::Logger::CreateStructuredMessage(std::string_view message, const StructuredData& data, Level level, std::source_location location)
{
    Message* poolMessage = LogManager::GetInstance().GetMessagePool().Acquire();
    if (!poolMessage)
    {
        m_droppedMessages.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    poolMessage->timestamp = std::chrono::system_clock::now();
    poolMessage->name = m_name;
    poolMessage->level = level;
    poolMessage->sourceLocation = location;
    poolMessage->messageStorage = StringStorage::Create(message);
    poolMessage->message = poolMessage->messageStorage.View();
    poolMessage->logger = this;
    poolMessage->structuredData = data;  // Copy the structured data

    return poolMessage;
}

void FlexLog::Logger::EnqueueMessage(Message* message)
{
    if (!message)
        return;

    uint8_t priority = static_cast<uint8_t>(message->level);

    LogManager::GetInstance().GetThreadPool().EnqueueMessage(message, priority);
    m_totalProcessed.fetch_add(1, std::memory_order_relaxed);
}

void FlexLog::Logger::ProcessMessage(Message* logMessage)
{
    if (!logMessage || !logMessage->IsActive())
        return;

    auto handle = m_sinkList.GetReadHandle();

    for (const auto& sink : handle.Items())
    {
        if (sink)
            sink->Output(*logMessage, m_format);
    }

    LogManager::GetInstance().GetMessagePool().Release(logMessage);
}
