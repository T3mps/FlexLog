#pragma once

#include <atomic>
#include <memory>
#include <span>
#include <vector>

#include "Core/HazardPointer.h"

namespace FlexLog
{
    template<typename T>
    class RCUList
    {
    public:
        RCUList(HazardPointerDomain* domain = nullptr) : m_hazardDomain(domain)
        {
            if (!m_hazardDomain)
                m_ownedDomain = std::make_unique<HazardPointerDomain>();
        }

        ~RCUList()
        {
            Clear();
        }

        RCUList(const RCUList&) = delete;
        RCUList& operator=(const RCUList&) = delete;

        RCUList(RCUList&& other) noexcept : 
            m_head(other.m_head.exchange(nullptr, std::memory_order_acquire)),
            m_hazardDomain(other.m_hazardDomain),
            m_ownedDomain(std::move(other.m_ownedDomain))
        {}

        RCUList& operator=(RCUList&& other) noexcept
        {
            if (this != &other)
            {
                // Clean up existing resources
                Clear();

                m_head.store(other.m_head.exchange(nullptr, std::memory_order_acquire), std::memory_order_release);
                m_hazardDomain = other.m_hazardDomain;
                m_ownedDomain = std::move(other.m_ownedDomain);
            }
            return *this;
        }

        // Thread-safe accessor for reading
        class ReadHandle
        {
        public:
            explicit ReadHandle(RCUList& list) : m_list(&list)
            {
                m_node = nullptr;

                HazardPointerDomain* domain = list.GetHazardDomain();
                m_hp = std::make_unique<HazardPointer>(domain);

                Node* head = list.m_head.load(std::memory_order_acquire);
                if (head)
                    m_node = m_hp->Protect(head);
            }

            ~ReadHandle() = default;

            ReadHandle(const ReadHandle&) = delete;
            ReadHandle& operator=(const ReadHandle&) = delete;

            ReadHandle(ReadHandle&& other) noexcept :
                m_list(other.m_list),
                m_node(other.m_node),
                m_hp(std::move(other.m_hp))
            {
                other.m_list = nullptr;
                other.m_node = nullptr;
            }

            ReadHandle& operator=(ReadHandle&& other) noexcept
            {
                if (this != &other)
                {
                    m_list = other.m_list;
                    m_node = other.m_node;
                    m_hp = std::move(other.m_hp);

                    other.m_list = nullptr;
                    other.m_node = nullptr;
                }
                return *this;
            }

            std::span<const T> Items() const
            {
                if (m_node)
                    return std::span<const T>(m_node->items);
                return std::span<const T>();
            }

            size_t Size() const { return m_node ? m_node->items.size() : 0; }
            bool Empty() const { return !m_node || m_node->items.empty(); }

        private:
            RCUList* m_list;
            typename RCUList::Node* m_node;
            std::unique_ptr<HazardPointer> m_hp;
        };

        void Add(T item)
        {
            Node* oldHead = m_head.load(std::memory_order_acquire);
            Node* newHead;

            do
            {
                newHead = new Node(oldHead ? oldHead->items : std::vector<T>());
                newHead->items.push_back(std::move(item));

            } while (!m_head.compare_exchange_weak(oldHead, newHead, std::memory_order_release, std::memory_order_acquire));

            if (oldHead)
                RetireNode(oldHead);
        }

        void AddRange(std::span<const T> items)
        {
            if (items.empty())
                return;

            Node* oldHead = m_head.load(std::memory_order_acquire);
            Node* newHead;

            do
            {
                newHead = new Node(oldHead ? oldHead->items : std::vector<T>());
                newHead->items.reserve(newHead->items.size() + items.size());

                for (const auto& item : items)
                {
                    newHead->items.push_back(item);
                }

            } while (!m_head.compare_exchange_weak(oldHead, newHead, std::memory_order_release, std::memory_order_acquire));

            if (oldHead)
                RetireNode(oldHead);
        }

        bool Remove(const T& item)
        {
            Node* oldHead = m_head.load(std::memory_order_acquire);
            if (!oldHead || oldHead->items.empty())
                return false;

            bool found = false;
            for (const auto& existingItem : oldHead->items)
            {
                if (existingItem == item)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                return false;

            Node* newHead;

            do
            {
                newHead = new Node();
                newHead->items.reserve(oldHead->items.size() - 1);

                for (const auto& existingItem : oldHead->items)
                {
                    if (!(existingItem == item))
                        newHead->items.push_back(existingItem);
                }

            } while (!m_head.compare_exchange_weak(oldHead, newHead, std::memory_order_release, std::memory_order_acquire));

            RetireNode(oldHead);
            return true;
        }

        void Clear()
        {
            Node* oldHead = m_head.exchange(nullptr, std::memory_order_acquire);
            if (oldHead)
                RetireNode(oldHead);
        }

        [[nodiscard]] ReadHandle GetReadHandle() { return ReadHandle(*this); }

        size_t EstimatedSize() const
        {
            Node* current = m_head.load(std::memory_order_acquire);
            return current ? current->items.size() : 0;
        }

    private:
        struct Node 
        {
            std::vector<T> items;

            explicit Node(std::vector<T> data = {}) : items(std::move(data)) {}
            Node(const Node& other) : items(other.items) {}
        };

        void RetireNode(Node* node) { GetHazardDomain()->RetireNode(node); }

        HazardPointerDomain* GetHazardDomain() { return m_hazardDomain ? m_hazardDomain : m_ownedDomain.get(); }

        std::atomic<Node*> m_head{nullptr};
        HazardPointerDomain* m_hazardDomain{nullptr};
        std::unique_ptr<HazardPointerDomain> m_ownedDomain;
    };
}
