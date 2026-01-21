#pragma once
#include "allocator.h"

namespace VoidEngine
{

    class FreeListAllocator : public Allocator
    {
    private:
        struct Header
        {
            size_t blockSize;
            size_t padding;
        };

        struct FreeNode
        {
            size_t nodeSize;
            FreeNode* next;
        };

    public:
        FreeListAllocator() 
            : Allocator(), m_usedSize(0), m_head(nullptr)
        {
        }

        FreeListAllocator(size_t totalSize)
            : Allocator(totalSize), m_usedSize(0), m_head(nullptr)
        {
            assert(totalSize >= sizeof(FreeNode) && "Size of pre-allocated buffer is too small! [LinkedListAllocator.Constructor]");

#if defined(_MSC_VER)
            m_baseAddr = static_cast<uint8_t*>(_aligned_malloc(m_totalSize, alignof(Header)));
#else
            m_baseAddr = static_cast<uint8_t*>(std::aligned_alloc(m_totalSize, alignof(Header)));
#endif

            assert(m_baseAddr && "Failed to allocate! [LinkedListAllocator.Constructor]");

            Clear();
        }

        FreeListAllocator& operator=(FreeListAllocator&& allocator) noexcept
        {
            if(this == &allocator)
            {
                return *this;
            }

            if(m_baseAddr)
            {
                free(m_baseAddr);
            }

            m_baseAddr = allocator.m_baseAddr;
            m_totalSize = allocator.m_totalSize;

            m_head = allocator.m_head;
            m_usedSize = allocator.m_usedSize;

            allocator.m_baseAddr = nullptr;
            allocator.m_totalSize = 0;

            allocator.m_head = nullptr;
            allocator.m_usedSize = 0;
        }

        ~FreeListAllocator()
        {
            if(!m_baseAddr)
            {
                return;
            }
#if defined(_MSC_VER)
            _aligned_free(m_baseAddr);
#else
            std::free(m_baseAddr);
#endif
        }

        void* Alloc(size_t size, size_t align) override
        {
            assert(size <= (m_totalSize - m_usedSize) && "Size of allocation is larger than what remaining! [LinkedListAllocator.Alloc]");

            FreeNode* node = m_head;
            FreeNode* prevNode = nullptr;

            size_t headerAlign = alignof(Header);
            size_t headerMask = headerAlign - 1;

            size_t alignedSize = (size + headerMask) & ~headerMask;

            size_t mask = align - 1;
            assert((mask & align) == 0); //power of 2

            while(node)
            {
                size_t nodeSize = node->nodeSize;

                if(alignedSize > nodeSize)
                {
                    prevNode = node;
                    node = node->next;
                    continue;
                }

                uintptr_t currentAddr = reinterpret_cast<uintptr_t>(node);

                //uintptr_t alignedAddr = ((currentAddr + sizeof(Header)) + mask) & ~mask;
                uintptr_t alignedAddr;
                if(headerAlign > align)
                {
                    alignedAddr = (currentAddr + sizeof(Header) + headerMask) & ~headerMask;
                }
                else
                {
                    alignedAddr = ((currentAddr + sizeof(Header)) + mask) & ~mask;
                }


                size_t padding = alignedAddr - currentAddr;

                //first fit policy
                if(nodeSize >= (padding + alignedSize))
                {
                    //split
                    size_t additionalByte = Split(prevNode, node->next, currentAddr, node->nodeSize, padding + alignedSize);

                    uintptr_t headerAddr = alignedAddr - sizeof(Header);
                    Header* header = reinterpret_cast<Header*>(headerAddr);
                    header->blockSize = alignedSize + additionalByte;
                    header->padding = padding;

                    m_usedSize += padding + alignedSize + additionalByte;

                    return std::memset(reinterpret_cast<void*>(alignedAddr), 0, header->blockSize);
                }
                else
                {
                    prevNode = node;
                    node = node->next;
                    continue;
                }
            }

            assert(0 && "No free space left to allocate! [LinkedListAllocator.Alloc]");
            return nullptr;
        }

        void* Alloc(size_t size) override
        {
            return Alloc(size, DEFAULT_ALIGNMENT);
        }

