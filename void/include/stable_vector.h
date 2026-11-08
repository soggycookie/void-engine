#include "logger/log.h"
#include "utils.h"
#include "vallocator.h"

namespace VoidEngine
{

template <typename T>
class StableVector
{
private:
    struct Block
    {
        Block() : nextFreeIndex(0), version(0), isFree(1)
        {
        }

        alignas(T) char data[sizeof(T)];
        int32_t nextFreeIndex;
        uint16_t version;
        bool isFree;
    };

    struct Page
    {
        Page() : blocks(nullptr), blockFreeIndex(0), nextFreePageIndex(0) {};

        Block *blocks;
        int32_t blockFreeIndex;
        int32_t nextFreePageIndex;
    };

public:
    class StableHandle
    {
    private:
        friend class StableVector<T>;

        StableVector<T> *m_vector;
        int32_t m_pageIndex;
        int32_t m_blockIndex;
        uint16_t m_version;

    public:
        StableHandle() : m_vector(nullptr), m_version(0)
        {
        }

        bool IsHandleInvalid() const
        {
            return m_vector == nullptr;
        }

        StableHandle(const StableHandle &other)
        {
            m_vector = other.m_vector;
            m_pageIndex = other.m_pageIndex;
            m_blockIndex = other.m_blockIndex;
            m_version = other.m_version;
        }

        StableHandle(StableHandle &&other)
        {
            m_vector = other.m_vector;
            m_pageIndex = other.m_pageIndex;
            m_blockIndex = other.m_blockIndex;
            m_version = other.m_version;

            other.m_vector = nullptr;
            other.m_pageIndex = 0;
            other.m_blockIndex = 0;
            other.m_version = 0;
        }

        StableHandle &operator=(const StableHandle &other)
        {
            m_vector = other.m_vector;
            m_pageIndex = other.m_pageIndex;
            m_blockIndex = other.m_blockIndex;
            m_version = other.m_version;

            return *this;
        }

        StableHandle &operator=(StableHandle &&other)
        {
            m_vector = other.m_vector;
            m_pageIndex = other.m_pageIndex;
            m_blockIndex = other.m_blockIndex;
            m_version = other.m_version;

            other.m_vector = nullptr;
            other.m_pageIndex = 0;
            other.m_blockIndex = 0;
            other.m_version = 0;

            return *this;
        }

        bool IsHandleOutdated() const
        {
            LOG_ASSERT(!IsHandleInvalid(), "StableHandle is invalid!");
            return m_version != m_vector->GetVersion(*this);
        }

        StableHandle operator+(size_t index)
        {
            LOG_ASSERT(!IsHandleOutdated(), "Handle is outdated!");

            int32_t nextBlockIndex = m_blockIndex + index;

            int32_t nextPageRange = nextBlockIndex / m_vector->m_perPageCapacity;
            nextBlockIndex = nextBlockIndex % m_vector->m_perPageCapacity;
            int32_t nextPageIndex = m_pageIndex + nextPageRange;

            if (nextPageIndex >= m_vector->m_pageCapacity)
            {
                return StableHandle();
            }

            const Block &nextBlock = m_vector->m_pages[nextPageIndex].blocks[nextBlockIndex];

            return StableHandle(m_vector, nextPageIndex, nextBlockIndex, nextBlock.version);
        }

        StableHandle operator-(size_t index)
        {
            LOG_ASSERT(!IsHandleOutdated(), "Handle is outdated!");

            int32_t prevBlockIndex = m_blockIndex - index;

            int32_t prevPageRange = std::floor(prevBlockIndex / (float)m_vector->m_perPageCapacity);
            prevBlockIndex = std::abs(prevBlockIndex) % m_vector->m_perPageCapacity;

            if (std::abs(prevPageRange) > m_pageIndex)
            {
                return StableHandle();
            }

            uint32_t prevPageIndex = m_pageIndex + prevPageRange;
            const Block &prevBlock = m_vector->m_pages[prevPageIndex].blocks[prevBlockIndex];

            return StableHandle(m_vector, prevPageIndex, prevBlockIndex, prevBlock.version);
        }

