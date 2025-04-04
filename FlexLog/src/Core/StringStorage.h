#pragma once

#include <cstring>
#include <string_view>

namespace FlexLog
{
    /**
    * @brief Efficient storage container for log message strings.
    * 
    * Uses Small String Optimization (SSO) to avoid heap allocations for short strings.
    * Thread-safe for move operations and provides memory visibility guarantees.
    */
    class StringStorage
    {
    public:
        StringStorage();
        StringStorage(StringStorage&& other) noexcept;
        StringStorage(const StringStorage&) = delete;
        ~StringStorage();
        
        StringStorage& operator=(StringStorage&& other) noexcept;
        StringStorage& operator=(const StringStorage&) = delete;
                
        static StringStorage Create(std::string_view str);
        
        std::string_view View() const { return m_view; }

        bool IsInline() const { return m_view.data() >= m_inlineBuffer && m_view.data() < m_inlineBuffer + INLINE_CAPACITY; }
        
    private:
        void Store(std::string_view str);
        
        static constexpr size_t INLINE_CAPACITY = 64;
        alignas(8) char m_inlineBuffer[INLINE_CAPACITY];
        char* m_heapBuffer;
        std::string_view m_view;
    };
}
