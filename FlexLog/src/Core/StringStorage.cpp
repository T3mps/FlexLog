#include "StringStorage.h"

#include <atomic>

FlexLog::StringStorage::StringStorage() :
    m_inlineBuffer({0}),
    m_heapBuffer(nullptr),
    m_view()
{
    std::memset(m_inlineBuffer, 0, INLINE_CAPACITY);
}

FlexLog::StringStorage::StringStorage(StringStorage&& other) noexcept : m_heapBuffer(other.m_heapBuffer)
{
    if (other.IsInline())
    {
        // For inline strings, we need to copy the content
        // Use memcpy with explicit size for efficiency
        std::memcpy(m_inlineBuffer, other.m_inlineBuffer, INLINE_CAPACITY);

        // Create view pointing to our copy
        size_t size = other.m_view.size();
        m_view = std::string_view(m_inlineBuffer, size);
    }
    else
    {
        // For heap strings, just take ownership of the buffer and view
        m_view = other.m_view;
    }

    // Clear the source object to prevent double-free
    other.m_heapBuffer = nullptr;
    other.m_view = std::string_view();
}

FlexLog::StringStorage::~StringStorage() 
{
    if (m_heapBuffer) 
        delete[] m_heapBuffer;
}

FlexLog::StringStorage& FlexLog::StringStorage::operator=(StringStorage&& other) noexcept 
{
    if (this != &other) 
    {
        this->~StringStorage();
        new (this) StringStorage(std::move(other));
    }
    return *this;
}

FlexLog::StringStorage FlexLog::StringStorage::Create(std::string_view str) 
{
    StringStorage storage;
    storage.Store(str);
    return storage;
}

void FlexLog::StringStorage::Store(std::string_view str) 
{
    if (str.size() < INLINE_CAPACITY) 
    {
        // String fits in the inline buffer - use SSO
        std::memcpy(m_inlineBuffer, str.data(), str.size());

        // Ensure null-termination for safety (not strictly required for string_view)
        if (str.size() < INLINE_CAPACITY - 1)
            m_inlineBuffer[str.size()] = '\0';

        m_view = std::string_view(m_inlineBuffer, str.size());
    } 
    else 
    {
        // String is too large for inline (stack) storage - use heap
        m_heapBuffer = new char[str.size()];
        std::memcpy(m_heapBuffer, str.data(), str.size());
        m_view = std::string_view(m_heapBuffer, str.size());
    }
}