        StableHandle operator++()
        {
            LOG_ASSERT(!IsHandleOutdated(), "Handle is outdated!");

            int32_t nextBlockIndex = m_blockIndex + 1;
            uint32_t nextPageIndex = m_pageIndex;

            if (nextBlockIndex >= m_vector->m_perPageCapacity)
            {
                ++nextPageIndex;
                if (nextPageIndex >= m_vector->m_pageCapacity)
                {
                    return StableHandle();
                }

                nextBlockIndex = 0;
            }

            const Block &nextBlock = m_vector->m_pages[nextPageIndex].blocks[nextBlockIndex];

            m_pageIndex = nextPageIndex;
            m_blockIndex = nextBlockIndex;
            m_version = nextBlock.version;

            return *this;
        }

        StableHandle operator++(int)
        {
            LOG_ASSERT(!IsHandleOutdated(), "Handle is outdated!");

            StableHandle temp = *this;

            int32_t nextBlockIndex = m_blockIndex + 1;
            uint32_t nextPageIndex = m_pageIndex;

            if (nextBlockIndex >= m_vector->m_perPageCapacity)
            {
                ++nextPageIndex;
                if (nextPageIndex >= m_vector->m_pageCapacity)
                {
                    return StableHandle();
                }

                nextBlockIndex = 0;
            }

            const Block &nextBlock = m_vector->m_pages[nextPageIndex].blocks[nextBlockIndex];

            m_pageIndex = nextPageIndex;
            m_blockIndex = nextBlockIndex;
            m_version = nextBlock.version;

            return temp;
        }

        StableHandle operator--()
        {
            LOG_ASSERT(!IsHandleOutdated(), "Handle is outdated!");

            int32_t prevBlockIndex = m_blockIndex - 1;
            uint32_t prevPageIndex = m_pageIndex;

            if (prevBlockIndex < 0)
            {
                if (prevPageIndex == 0)
                {
                    return StableHandle();
                }

                --prevPageIndex;
                prevBlockIndex = m_vector->m_perPageCapacity - 1;
            }

            const Block &prevBlock = m_vector->m_pages[prevPageIndex].blocks[prevBlockIndex];

            return StableHandle(m_vector, prevPageIndex, prevBlockIndex, prevBlock.version);
        }

        StableHandle operator--(int)
        {
            LOG_ASSERT(!IsHandleOutdated(), "Handle is outdated!");

            StableHandle temp = *this;

            int32_t prevBlockIndex = m_blockIndex - 1;
            uint32_t prevPageIndex = m_pageIndex;

            if (prevBlockIndex < 0)
            {
                if (prevPageIndex == 0)
                {
                    return StableHandle();
                }

                --prevPageIndex;
                prevBlockIndex = m_vector->m_perPageCapacity - 1;
            }

            const Block &prevBlock = m_vector->m_pages[prevPageIndex].blocks[prevBlockIndex];

            m_pageIndex = prevPageIndex;
            m_blockIndex = prevBlockIndex;
            m_version = prevBlock.version;

            return temp;
        }

        T &operator*()
        {
            LOG_ASSERT(!IsHandleOutdated(), "Handle is outdated!");

            Block &block = m_vector->m_pages[m_pageIndex].blocks[m_blockIndex];

            return *RCAST(T *, block.data);
        }

        const T &operator*() const
        {
            LOG_ASSERT(!IsHandleOutdated(), "Handle is outdated!");

            Block &block = m_vector->m_pages[m_pageIndex].blocks[m_blockIndex];

            return *RCAST(T *, block.data);
        }

        bool operator==(const StableHandle &other) noexcept
        {
            return (m_vector == other.m_vector) && (m_pageIndex == other.m_pageIndex) &&
                   (m_blockIndex == other.m_blockIndex) && (m_version == other.m_version);
        }

        bool operator!=(const StableHandle &other) noexcept
        {
            return !(*this == other);
        }

    private:
        StableHandle(StableVector<T> *vector, uint32_t pageIndex, int32_t blockIndex, uint16_t version)
            : m_vector(vector), m_pageIndex(pageIndex), m_blockIndex(blockIndex), m_version(version)
        {
        }
    };

private:
    static constexpr uint32_t s_minDefaultPerPageCapactiy = 8;
    static constexpr int32_t s_InvalidBlockIndex = -1;
    static constexpr int32_t s_InvalidPageIndex = -1;
    Vallocator *m_allocator;