        void* Realloc(void* addr, size_t newSize)
        {
            if(addr == nullptr)
            {
                assert(0 && "Free ptr is NULL [LinkedListAllocator.Free]");
            }

            uintptr_t currNodeAddr = reinterpret_cast<uintptr_t>(addr);
            uintptr_t baseNodeAddr = reinterpret_cast<uintptr_t>(m_baseAddr);
            uintptr_t endNodeAddr = reinterpret_cast<uintptr_t>(m_baseAddr + m_totalSize);

            if(currNodeAddr < baseNodeAddr || currNodeAddr >= endNodeAddr)
            {
                assert(0 && "Out of bounds memory address passed to pool [LinkedListAllocator.Free]");
                return nullptr;
            }

            uintptr_t headerAddr = currNodeAddr - sizeof(Header);
            Header* header = reinterpret_cast<Header*>(headerAddr);

            size_t padding = header->padding;
            size_t blockSize = header->blockSize;

            size_t headerAlign = alignof(Header);
            size_t headerMask = headerAlign - 1;

            size_t alignedNewSize = (newSize + headerMask) & ~headerMask;

            if(alignedNewSize <= blockSize)
            {
                return nullptr;
            }

            //check if next addr is free
            uintptr_t nextFreeNeighbor = currNodeAddr + blockSize;

            FreeNode* node = m_head;
            FreeNode* prevNode = nullptr;;

            while(node)
            {
                uintptr_t currNodeAddr = reinterpret_cast<uintptr_t>(node);

                if(currNodeAddr == nextFreeNeighbor)
                {
                    size_t newBlockSize = blockSize + node->nodeSize;
                    if(newBlockSize >= alignedNewSize)
                    {
                        size_t reallocSize = alignedNewSize - blockSize;

                        size_t additionalSize = Split(prevNode, node->next, currNodeAddr, node->nodeSize, reallocSize);

                        m_usedSize += reallocSize + additionalSize;
                        header->blockSize += (reallocSize + additionalSize);
                        header->padding = padding;

                        return addr;
                    }
                }
                prevNode = node;
                node = node->next;
            }

            node = m_head;
            prevNode = nullptr;

            size_t align = DEFAULT_ALIGNMENT;
            size_t mask = align - 1;

            while(node)
            {
                uintptr_t currNodeAddr = reinterpret_cast<uintptr_t>(node);
                uintptr_t alignedAddr;

                if(headerAlign > align)
                {
                    alignedAddr = (currNodeAddr + sizeof(Header) + headerMask) & ~headerMask;
                }
                else
                {
                    alignedAddr = ((currNodeAddr + sizeof(Header)) + mask) & ~mask;
                }

                size_t newPadding = alignedAddr - currNodeAddr;

                if(node->nodeSize >= alignedNewSize + newPadding)
                {
                    size_t additionalSize = Split(prevNode, node->next, currNodeAddr, node->nodeSize, alignedNewSize + newPadding);

                    Header* header = reinterpret_cast<Header*>(alignedAddr - sizeof(Header));
                    header->padding = newPadding;
                    header->blockSize = alignedNewSize;
                    m_usedSize += alignedNewSize + newPadding;

                    Free(addr);

                    return std::memset(reinterpret_cast<void*>(alignedAddr), 0, alignedNewSize);
                }

                prevNode = node;
                node = node->next;
            }

            return nullptr;
        }

        void Free(void* addr)
        {
            if(addr == nullptr)
            {
                assert(0 && "Free ptr is NULL [LinkedListAllocator.Free]");
            }

            uintptr_t currAddr = reinterpret_cast<uintptr_t>(addr);
            uintptr_t baseAddr = reinterpret_cast<uintptr_t>(m_baseAddr);
            uintptr_t endAddr = reinterpret_cast<uintptr_t>(m_baseAddr + m_totalSize);

            if(currAddr < baseAddr || currAddr >= endAddr)
            {
                assert(0 && "Out of bounds memory address passed to pool [LinkedListAllocator.Free]");
                return;
            }

            uintptr_t headerAddr = currAddr - sizeof(Header);
            Header* header = reinterpret_cast<Header*>(headerAddr);

            size_t padding = header->padding;
            size_t blockSize = header->blockSize;

            size_t freeNodeSize = blockSize + padding;
            m_usedSize -= freeNodeSize;

            uintptr_t startFreeNodeAddr = currAddr - padding;
            FreeNode* newFreeNode = reinterpret_cast<FreeNode*>(startFreeNodeAddr);

            newFreeNode->nodeSize = freeNodeSize;
            newFreeNode->next = nullptr;

            FreeNode* prevNode = Sort(newFreeNode);
            Coalesce(prevNode, newFreeNode);
        }

        void Clear() override
        {
            FreeNode* node = reinterpret_cast<FreeNode*>(m_baseAddr);
            node->nodeSize = m_totalSize;
            node->next = nullptr;

            m_head = node;
        }

