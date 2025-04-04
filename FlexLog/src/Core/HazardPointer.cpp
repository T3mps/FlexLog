#include "HazardPointer.h"

FlexLog::HazardPointerDomain::HazardPointerDomain()
{
    for (size_t i = 0; i < MAX_HAZARD_POINTERS; ++i)
    {
        m_hazardPointers[i].threadID.store(std::thread::id(), std::memory_order_relaxed);
        m_hazardPointers[i].pointer.store(nullptr, std::memory_order_relaxed);
    }
}

FlexLog::HazardPointerDomain::~HazardPointerDomain()
{
    // Clean up all retired nodes regardless of hazard pointers
    RetiredNode* nodes = m_retiredList.exchange(nullptr, std::memory_order_acquire);
    while (nodes)
    {
        RetiredNode* next = nodes->next;
        nodes->deleter(nodes->pointer);
        delete nodes;
        nodes = next;
    }
}

void* FlexLog::HazardPointerDomain::ProtectPointer(void* ptr, size_t& hpIndex)
{
    if (!ptr)
        return nullptr;  // Don't protect null pointers

    std::thread::id currentID = std::this_thread::get_id();
    std::thread::id defaultID; // Default constructed thread ID represents "no thread"

    // First try to reuse a slot owned by this thread
    for (size_t i = 0; i < MAX_HAZARD_POINTERS; ++i)
    {
        // Check if this slot is already owned by our thread
        if (m_hazardPointers[i].threadID.load(std::memory_order_acquire) == currentID)
        {
            hpIndex = i;
            m_hazardPointers[i].pointer.store(ptr, std::memory_order_release);
            return ptr;
        }

        // Try to claim an unowned slot
        std::thread::id expectedID = defaultID;
        if (m_hazardPointers[i].threadID.compare_exchange_strong(expectedID, currentID, std::memory_order_acquire, std::memory_order_relaxed))
        {
            hpIndex = i;
            m_hazardPointers[i].pointer.store(ptr, std::memory_order_release);
            return ptr;
        }
    }

    // Couldn't find a slot - this shouldn't happen with appropriate MAX_HAZARD_POINTERS
    throw std::runtime_error("Out of hazard pointers");
}

void FlexLog::HazardPointerDomain::UnprotectPointer(size_t hpIndex)
{
    m_hazardPointers[hpIndex].pointer.store(nullptr, std::memory_order_release);
}

void FlexLog::HazardPointerDomain::RetireNode(void* ptr, std::function<void(void*)> deleter)
{
    RetiredNode* retiredNode = new RetiredNode
    {
        ptr,
        std::move(deleter),
        m_retireEpoch.fetch_add(1, std::memory_order_relaxed),
        nullptr
    };

    // Add to retired list
    RetiredNode* oldHead = m_retiredList.load(std::memory_order_relaxed);
    do
    {
        retiredNode->next = oldHead;
    }
    while (!m_retiredList.compare_exchange_weak(oldHead, retiredNode, std::memory_order_release, std::memory_order_relaxed));

    // Attempt cleanup if we've accumulated enough nodes
    if (m_retiredCount.fetch_add(1, std::memory_order_relaxed) >= SCAN_THRESHOLD)
        TryCleanup();
}

void FlexLog::HazardPointerDomain::TryCleanup()
{
    // Reset retired count regardless of success to avoid constant cleaning attempts
    m_retiredCount.store(0, std::memory_order_relaxed);

    // Collect active hazard pointers
    std::vector<void*> hpList;
    for (size_t i = 0; i < MAX_HAZARD_POINTERS; ++i)
    {
        void* hp = m_hazardPointers[i].pointer.load(std::memory_order_acquire);
        if (hp)
            hpList.push_back(hp);
    }

    std::sort(hpList.begin(), hpList.end());

    // Extract the entire retired list
    RetiredNode* nodes = m_retiredList.exchange(nullptr, std::memory_order_acquire);
    if (!nodes)
        return;

    RetiredNode* deferred = nullptr;
    RetiredNode* toDelete = nullptr;

    // Process each node in the retired list
    while (nodes)
    {
        RetiredNode* current = nodes;
        nodes = nodes->next;

        // Check if current node is pointed to by a hazard pointer
        if (std::binary_search(hpList.begin(), hpList.end(), current->pointer))
        {
            // Some thread is still accessing this pointer, keep in the retired list
            current->next = deferred;
            deferred = current;
        }
        else
        {
            // No thread is accessing this pointer, safe to delete
            current->next = toDelete;
            toDelete = current;
        }
    }

    // Re-add deferred nodes to the retired list
    if (deferred)
    {
        RetiredNode* oldHead = m_retiredList.load(std::memory_order_relaxed);
        do
        {
            // Add all deferred nodes back to the list
            RetiredNode* last = deferred;
            while (last->next)
            {
                last = last->next;
            }
            last->next = oldHead;
        }
        while (!m_retiredList.compare_exchange_weak(oldHead, deferred, std::memory_order_release, std::memory_order_relaxed)) /* spin */;

        // Update the count of retired nodes
        m_retiredCount.fetch_add(1, std::memory_order_relaxed);
    }

    // Delete nodes that are safe to reclaim
    while (toDelete)
    {
        RetiredNode* current = toDelete;
        toDelete = toDelete->next;
        current->deleter(current->pointer);
        delete current;
    }
}

FlexLog::HazardPointer::HazardPointer(HazardPointerDomain* domain) : m_domain(domain), m_index(0), m_active(false) {}

FlexLog::HazardPointer::~HazardPointer()
{
    if (m_active)
        m_domain->UnprotectPointer(m_index);
}

void FlexLog::HazardPointer::Reset()
{
    if (m_active)
    {
        m_domain->UnprotectPointer(m_index);
        m_active = false;
    }
}

FlexLog::HazardPointer::HazardPointer(HazardPointer&& other) noexcept : m_domain(other.m_domain), m_index(other.m_index), m_active(other.m_active)
{
    other.m_active = false;
}

FlexLog::HazardPointer& FlexLog::HazardPointer::operator=(HazardPointer&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_domain = other.m_domain;
        m_index = other.m_index;
        m_active = other.m_active;
        other.m_active = false;
    }
    return *this;
}