    Page *m_pages;
    uint32_t m_pageCapacity;
    uint32_t m_pageCount;
    uint32_t m_count;
    uint32_t m_capacity;
    uint32_t m_perPageCapacity;
    int32_t m_freePageIndex;

public:
    StableVector(uint32_t pageCapacity, uint32_t perPageCapacity, Vallocator *allocator)
        : m_perPageCapacity(pageCapacity), m_freePageIndex(s_InvalidPageIndex), m_count(0),
          m_capacity(0), m_pages(nullptr), m_allocator(allocator), m_pageCapacity(pageCapacity),
          m_pageCount(0)
    {
        static_assert(std::is_constructible_v<T>, "Type must be constructible!");
        static_assert(std::is_destructible_v<T>, "Type must be destructible!");
        static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
                      "Type must be copy or move constructible!");

        LOG_ASSERT(m_allocator, "Backing Allocator is null!");

        if (m_perPageCapacity < s_minDefaultPerPageCapactiy)
        {
            m_perPageCapacity = s_minDefaultPerPageCapactiy;
        }

        m_capacity = m_pageCapacity * m_perPageCapacity;
        LOG_ASSERT(m_capacity > 0, "Capacity must be greater than 0!");
    }

    StableVector(StableVector &&other) noexcept
    {
        m_pages = other.m_pages;
        m_allocator = other.m_allocator;

        m_pageCount = other.m_pageCount;
        m_pageCapacity = other.m_pageCapacity;
        m_count = other.m_count;
        m_capacity = other.m_capacity;
        m_perPageCapacity = other.m_perPageCapacity;
        m_freePageIndex = other.m_freePageIndex;

        other.m_count = 0;
        other.m_freePageIndex = 0;
    }

    StableVector &operator=(StableVector &&other) noexcept
    {
        Destroy();

        m_pages = other.m_pages;
        m_allocator = other.m_allocator;

        m_pageCount = other.m_pageCount;
        m_pageCapacity = other.m_pageCapacity;
        m_count = other.m_count;
        m_capacity = other.m_capacity;
        m_perPageCapacity = other.m_perPageCapacity;
        m_freePageIndex = other.m_freePageIndex;

        other.m_count = 0;
        other.m_freePageIndex = 0;

        return *this;
    }

    ~StableVector()
    {
        Destroy();
    }

    void AssignAllocator(Vallocator *allocator)
    {
        LOG_ASSERT(!m_allocator, "Allocator is already set!");
        LOG_ASSERT(!m_pages, "Pages are already set!");
        m_allocator = allocator;
    }

    template <typename U>
    StableHandle Insert(U &&value)
    {
        LOG_ASSERT(m_allocator, "Backing allocator is null!");
        static_assert(std::is_constructible_v<T, U &&>, "T must be constructible from U&&");
        if (m_freePageIndex == s_InvalidPageIndex)
        {
            ResizePage(!m_pages ? m_pageCapacity : std::ceil(m_pageCapacity * 1.3f));
        }

        auto freeBlock = GetFreeBlock(m_pages[m_freePageIndex]);

        if (!freeBlock.first)
        {
            LOG_FATAL("INTERNAL", "Failed to allocate block!");
        }

        new (freeBlock.first->data) T(std::forward<U>(value));

        ++m_count;

        return StableHandle(this, m_freePageIndex, freeBlock.second, freeBlock.first->version);
    }

    template <typename... Args>
    StableHandle Emplace(Args &&...args)
    {
        LOG_ASSERT(m_allocator, "Backing allocator is null!");
        static_assert(std::is_constructible_v<T, Args &&...>, "T must be constructible from Args&&");
        if (m_freePageIndex == s_InvalidPageIndex)
        {
            ResizePage(!m_pages ? m_pageCapacity : std::ceil(m_pageCapacity * 1.3f));
        }

        auto &freeBlock = GetFreeBlock(m_pages[m_freePageIndex]);

        if (!freeBlock.first)
        {
            LOG_FATAL("INTERNAL", "Failed to allocate block!");
        }

        new (freeBlock.first->data) T(std::forward<Args>(args)...);

        ++m_count;

        return StableHandle(this, m_freePageIndex, freeBlock.second, freeBlock.first->version);
    }

    void Erase(StableHandle handle)
    {
        LOG_ASSERT(m_allocator, "Backing allocator is null!");
        LOG_ASSERT(!handle.IsHandleOutdated(), "Handle is outdated");

        Page &page = m_pages[handle.m_pageIndex];

        Get(page.blocks[handle.m_blockIndex])->~T();

        page.blocks[handle.m_blockIndex].nextFreeIndex = page.blockFreeIndex;
        page.blocks[handle.m_blockIndex].isFree = true;

        if (page.blockFreeIndex == s_InvalidBlockIndex)
        {
            page.nextFreePageIndex = m_freePageIndex;
            m_freePageIndex = handle.m_pageIndex;
        }
        page.blockFreeIndex = handle.m_blockIndex;

        --m_count;
    }