    private:
        // returned value is additional size added to this alloc block
        // to make coalescing easier and avoid external fragmentation
        // 0 if splitable
        size_t Split(FreeNode* prevNode, FreeNode* nextNode, uintptr_t currNodeAddr, size_t nodeSize, size_t blockSize)
        {
            uintptr_t endNodeAddr = currNodeAddr + nodeSize;

            uintptr_t endBlockAddr = currNodeAddr + blockSize;

            uintptr_t remainingSize = endNodeAddr - endBlockAddr;

            //enough space to split into new free node
            if(remainingSize >= sizeof(FreeNode))
            {
                FreeNode* newFreeNode = reinterpret_cast<FreeNode*>(endBlockAddr);

                if(!prevNode)
                {
                    m_head = newFreeNode;
                }
                else
                {
                    prevNode->next = newFreeNode;
                }

                newFreeNode->next = nextNode;
                newFreeNode->nodeSize = remainingSize;

                return 0;
            }
            else
            {
                if(!prevNode)
                {
                    m_head = nextNode;
                }

                return remainingSize;
            }
        }

        void Coalesce(FreeNode* prevNode, FreeNode* currNode)
        {
            FreeNode* nextNode = currNode->next;
            uintptr_t currNodeAddr = reinterpret_cast<uintptr_t>(currNode);
            uintptr_t endCurrNodeAddr = currNodeAddr + currNode->nodeSize;

            //prev next null
            if(!prevNode && !nextNode)
            {
                return;
            }
            //prev null
            else if(!prevNode)
            {
                uintptr_t nextNodeAddr = reinterpret_cast<uintptr_t>(nextNode);

                if(nextNodeAddr == endCurrNodeAddr)
                {
                    currNode->next = nextNode->next;
                    currNode->nodeSize = currNode->nodeSize + nextNode->nodeSize;
                }
            }
            //next null
            else if(!nextNode)
            {
                uintptr_t prevNodeAddr = reinterpret_cast<uintptr_t>(prevNode);
                uintptr_t endPrevNodeAddr = prevNodeAddr + prevNode->nodeSize;


                if(currNodeAddr == endPrevNodeAddr)
                {
                    prevNode->next = nextNode;
                    prevNode->nodeSize = prevNode->nodeSize + currNode->nodeSize;
                }
            }
            //prev next not null
            else
            {
                uintptr_t nextNodeAddr = reinterpret_cast<uintptr_t>(nextNode);

                uintptr_t prevNodeAddr = reinterpret_cast<uintptr_t>(prevNode);
                uintptr_t endPrevNodeAddr = prevNodeAddr + prevNode->nodeSize;

                if(currNodeAddr == endPrevNodeAddr)
                {
                    prevNode->next = nextNode;
                    prevNode->nodeSize = prevNode->nodeSize + currNode->nodeSize;

                    currNode = prevNode;
                }

                if(nextNodeAddr == endCurrNodeAddr)
                {
                    currNode->next = nextNode->next;
                    currNode->nodeSize = currNode->nodeSize + nextNode->nodeSize;
                }
            }
        }

        // returned prev free node
        FreeNode* Sort(FreeNode* freeNode)
        {
            FreeNode* node = m_head;
            uintptr_t freeNodeAddr = reinterpret_cast<uintptr_t>(freeNode);

            //whole buffer is allocated
            if(!m_head)
            {
                m_head = freeNode;
                return nullptr;
            }

            uintptr_t headAddr = reinterpret_cast<uintptr_t>(node);
            //at the head
            if(freeNodeAddr < headAddr)
            {
                freeNode->next = node;
                m_head = freeNode;

                return nullptr;
            }

            while(true)
            {
                uintptr_t nodeAddr = reinterpret_cast<uintptr_t>(node);

                if(nodeAddr < freeNodeAddr)
                {
                    FreeNode* nextNode = node->next;
                    if(nextNode)
                    {
                        uintptr_t nextNodeAddr = reinterpret_cast<uintptr_t>(nextNode);

                        if(freeNodeAddr < nextNodeAddr)
                        {
                            node->next = freeNode;
                            freeNode->next = nextNode;
                            break;
                        }
                        else
                        {
                            node = node->next;
                        }
                    }
                    else
                    {
                        node->next = freeNode;
                        freeNode->next = nullptr;
                        break;
                    }
                }
            }

            return node;
        }
    private:
        FreeNode* m_head;
        size_t m_usedSize;
    };

    class RedBlackTreeAllocator
    {
    };

}