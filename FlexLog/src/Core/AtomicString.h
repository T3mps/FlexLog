#pragma once

#include <atomic>
#include <string>
#include <string_view>

namespace FlexLog
{
    /**
    * @brief Thread-safe string storage with atomic access.
    * 
    * Provides a fixed-capacity string that can be safely accessed
    * and modified across multiple threads without locks.
    */
    class AtomicString 
    {
    public:
        static constexpr size_t MAX_LENGTH = 128;

        AtomicString();
        explicit AtomicString(std::string_view str);
        ~AtomicString() = default;

        void Store(std::string_view str);
        std::string Load() const;

        bool Compare(std::string_view str) const;

    private:
        char m_buffer[MAX_LENGTH];
        std::atomic<size_t> m_length{0};
    };
}