    uint16_t GetVersion(StableHandle handle) const
    {
        LOG_ASSERT(!handle.IsHandleInvalid(), "Invalid stable handle!");

        return m_pages[SCAST(size_t, handle.m_pageIndex)]
            .blocks[SCAST(size_t, handle.m_blockIndex)]
            .version;
    }

    uint32_t GetPageCapacity() const
    {
        return m_perPageCapacity;
    }

private:
    void ResizePage(uint32_t newCapacity)
    {
        LOG_ASSERT(m_allocator, "Backing allocator is null!");
        Page *newPages = SCAST(Page *, m_allocator->valloc(newCapacity * sizeof(Page), alignof(Page)));

        std::memcpy(newPages, m_pages, m_pages ? m_pageCapacity * sizeof(Page) : 0);
        m_allocator->vfree(m_pages);
        m_pages = newPages;

        for (size_t idx = m_pageCount; idx < newCapacity - 1; ++idx)
        {
            m_pages[idx].blocks = nullptr;
            m_pages[idx].blockFreeIndex = s_InvalidBlockIndex;
            m_pages[idx].nextFreePageIndex = idx + 1;
        }

        m_pages[newCapacity - 1].blocks = nullptr;
        m_pages[newCapacity - 1].blockFreeIndex = s_InvalidBlockIndex;
        m_pages[newCapacity - 1].nextFreePageIndex = s_InvalidPageIndex;

        m_freePageIndex = m_pageCount;

        m_pageCount = newCapacity;
        m_pageCapacity = newCapacity;
    }

    std::pair<Block *, int32_t> GetFreeBlock(Page &page)
    {
        LOG_ASSERT(m_allocator, "Backing allocator is null!");
        if (!page.blocks && page.blockFreeIndex == s_InvalidBlockIndex)
        {
            page.blocks =
                SCAST(Block *, m_allocator->valloc(sizeof(Block) * m_perPageCapacity, alignof(Page)));

            if (!page.blocks)
            {
                LOG_FATAL("INTERNAL", "Failed to alloc page!");

                return std::make_pair(nullptr, s_InvalidBlockIndex);
            }

            for (size_t blockIdx = 0; blockIdx < m_perPageCapacity - 1;)
            {
                Block *block = new (page.blocks + blockIdx) Block();
                block->nextFreeIndex = ++blockIdx;
            }
            Block *lastBlock = new (page.blocks + m_perPageCapacity - 1) Block();
            lastBlock->nextFreeIndex = s_InvalidBlockIndex;
            page.blockFreeIndex = 0;
        }

        int32_t index = page.blockFreeIndex;
        LOG_ASSERT(index != s_InvalidBlockIndex, "Page does not contain free block!");
        Block *block = &page.blocks[index];
        ++block->version;
        LOG_ASSERT(block->isFree, "Block is not free!");

        block->isFree = false;
        page.blockFreeIndex = block->nextFreeIndex;

        if (page.blockFreeIndex == s_InvalidBlockIndex)
        {
            m_freePageIndex = page.nextFreePageIndex;
            page.nextFreePageIndex = s_InvalidPageIndex;
        }

        return std::make_pair(block, index);
    }

    T *Get(Block &block)
    {
        return RCAST(T *, block.data);
    }

    void Destroy()
    {
        if (m_pages)
        {
            LOG_ASSERT(m_allocator, "Allocator is null!");
            for (size_t idx = 0; idx < m_pageCount; ++idx)
            {
                if (m_pages[idx].blocks)
                {
                    for (size_t blockIdx = 0; blockIdx < m_perPageCapacity; ++blockIdx)
                    {
                        if (!m_pages[idx].blocks[blockIdx].isFree)
                        {
                            Get(m_pages[idx].blocks[blockIdx])->~T();
                        }
                    }

                    if (m_allocator)
                    {
                        m_allocator->vfree(m_pages[idx].blocks);
                    }
                    else
                    {
                        aligned_free(m_pages[idx].blocks);
                    }
                }
            }

            m_allocator->vfree(m_pages);
        }
        else
        {
            LOG_ASSERT(m_count == 0, "Should never allocate!");
        }
    }
};

} // namespace VoidEngine
