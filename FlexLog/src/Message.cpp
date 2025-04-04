#include "Message.h"

#include "Core/MessagePool.h"
#include "Logger.h"
#include "LogManager.h"

FlexLog::MessageRef::MessageRef(Message* message) : m_message(message)
{
    if (m_message)
        m_message->AddRef();
}

FlexLog::MessageRef::MessageRef(const MessageRef& other) : m_message(other.m_message)
{
    if (m_message)
        m_message->AddRef();
}

FlexLog::MessageRef::MessageRef(MessageRef&& other) noexcept : m_message(other.m_message)
{
    other.m_message = nullptr;
}

FlexLog::MessageRef::~MessageRef()
{
    Reset();
}

FlexLog::MessageRef& FlexLog::MessageRef::operator=(const MessageRef& other)
{
    if (this != &other)
    {
        Reset();
        m_message = other.m_message;
        if (m_message)
            m_message->AddRef();
    }
    return *this;
}

FlexLog::MessageRef& FlexLog::MessageRef::operator=(MessageRef&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_message = other.m_message;
        other.m_message = nullptr;
    }
    return *this;
}

void FlexLog::MessageRef::Reset()
{
    if (m_message)
    {
        // If this was the last reference and the message is in releasing state,
        // finalize the release
        if (m_message->ReleaseRef() && m_message->state.load(std::memory_order_acquire) == MessageState::Releasing)
            LogManager::GetInstance().GetMessagePool().FinalizeRelease(m_message);
        m_message = nullptr;
    }
}
