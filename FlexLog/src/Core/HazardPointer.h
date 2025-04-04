#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace FlexLog
{
    /**
    * @brief Memory management domain for lock-free data structures.
    * 
    * Provides safe memory reclamation through hazard pointers, allowing
    * nodes to be safely deleted after they are no longer in use by any thread.
    */
    class HazardPointerDomain
    {
    public:
        HazardPointerDomain();
        ~HazardPointerDomain();

        void* ProtectPointer(void* ptr, size_t& hpIndex);
        void UnprotectPointer(size_t hpIndex);

        // Retire a node for later deletion
        template<typename T>
        void RetireNode(T* node)
        {
            RetireNode(static_cast<void*>(node), [](void* p)
            {
                delete static_cast<T*>(p);
            });
        }

        void RetireNode(void* ptr, std::function<void(void*)> deleter);

        // Try to clean up retired nodes
        void TryCleanup();

    private:
        static constexpr size_t MAX_HAZARD_POINTERS = 100;
        static constexpr size_t SCAN_THRESHOLD = 1000;

        struct HazardPointerRecord
        {
            std::atomic<std::thread::id> threadID;
            std::atomic<void*> pointer;

            HazardPointerRecord()
            {
                threadID.store(std::thread::id(), std::memory_order_relaxed);
                pointer.store(nullptr, std::memory_order_relaxed);
            }
        };

        HazardPointerRecord m_hazardPointers[MAX_HAZARD_POINTERS];

        struct RetiredNode
        {
            void* pointer;
            std::function<void(void*)> deleter;
            size_t retireEpoch;
            RetiredNode* next;
        };

        std::atomic<RetiredNode*> m_retiredList{nullptr};
        std::atomic<size_t> m_retireEpoch{0};
        std::atomic<size_t> m_retiredCount{0};
    };

    /**
    * @brief RAII wrapper for hazard pointers.
    * 
    * Provides automatic management of hazard pointer acquisition and release
    * with proper memory ordering semantics.
    */
    class HazardPointer 
    {
    public:
        explicit HazardPointer(HazardPointerDomain* domain);
        ~HazardPointer();

        template<typename T>
        T* Protect(T* ptr)
        {
            if (!ptr)
                return nullptr;  // Don't protect null pointers

            m_domain->ProtectPointer(static_cast<void*>(ptr), m_index);
            m_active = true;

            // Memory barrier to ensure the pointer is visible before dereferencing
            std::atomic_thread_fence(std::memory_order_seq_cst);

            return ptr;
        }

        void Reset();

        HazardPointer(const HazardPointer&) = delete;
        HazardPointer& operator=(const HazardPointer&) = delete;

        HazardPointer(HazardPointer&& other) noexcept;
        HazardPointer& operator=(HazardPointer&& other) noexcept;

    private:
        HazardPointerDomain* m_domain;
        size_t m_index;
        bool m_active;
    };
}
