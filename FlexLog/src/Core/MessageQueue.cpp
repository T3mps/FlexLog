#include "MessageQueue.h"

FlexLog::MessageQueue::MessageQueue(size_t capacity) :
    m_capacity(RoundUpPow2(capacity == 0 ? DEFAULT_CAPACITY : capacity))
{
    // Allocate slots with proper alignment
    m_slots = std::make_unique<Slot[]>(m_capacity);

    // Initialize sequence numbers
    // Consumers expect slot.sequence == position
    // Producers expect slot.sequence == position + 1
    for (size_t i = 0; i < m_capacity; ++i)
    {
        m_slots[i].sequence.store(i, std::memory_order_relaxed);
    }
}

FlexLog::MessageQueue::~MessageQueue() 
{
    // Clean up any remaining messages in the queue
    size_t head = m_consumerIndex.load(std::memory_order_relaxed);
    size_t tail = m_producerIndex.load(std::memory_order_relaxed);

    for (size_t i = head; i != tail; i++)
    {
        size_t index = i & IndexMask();
        Message* msg = m_slots[index].message;
        if (msg)
        {
            // For messages with reference counting, we'd decrement references here
            // In this implementation, MessagePool handles the cleanup
        }
    }
}

bool FlexLog::MessageQueue::TryEnqueue(Message* message)
{
    if (!message)
        return false;

    // Reserve a slot by reading the current producer index
    // memory_order_relaxed is sufficient here since we'll verify with the sequence
    const size_t positionToPublish = m_producerIndex.load(std::memory_order_relaxed);
    const size_t index = positionToPublish & IndexMask();
    Slot& slot = m_slots[index];

    // Check if the slot is available for writing
    // The sequence will equal the position if it's available
    // memory_order_acquire ensures we see up-to-date sequence value
    const size_t sequenceExpected = positionToPublish;
    if (slot.sequence.load(std::memory_order_acquire) != sequenceExpected)
    {
        // Queue is full
        return false;
    }

    // Try to atomically reserve the slot
    // memory_order_seq_cst provides the strongest ordering guarantee
    // This ensures the producer index increment is visible to all threads
    if (!m_producerIndex.compare_exchange_strong(
        const_cast<size_t&>(positionToPublish), 
        positionToPublish + 1, 
        std::memory_order_seq_cst))
    {
        // Another thread reserved this slot first
        return false;
    }

    // We now own the slot, write the message
    // This is a non-atomic store, but it's safe because we own the slot
    slot.message = message;

    // Make the slot available for consumption
    // Set sequence to position + 1 to indicate the slot is ready for reading
    // memory_order_release ensures the message store is visible before sequence update
    slot.sequence.store(positionToPublish + 1, std::memory_order_release);

    // Update peak usage statistics (relaxed ordering is sufficient for statistics)
    size_t currentSize = Size();
    size_t peak = m_peakUsage.load(std::memory_order_relaxed);
    if (currentSize > peak)
        m_peakUsage.store(currentSize, std::memory_order_relaxed);

    return true;
}

FlexLog::Message* FlexLog::MessageQueue::TryDequeue()
{
    // Get the current consumer position
    // memory_order_relaxed is sufficient here since we'll verify with the sequence
    const size_t positionToConsume = m_consumerIndex.load(std::memory_order_relaxed);
    const size_t index = positionToConsume & IndexMask();
    Slot& slot = m_slots[index];

    // Check if the slot is available for reading
    // The sequence will equal position + 1 if it's ready for consumption
    // memory_order_acquire ensures we see the updated sequence value and message
    const size_t sequenceExpected = positionToConsume + 1;
    if (slot.sequence.load(std::memory_order_acquire) != sequenceExpected)
    {
        // Queue is empty or this slot isn't ready
        return nullptr;
    }

    // Try to atomically reserve the slot for consumption
    // memory_order_seq_cst provides the strongest ordering guarantee
    // This ensures the consumer index increment is visible to all threads
    if (!m_consumerIndex.compare_exchange_strong(
        const_cast<size_t&>(positionToConsume),
        positionToConsume + 1,
        std::memory_order_seq_cst))
    {
        // Another thread consumed this slot first
        return nullptr;
    }

    // We now own the slot, read the message
    Message* message = slot.message;
    slot.message = nullptr;

    // Make the slot available for production again
    // The next expected sequence is position + capacity
    // memory_order_release ensures the message read is completed before the slot is reused
    slot.sequence.store(positionToConsume + m_capacity, std::memory_order_release);

    return message;
}

size_t FlexLog::MessageQueue::DequeueAll(std::vector<Message*>& out)
{
    size_t count = 0;
    Message* msg = nullptr;

    // Reserve space to avoid reallocation
    const size_t initialSize = out.size();
    const size_t approximateSize = Size();
    if (approximateSize > 0)
        out.reserve(initialSize + approximateSize);

    // Dequeue all available messages
    while ((msg = TryDequeue()) != nullptr)
    {
        out.push_back(msg);
        ++count;
    }

    return count;
}

bool FlexLog::MessageQueue::IsEmpty() const
{
    // memory_order_acquire ensures we see the most recent updates to indices
    const size_t producerPos = m_producerIndex.load(std::memory_order_acquire);
    const size_t consumerPos = m_consumerIndex.load(std::memory_order_acquire);

    return producerPos == consumerPos;
}

size_t FlexLog::MessageQueue::Size() const
{
    // Using memory_order_acquire ensures we get up-to-date values
    const size_t producerPos = m_producerIndex.load(std::memory_order_acquire);
    const size_t consumerPos = m_consumerIndex.load(std::memory_order_acquire);

    // Calculate the difference, accounting for potential wrap-around
    return producerPos >= consumerPos ? producerPos - consumerPos : m_capacity - (consumerPos - producerPos);
}

float FlexLog::MessageQueue::UsagePercentage() const
{
    const size_t currentSize = Size();
    return static_cast<float>(currentSize) / m_capacity * 100.0f;
}

size_t FlexLog::MessageQueue::GetPeakUsage() const
{
    return m_peakUsage.load(std::memory_order_relaxed);
}

void FlexLog::MessageQueue::ResetPeakUsage()
{
    m_peakUsage.store(0, std::memory_order_relaxed);
}
