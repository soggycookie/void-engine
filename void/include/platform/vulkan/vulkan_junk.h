
//
// struct VulkanDeviceMemoryBlock;
// struct VulkanAllocation;
// struct VulkanPool;
// struct VulkanAllocator;
// struct VulkanMetadata;
//
// enum class VulkanSuballocationType : uint8_t
// {
//     VULKAN_BUFFER,
//     VULKAN_IMAGE_LINEAR,
//     VULKAN_IMAGE_OPTIMAL,
// };
//
// using VulkanAllocMetadata = void *;
//
// struct VulkanAllocation
// {
//     DEFAULT_CTOR(VulkanAllocation);
//
//     VulkanAllocation(VulkanAllocation &&other) noexcept
//     {
//         metadataHandle = other.metadataHandle;
//         vlkMemoryBlock = other.vlkMemoryBlock;
//         memory = other.memory;
//         offset = other.offset;
//         size = other.size;
//         suballocationType = other.suballocationType;
//
//         other.metadataHandle = nullptr;
//         other.vlkMemoryBlock = nullptr;
//         other.memory = VK_NULL_HANDLE;
//         other.offset = 0;
//         other.size = 0;
//     }
//
//     VulkanAllocation &operator=(VulkanAllocation &&other)
//     {
//         metadataHandle = other.metadataHandle;
//         vlkMemoryBlock = other.vlkMemoryBlock;
//         memory = other.memory;
//         offset = other.offset;
//         size = other.size;
//         suballocationType = other.suballocationType;
//
//         other.metadataHandle = nullptr;
//         other.vlkMemoryBlock = nullptr;
//         other.memory = VK_NULL_HANDLE;
//         other.offset = 0;
//         other.size = 0;
//
//         return *this;
//     }
//
//     VulkanAllocMetadata metadataHandle;
//     VulkanDeviceMemoryBlock *vlkMemoryBlock;
//     VkDeviceMemory memory;
//     VkDeviceSize offset;
//     VkDeviceSize size;
//     VulkanSuballocationType suballocationType;
//     bool isPersistentMapped;
// };
//
// using VulkanAllocationHandle = StableVector<VulkanAllocation>::StableHandle;
//
// // TODO: implement internal fragmentation monitoring
// class VulkanMetadata
// {
// public:
//     using Header = size_t;
//
//     struct Block
//     {
//         Header header;
//         VkDeviceSize offset;
//         Block *nextFree;
//         Block *prevFree;
//         Block *nextPhysical;
//         Block *prevPhysical;
//
//     private:
//         DEFAULT_CTOR(Block);
//
//         friend class VulkanMetadata;
//     };
//
//     struct Index
//     {
//         uint16_t fli;
//         uint16_t sli;
//
//         DEFAULT_COMPARABLE(Index);
//     };
//
//     struct RequestAlloc
//     {
//         Block *block;
//         VkDeviceSize size;
//         uint32_t alignment;
//         Index index;
//     };
//
//     static constexpr uint32_t s_slBitShift = 5;
//     static constexpr uint32_t s_slCount = 1U << s_slBitShift;
//     static constexpr uint32_t s_flCount = 24;
//     static constexpr Index s_invalidIndex = {s_flCount, s_slCount};
//
// private:
//     struct FreeNodePool
//     {
//         DEFAULT_CTOR(FreeNodePool);
//
//         FreeNodePool *nextPool;
//     };
//
//     struct FreeNode
//     {
//         FreeNode *next;
//     };
//
// private:
//     static constexpr uint32_t s_minBaseAlignment = 2;
//     static constexpr uint32_t s_blockPerPool = 20;
//     static constexpr uint32_t s_poolSize = sizeof(Block) * s_blockPerPool + sizeof(FreeNodePool);
//     static constexpr uint32_t s_minPayloadSize = 256;
//     static constexpr uint32_t s_freeListCount = s_flCount * s_slCount;
//
//     uint32_t m_slBitmaps[s_flCount];
//     uint32_t m_flBitMap;
//     uint32_t m_allocationCount;
//     uint32_t m_internalFragmentCount;
//     uint32_t m_cpuAllocationCount;
//     size_t m_allocationSize;
//
//     VulkanDeviceMemoryBlock *m_memoryBlock;
//     Block *m_head;
//     Vallocator *m_cpuAllocator;
//     FreeNode *m_freeNode;
//     FreeNodePool *m_freeNodeHead;
//     Block *m_freeLists[s_freeListCount];
//     uint32_t m_smallCount;
//     uint32_t m_lineTimeAllocationCount;
//
// public:
//     VulkanMetadata(VulkanDeviceMemoryBlock *memoryBlock, Vallocator *allocator);
//
//     void Destroy();
//
//     bool CanAlloc(uint32_t alignment, size_t size, RequestAlloc &outRequest);
//
//     void Alloc(const RequestAlloc &request, VulkanAllocation &allocation);
//     void Free(Block *block);
//
//     uint32_t GetAllocationCount() const
//     {
//         return m_allocationCount;
//     }
//     size_t GetAllocationSize() const
//     {
//         return m_allocationSize;
//     }
//
//     void Print();
//
// private:
//     FreeNodePool *AllocateFreeNodePool();
//
//     void *GetBlockAddress();
//
//     Index MapIndex(size_t size);
//     Index FindNextFreeIndex(Index startIndex);
//
//     size_t GetPayloadSize(Block *block);
//
//     void InsertFreeBlock(Block *block);
//     Block *AcquireBlockFromList(Index index);
//     void RemoveFreeBlock(Block *block);
//
//     Block *Split(Block *block, size_t size);
//     bool CanSplit(Block *block, size_t size);
//
//     Block *Coalesce(Block *prevBlock, Block *block);
//
//     void MarkUsed(Block *block);
//     void MarkFree(Block *block);
//
//     bool IsUsedBlock(Block *size);
// };
//
// struct VulkanDeviceMemoryBlock
// {
//     DEFAULT_CTOR(VulkanDeviceMemoryBlock);
//
//     VulkanDeviceMemoryBlock(VulkanDeviceMemoryBlock &&other) noexcept
//         : metadata(other.metadata), memory(other.memory), size(other.size),
//           memoryTypeIndex(other.memoryTypeIndex), pool(other.pool), mapped(other.mapped),
//           allocations(std::move(other.allocations)), mappingCount(other.mappingCount)
//     {
//         other.metadata = nullptr;
//         other.memory = VK_NULL_HANDLE;
//         other.pool = nullptr;
//         other.mapped = nullptr;
//         other.mappingCount = 0;
//     }
//
//     //
//     VulkanMetadata *metadata;
//     VkDeviceMemory memory;
//     VkDeviceSize size;
//     VulkanPool *pool;
//     void *mapped;
//     uint32_t memoryTypeIndex;
//     uint16_t mappingCount;
//
//     // remove this, serve nothing, release and keep track is not this struct's duty
//     std::optional<StableVector<VulkanAllocation>> allocations;
// };
//
// struct VulkanPool
// {
//     VulkanAllocator *allocator;
//     VkMemoryType memoryType;
//     uint32_t memoryTypeIndex;
//
//     DynamicArray<VulkanDeviceMemoryBlock> linearMemoryBlocks;
//     DynamicArray<VulkanDeviceMemoryBlock> nonLinearMemoryBlocks;
// };
//
// struct VulkanHeap
// {
//     VkDeviceSize budget;
//     VkDeviceSize usage;
//     VkDeviceSize trackedByte;
// };
//
// struct VulkanBufferArena
// {
//     VulkanMetadata metadata;
//     VkDeviceMemory memory;
//     VkDeviceSize size;
//     uint32_t memoryTypeIndex;
//     uint32_t tag;
// };
//
// struct VulkanAlignmentLimits
// {
//     VkDeviceSize minTexelBufferOffsetAlignment;
//     VkDeviceSize minUniformBufferOffsetAlignment;
//     VkDeviceSize minStorageBufferOffsetAlignment;
//     VkDeviceSize bufferImageGranularity;
// };
//
// struct VulkanAllocationInfo
// {
//     void *mappedData;
// };
//
// enum VulkanMemoryUsage : uint8_t
// {
//     VULKAN_MEMORY_USAGE_GPU_ONLY,
//     VULKAN_MEMORY_USAGE_CPU_ONLY,
//     VULKAN_MEMORY_USAGE_CPU_AND_GPU,
// };
// using VulkanMemoryFlag = uint32_t;
//
// enum VulkanMemoryFlagBit : uint8_t
// {
//     VULKAN_MEMORY_FLAG_PERSISTANT_MAPPED_BIT = 0x00000001,
//     VULKAN_MEMORY_FLAG_CPU_RANDOM_ACCESS_BIT = 0x00000002,
//     VULKAN_MEMORY_FLAG_CPU_SEQUENTIAL_WRITE_BIT = 0x00000004,
//     VULKAN_MEMORY_FLAG_DEDICATED_MEMORY_BIT = 0x00000008,
//     VULKAN_MEMORY_FLAG_WITHIN_BUDGET_BIT = 0x00000010,
// };
//
// // VULKAN ALLOCATOR
// class VulkanAllocator
// {
// private:
//     static constexpr VkDeviceSize s_baseMemorySize = mib(32);
//     VulkanAllocator()
//     {
//     }
//
//     VulkanPool m_pools[VK_MAX_MEMORY_TYPES];
//     VulkanHeap m_heaps[VK_MAX_MEMORY_HEAPS];
//     // std::optional<FlatHashMap<uint32_t, VlkBufferArena>> m_bufferArenas;
//     Vallocator *m_cpuAllocator;
//
//     VkPhysicalDevice m_physicalDevice;
//     VkDevice m_device;
//
//     VulkanAlignmentLimits m_alignmentLimits;
//
//     uint32_t m_memoryTypeCount;
//     uint32_t m_memoryHeapCount;
//     uint32_t m_maxAllocation;
//     uint32_t m_allocationCount;
//
//     bool m_isMemoryBudgetEXTSupported;
//     bool m_isDedicatedAllocationEXTSupported;
//
// public:
//     NO_COPY_ASSIGNMENT(VulkanAllocator);
//     NO_COPY_CTOR(VulkanAllocator);
//     NO_MOVE_ASSIGNMENT(VulkanAllocator);
//     NO_MOVE_CTOR(VulkanAllocator);
//
//     VulkanAllocationHandle Alloc(size_t alignment, size_t size, uint32_t memoryTypeIndex,
//                                  VulkanSuballocationType type);
//     void Free(VulkanAllocationHandle handle);
//
//     void AllocateBufferArena(uint32_t tag, size_t size);
//     VulkanAlignmentLimits GetAlignmentLimits() const
//     {
//         return m_alignmentLimits;
//     }
//
//     VkDevice GetDevice() const
//     {
//         return m_device;
//     }
//     VkPhysicalDevice GetPhysicalDevice() const
//     {
//         return m_physicalDevice;
//     }
//
//     uint32_t ChooseSuitableMemoryTypeIndex(uint32_t memoryTypeBits, VulkanMemoryUsage usage,
//                                            VulkanMemoryFlag flag);
//
//     bool IsMemoryTypeIndexLocal(uint32_t memoryTypeIndex);
//     bool IsMemoryTypeIndexHost(uint32_t memoryTypeIndex);
//
//     // void PrintMemoryType();
//
//     void PrintPool();
//
// private:
//     friend class VulkanBackend;
//
//     void Destroy();
//
//     void Init(VkPhysicalDevice physicalDevice, VkDevice device, bool isMemoryBudgetEXTSupported,
//               bool isDedicatedAllocationEXTSupported, Vallocator *allocator);
//
//     void ResizeBufferArena(uint32_t tag, size_t newSize);
//
//     void UpdateBudget();
//
//     void DestroyArena(uint32_t tag);
//
//     void AllocateDeviceMemory(VulkanPool *pool, VkDeviceSize size, uint32_t memoryTypeIndex,
//                               bool isLinear);
// };
//
// void MemoryTypeFlagsToString(VkMemoryPropertyFlags flags, char *buffer, size_t size);
//
//
//
// VulkanMetadata::VulkanMetadata(VulkanDeviceMemoryBlock *memoryBlock, Vallocator *allocator)
//     : m_flBitMap(0), m_allocationCount(0), m_allocationSize(0), m_freeLists(nullptr),
//       m_freeNodeHead(nullptr), m_freeNode(nullptr), m_cpuAllocator(allocator),
//       m_memoryBlock(memoryBlock), m_smallCount(0), m_internalFragmentCount(0), m_head(nullptr),
//       m_cpuAllocationCount(0), m_lineTimeAllocationCount(0)
// {
//
//     std::memset(m_slBitmaps, 0, sizeof(m_slBitmaps));
//     std::memset(m_freeLists, 0, sizeof(m_freeLists));
//
//     m_freeNodeHead = AllocateFreeNodePool();
//
//     Block *block = new (GetBlockAddress()) Block();
//     block->header = m_memoryBlock->size;
//     block->offset = 0;
//     block->nextFree = nullptr;
//     block->prevFree = nullptr;
//     block->nextPhysical = nullptr;
//     block->prevPhysical = nullptr;
//
//     m_head = block;
//
//     InsertFreeBlock(block);
// }
//
// VulkanMetadata::FreeNodePool *VulkanMetadata::AllocateFreeNodePool()
// {
//     LOG_ASSERT(m_cpuAllocator, "Allocator is not provided!");
//     LOG_ASSERT(m_memoryBlock, "Memory block is not provided!");
//
//     char *rawPool = nullptr;
//
//     if (m_cpuAllocator)
//     {
//         rawPool = SCAST(char *, m_cpuAllocator->valloc(s_poolSize, 8));
//     }
//     else
//     {
//         rawPool = SCAST(char *, aligned_alloc(alignof(std::max_align_t), s_poolSize));
//     }
//
//     LOG_ASSERT(rawPool, "Failed to allocate more VulkanMetadata Pool!");
//
//     FreeNodePool *pool = new (rawPool) FreeNodePool();
//     pool->nextPool = m_freeNodeHead;
//     m_freeNodeHead = pool;
//
//     char *address = rawPool + sizeof(FreeNodePool);
//
//     for (size_t idx = 0; idx < s_blockPerPool; ++idx)
//     {
//         FreeNode *nextNode = new (address + idx * sizeof(Block)) FreeNode();
//         nextNode->next = m_freeNode;
//         m_freeNode = nextNode;
//     }
//
//     ++m_cpuAllocationCount;
//
//     return pool;
// }
//
// void *VulkanMetadata::GetBlockAddress()
// {
//     if (!m_freeNode)
//     {
//         AllocateFreeNodePool();
//     }
//
//     FreeNode *node = m_freeNode;
//     m_freeNode = node->next;
//
//     node->~FreeNode();
//
//     return RCAST(void *, node);
// }
//
// VulkanMetadata::Index VulkanMetadata::MapIndex(size_t size)
// {
//     if (size <= s_minPayloadSize)
//     {
//         return {0, 0};
//     }
//
//     constexpr size_t firstFLI = floor_log2(s_minPayloadSize);
//     size_t exp = floor_log2(size);
//     size_t flSizeClass = (1 << exp);
//
//     uint16_t fli = SCAST(uint16_t, exp - firstFLI);
//     uint16_t sli = SCAST(uint16_t, (size - flSizeClass) >> (exp - s_slBitShift));
//
//     return {fli, sli};
// }
//
// VulkanMetadata::Index VulkanMetadata::FindNextFreeIndex(Index startIndex)
// {
//     LOG_ASSERT(startIndex.fli < s_flCount, "FLI is out of bound!");
//     LOG_ASSERT(startIndex.sli < s_slCount, "SLI is out of bound!");
//
//     uint32_t slMap = 0;
//     if (startIndex.sli < s_slCount - 1)
//     {
//         slMap = m_slBitmaps[startIndex.fli] & (~0U << (startIndex.sli + 1));
//     }
//
//     if (slMap > 0)
//     {
//         uint16_t sli = SCAST(uint16_t, std::countr_zero(slMap));
//
//         return {startIndex.fli, sli};
//     }
//
//     uint32_t flMap = m_flBitMap & (~0U << (startIndex.fli + 1));
//
//     if (flMap > 0)
//     {
//         uint16_t fli = SCAST(uint16_t, std::countr_zero(flMap));
//         uint16_t sli = SCAST(uint16_t, std::countr_zero(m_slBitmaps[fli]));
//
//         return {fli, sli};
//     }
//
//     return s_invalidIndex;
// }
//
// size_t VulkanMetadata::GetPayloadSize(Block *block)
// {
//     LOG_ASSERT(block, "Block cannot be null!");
//
//     return block->header & ~1U;
// }
//
// void VulkanMetadata::InsertFreeBlock(Block *block)
// {
//     LOG_ASSERT(block, "Block is null!");
//     LOG_ASSERT(!IsUsedBlock(block), "Inserting used block is invalid!");
//
//     Index index = MapIndex(GetPayloadSize(block));
//
//     Block *current = m_freeLists[index.fli * s_slCount + index.sli];
//     block->nextFree = current;
//     block->prevFree = nullptr;
//
//     if (current)
//     {
//         current->prevFree = block;
//     }
//
//     m_flBitMap |= (1U << index.fli);
//     m_slBitmaps[index.fli] |= (1U << index.sli);
//
//     m_freeLists[index.fli * s_slCount + index.sli] = block;
// }
//
// VulkanMetadata::Block *VulkanMetadata::AcquireBlockFromList(Index index)
// {
//     LOG_ASSERT(index != s_invalidIndex, "Index cannot be invalid!");
//     LOG_ASSERT(index.fli < s_flCount, "FLI is out of bound!");
//     LOG_ASSERT(index.sli < s_slCount, "SLI is out of bound!");
//
//     Block *block = nullptr;
//     {
//         block = m_freeLists[index.fli * s_slCount + index.sli];
//
//         if (block == nullptr)
//         {
//             return nullptr;
//         }
//
//         Block *next = block->nextFree;
//         block->nextFree = nullptr;
//         block->prevFree = nullptr;
//         m_freeLists[index.fli * s_slCount + index.sli] = next;
//
//         if (next == nullptr)
//         {
//             m_slBitmaps[index.fli] &= ~(1U << index.sli);
//             if (m_slBitmaps[index.fli] == 0)
//             {
//                 m_flBitMap &= ~(1U << index.fli);
//             }
//         }
//         else
//         {
//             next->prevFree = nullptr;
//         }
//     }
//
//     return block;
// }
//
// void VulkanMetadata::RemoveFreeBlock(Block *block)
// {
//     LOG_ASSERT(block, "Block cannot be null!");
//     LOG_ASSERT(!IsUsedBlock(block), "Block cannot be used!");
//
//     Index index = MapIndex(GetPayloadSize(block));
//
//     Block *next = block->nextFree;
//     Block *prev = block->prevFree;
//
//     if (m_freeLists[index.fli * s_slCount + index.sli] == block)
//     {
//         m_freeLists[index.fli * s_slCount + index.sli] = next;
//     }
//
//     if (next == nullptr && prev == nullptr)
//     {
//         m_slBitmaps[index.fli] &= ~(1U << index.sli);
//         if (m_slBitmaps[index.fli] == 0)
//         {
//             m_flBitMap &= ~(1U << index.fli);
//         }
//     }
//     else
//     {
//         if (next)
//         {
//             next->prevFree = prev;
//         }
//
//         if (prev)
//         {
//             prev->nextFree = next;
//         }
//     }
// }
//
// VulkanMetadata::Block *VulkanMetadata::Split(Block *block, size_t size)
// {
//     LOG_ASSERT(block, "Block cannot be null!");
//     LOG_ASSERT(!IsUsedBlock(block), "Block cannot be used!");
//     LOG_ASSERT(size >= s_minPayloadSize, "Split size must be greater or equal min payload size %u",
//                s_minPayloadSize);
//     LOG_ASSERT(GetPayloadSize(block) >= s_minPayloadSize * 2,
//                "Block size must be greater or equal to %u", s_minPayloadSize * 2);
//
//     size_t remainingSize = GetPayloadSize(block) - size;
//
//     LOG_ASSERT(remainingSize >= s_minPayloadSize,
//                "Remaining size must be greater or equal min block size %u", s_minPayloadSize);
//
//     Block *newBlock = new (GetBlockAddress()) Block();
//     newBlock->header = remainingSize;
//     newBlock->offset = block->offset + size;
//     newBlock->nextFree = nullptr;
//     newBlock->prevFree = nullptr;
//     newBlock->prevPhysical = block;
//     newBlock->nextPhysical = block->nextPhysical;
//
//     // dont need to retain flag because prev physical block must be used anyway
//     // if it is free then it is already coalesced
//     block->header = size;
//     block->nextPhysical = newBlock;
//
//     return newBlock;
// }
//
// bool VulkanMetadata::CanSplit(Block *block, size_t size)
// {
//     LOG_ASSERT(block, "Block cannot be null!");
//     LOG_ASSERT(!IsUsedBlock(block), "Block cannot be used!");
//
//     if (size < s_minPayloadSize || size >= GetPayloadSize(block) ||
//         (GetPayloadSize(block) - size) < s_minPayloadSize)
//     {
//         return false;
//     }
//
//     return true;
// }
//
// VulkanMetadata::Block *VulkanMetadata::Coalesce(Block *prevBlock, Block *block)
// {
//     LOG_ASSERT(block, "Block cannot be null!");
//     LOG_ASSERT(!IsUsedBlock(block), "Block cannot be used!");
//     LOG_ASSERT(prevBlock, "Prev block cannot be null!");
//     LOG_ASSERT(!IsUsedBlock(prevBlock), "Prev block cannot be used!");
//
//     LOG_ASSERT((GetPayloadSize(prevBlock) + prevBlock->offset) == block->offset,
//                "Prev and current block are not continguous!");
//
//     size_t newSize = GetPayloadSize(prevBlock) + GetPayloadSize(block);
//
//     prevBlock->header = newSize;
//     prevBlock->nextPhysical = block->nextPhysical;
//     prevBlock->prevFree = nullptr;
//     prevBlock->nextFree = nullptr;
//
//     block->nextPhysical = nullptr;
//     block->prevPhysical = nullptr;
//     block->nextFree = nullptr;
//     block->prevFree = nullptr;
//
//     return prevBlock;
// }
//
// void VulkanMetadata::MarkUsed(Block *block)
// {
//     LOG_ASSERT(block, "Block cannot be null!");
//
//     block->header |= 1U;
// }
//
// void VulkanMetadata::MarkFree(Block *block)
// {
//     LOG_ASSERT(block, "Block cannot be null!");
//
//     block->header &= ~1U;
// }
//
// bool VulkanMetadata::IsUsedBlock(Block *block)
// {
//     LOG_ASSERT(block, "Block cannot be null!");
//
//     return (block->header & 1U) > 0;
// }
//
// void VulkanMetadata::Destroy()
// {
//     LOG_ASSERT(m_allocationCount == 0, "Allocation Count muse be zero!");
//
//     FreeNodePool *pool = m_freeNodeHead;
//
//     while (pool)
//     {
//         FreeNodePool *nextPool = pool->nextPool;
//         if (m_cpuAllocator)
//         {
//             DestructFree(*m_cpuAllocator, pool);
//         }
//         else
//         {
//             aligned_free(pool);
//         }
//         pool = nextPool;
//     }
// }
//
// bool VulkanMetadata::CanAlloc(uint32_t alignment, size_t size, RequestAlloc &outRequest)
// {
//     LOG_ASSERT(is_power_of_2(alignment), "Request block cannot be null!");
//
//     alignment = std::max(alignment, s_minBaseAlignment);
//
//     if (size < s_minPayloadSize)
//     {
//         ++m_smallCount;
//     }
//     size = size < s_minPayloadSize ? s_minPayloadSize : size;
//     size_t alignedSize = align_up(size, alignment);
//
//     outRequest.block = nullptr;
//     outRequest.size = alignedSize;
//     outRequest.alignment = alignment;
//     outRequest.index = s_invalidIndex;
//
//     if (alignedSize > (m_memoryBlock->size - m_allocationSize))
//     {
//         return false;
//     }
//
//     Index index = MapIndex(alignedSize);
//
//     // TODO: go back here and implement an exhaustive search when the pool start to deplete?
//     if (alignedSize >= 512)
//     {
//         index = FindNextFreeIndex(index);
//     }
//
//     Block *suitableBlock = nullptr;
//
//     constexpr uint32_t baseSearchCount = 4;
//
//     do
//     {
//         if (index == s_invalidIndex)
//         {
//             return false;
//         }
//
//         Block *block = m_freeLists[index.fli * s_slCount + index.sli];
//         for (size_t idx = 0; idx < baseSearchCount; ++idx)
//         {
//             if (!block)
//             {
//                 break;
//             }
//
//             size_t alignedOffset = align_up(block->offset, alignment);
//
//             if (GetPayloadSize(block) - (alignedOffset - block->offset) >= alignedSize)
//             {
//                 suitableBlock = block;
//                 break;
//             }
//
//             block = block->nextFree;
//         }
//         // block = AcquireBlockFromList(index);
//
//         // RemoveFreeBlock(block);
//
//         if (suitableBlock == nullptr)
//         {
//             index = FindNextFreeIndex(index);
//             continue;
//         }
//
//         break;
//
//     } while (true);
//
//     outRequest.index = index;
//     outRequest.block = suitableBlock;
//     return true;
// }
//
// void VulkanMetadata::Alloc(const RequestAlloc &request, VulkanAllocation &allocation)
// {
//     LOG_ASSERT(request.block, "Request block cannot be null!");
//     LOG_ASSERT(is_power_of_2(request.alignment), "Request alignment must be power of 2!");
//     LOG_ASSERT(request.index != s_invalidIndex, "Request index cannot be invalid!");
//     LOG_ASSERT(request.size >= s_minPayloadSize, "Request size must be greater than &u!",
//                s_minPayloadSize);
//
//     size_t alignedOffset = align_up(request.block->offset, request.alignment);
//
//     RemoveFreeBlock(request.block);
//
//     ++m_allocationCount;
//
//     Block *allocatedBlock = request.block;
//     size_t offsetDistance = alignedOffset - allocatedBlock->offset;
//
//     // base alignment is 2, allocated size is aligned up by at least 2, there is no way that
//     // offset distance is 1
//     LOG_ASSERT(offsetDistance != 1, "Offset distance cannot be 1!");
//
//     if (offsetDistance != 0)
//     {
//         if (CanSplit(allocatedBlock, offsetDistance))
//         {
//             allocatedBlock = Split(allocatedBlock, alignedOffset - allocatedBlock->offset);
//             InsertFreeBlock(request.block);
//         }
//         else
//         {
//             allocatedBlock->offset += offsetDistance;
//             // prev physical should glob this fragmented memory
//
//             Block *prevPhysical = request.block->prevPhysical;
//             // likely never happen?
//             LOG_ASSERT(prevPhysical, "Prev physical is nullptr");
//
//             // we can still preserve the flag because offsetDistance is guarantee greater than 1
//             prevPhysical->header += (offsetDistance);
//
//             m_allocationSize += offsetDistance;
//             ++m_internalFragmentCount;
//         }
//     }
//
//     if (CanSplit(allocatedBlock, request.size))
//     {
//         Block *newBlock = Split(allocatedBlock, request.size);
//         InsertFreeBlock(newBlock);
//     }
//     else
//     {
//         ++m_internalFragmentCount;
//     }
//
//     m_allocationSize += GetPayloadSize(request.block);
//     ++m_lineTimeAllocationCount;
//     MarkUsed(allocatedBlock);
//
//     LOG_ASSERT(allocatedBlock->offset % request.alignment == 0, "Offset is not aligned!");
//     allocation.metadataHandle = RCAST(void *, allocatedBlock);
//     allocation.offset = allocatedBlock->offset;
//     allocation.size = GetPayloadSize(allocatedBlock);
//     allocation.vlkMemoryBlock = m_memoryBlock;
//     allocation.memory = m_memoryBlock->memory;
//     allocation.isPersistentMapped = false;
// }
//
// void VulkanMetadata::Free(Block *block)
// {
//     LOG_ASSERT(block, "Block cannot be null");
//
//     Block *freeBlock = block;
//     MarkFree(freeBlock);
//     --m_allocationCount;
//     m_allocationSize -= GetPayloadSize(freeBlock);
//
//     Block *prev = freeBlock->prevPhysical;
//     Block *next = freeBlock->nextPhysical;
//
//     if (prev && !IsUsedBlock(prev))
//     {
//         RemoveFreeBlock(prev);
//         freeBlock = Coalesce(prev, block);
//
//         block->~Block();
//
//         FreeNode *node = new (block) FreeNode();
//
//         node->next = m_freeNode;
//         m_freeNode = node;
//     }
//
//     if (next && !IsUsedBlock(next))
//     {
//         RemoveFreeBlock(next);
//         freeBlock = Coalesce(freeBlock, next);
//
//         next->~Block();
//
//         FreeNode *node = new (next) FreeNode();
//
//         node->next = m_freeNode;
//         m_freeNode = node;
//     }
//
//     InsertFreeBlock(freeBlock);
//     MarkFree(freeBlock);
// }
//
// void VulkanMetadata::Print()
// {
//     Block *block = m_head;
//
//
//     VkPhysicalDeviceMemoryProperties2 properties{
//         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
//         .pNext = nullptr,
//     };
//
//     vkGetPhysicalDeviceMemoryProperties2(m_memoryBlock->pool->allocator->GetPhysicalDevice(), &properties);
//
//     // TODO: change this to string pool or sth
//     std::stringstream ss;
//     char buffer[128];
//     MemoryTypeFlagsToString(properties.memoryProperties.memoryTypes[m_memoryBlock->memoryTypeIndex].propertyFlags, buffer, 128);
//     ss << "=============[DEVICE MEMORY]============\n";
//     ss << "Pool index: " << m_memoryBlock->memoryTypeIndex << "\n";
//     ss << "Memory type Index: " << m_memoryBlock->memoryTypeIndex << "\n";
//     ss << "Flags: " << buffer << "\n"; 
//     ss << "Total Size: " << m_memoryBlock->size << "\n";
//     ss << "Current allocation count: " << m_allocationCount << "\n";
//     ss << "Small allocation count: " << m_smallCount << "\n";
//     ss << "Internal fragment count: " << m_internalFragmentCount << "\n";
//     ss << "Live time allocation count: " << m_lineTimeAllocationCount << "\n";
//     ss << "Allocation size: " << m_allocationSize << "\n";
//     ss << "Remaining size: " << m_memoryBlock->size - m_allocationSize << "\n";
//     ss << "Free node count: " << m_cpuAllocationCount << "\n";
//
//     ss << "========================================\n";
//     while (block)
//     {
//         if (IsUsedBlock(block))
//         {
//             ss << "[Used][Offset: " << block->offset << " - " << GetPayloadSize(block) << "]\n";
//         }
//         else
//         {
//             ss << "[Free][Offset: " << block->offset << " - " << GetPayloadSize(block) << "]\n";
//         }
//         block = block->nextPhysical;
//     }
//     ss << "========================================\n";
//     std::cout << ss.str() << std::endl;
// }
//
// // ====================================================
// //                     VulkanAllocator
// // ====================================================
//
// void VulkanAllocator::Init(VkPhysicalDevice physicalDevice, VkDevice device,
//                            bool isMemoryBudgetEXTSupported, bool isDedicatedAllocationEXTSupported,
//                            IAllocator *allocator)
// {
//     m_device = device;
//     m_physicalDevice = physicalDevice;
//     m_cpuAllocator = allocator;
//     m_isMemoryBudgetEXTSupported = isMemoryBudgetEXTSupported;
//     m_isDedicatedAllocationEXTSupported = isDedicatedAllocationEXTSupported;
//
//     VkPhysicalDeviceProperties2 physicalDeviceProperties{
//         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
//         .pNext = nullptr,
//     };
//
//     vkGetPhysicalDeviceProperties2(m_physicalDevice, &physicalDeviceProperties);
//
//     m_maxAllocation = physicalDeviceProperties.properties.limits.maxMemoryAllocationCount;
//     m_alignmentLimits = {
//         .minTexelBufferOffsetAlignment =
//             physicalDeviceProperties.properties.limits.minTexelBufferOffsetAlignment,
//         .minUniformBufferOffsetAlignment =
//             physicalDeviceProperties.properties.limits.minUniformBufferOffsetAlignment,
//         .minStorageBufferOffsetAlignment =
//             physicalDeviceProperties.properties.limits.minStorageBufferOffsetAlignment,
//         .bufferImageGranularity = physicalDeviceProperties.properties.limits.bufferImageGranularity,
//     };
//     // 8 fucking arenas
//     // m_bufferArenas.emplace(8, allocator);
//
//     LOG_ASSERT(m_device != VK_NULL_HANDLE, "Logical device is null!");
//     LOG_ASSERT(m_physicalDevice != VK_NULL_HANDLE, "Physical device is null!");
//
//     VkPhysicalDeviceMemoryProperties2 properties{
//         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
//         .pNext = nullptr,
//     };
//
//     vkGetPhysicalDeviceMemoryProperties2(m_physicalDevice, &properties);
//
//     m_memoryTypeCount = properties.memoryProperties.memoryTypeCount;
//     m_memoryHeapCount = properties.memoryProperties.memoryHeapCount;
//
//     UpdateBudget();
//
//     for (size_t idx = 0; idx < m_memoryTypeCount; ++idx)
//     {
//         m_pools[idx].allocator = this;
//         m_pools[idx].memoryType = properties.memoryProperties.memoryTypes[idx];
//         m_pools[idx].memoryTypeIndex = idx;
//         // m_pools[idx].linearMemoryBlocks.init(m_cpuAllocator, 8);
//         // m_pools[idx].nonLinearMemoryBlocks.init(m_cpuAllocator, 8);
//     }
//
//     // PrintMemoryType();
// }
//
// void VulkanAllocator::UpdateBudget()
// {
//     VkPhysicalDeviceMemoryBudgetPropertiesEXT memoryBudget{
//         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT,
//         .pNext = nullptr,
//     };
//
//     VkPhysicalDeviceMemoryProperties2 properties{
//         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
//         .pNext = &memoryBudget,
//     };
//
//     vkGetPhysicalDeviceMemoryProperties2(m_physicalDevice, &properties);
//
//     for (size_t idx = 0; idx < m_memoryHeapCount; ++idx)
//     {
//         m_heaps[idx].budget = memoryBudget.heapBudget[idx];
//         m_heaps[idx].usage = memoryBudget.heapUsage[idx];
//
//         if (m_heaps[idx].budget <= 0 ||
//             m_heaps[idx].budget > properties.memoryProperties.memoryHeaps[idx].size)
//         {
//             LOG_ASSERT(true, "Budget is invalid!")
//             // m_heaps[idx].budget = properties.memoryProperties.memoryHeaps[idx].size;
//         }
//
//         if (m_heaps[idx].usage >= m_heaps[idx].budget)
//         {
//             LOG_ASSERT(true, "Usage is invalid!")
//             // m_heaps[idx].usage = m_heaps[idx].trackedByte;
//         }
//     }
// }
//
// void VulkanAllocator::Destroy()
// {
//     PrintPool();
//
//     for (size_t idx = 0; idx < m_memoryTypeCount; ++idx)
//     {
//         VulkanPool &pool = m_pools[idx];
//
//         auto &linear = pool.linearMemoryBlocks;
//         auto &nonLinear = pool.nonLinearMemoryBlocks;
//
//         for (size_t linearIdx = 0; linearIdx < linear.get_count(); ++linearIdx)
//         {
//             VulkanDeviceMemoryBlock &memoryBlock = linear[linearIdx];
//
//             LOG_ASSERT(memoryBlock.metadata->GetAllocationCount() == 0,
//                        "Free all resource before destroying memory block!");
//
//             LOG_ASSERT(memoryBlock.allocations.has_value(),
//                        "Vulkan allocations of memory block is null!");
//
//             memoryBlock.metadata->Destroy();
//             DestructFree(*m_cpuAllocator, memoryBlock.metadata);
//
//             vkFreeMemory(m_device, memoryBlock.memory, nullptr);
//         }
//
//         for (size_t nonLinearIdx = 0; nonLinearIdx < nonLinear.get_count(); ++nonLinearIdx)
//         {
//             VulkanDeviceMemoryBlock &memoryBlock = nonLinear[nonLinearIdx];
//
//             LOG_ASSERT(memoryBlock.metadata->GetAllocationCount() == 0,
//                        "Free all resource before destroying memory block!");
//
//             LOG_ASSERT(memoryBlock.allocations.has_value(),
//                        "Vulkan allocations of memory block is null!");
//
//             memoryBlock.metadata->Destroy();
//             DestructFree(*m_cpuAllocator, memoryBlock.metadata);
//
//             vkFreeMemory(m_device, memoryBlock.memory, nullptr);
//         }
//
//         linear.destroy(m_cpuAllocator);
//         nonLinear.destroy(m_cpuAllocator);
//     }
// }
//
// VulkanAllocationHandle VulkanAllocator::Alloc(size_t alignment, size_t size,
//                                               uint32_t memoryTypeIndex,
//                                               VulkanSuballocationType type)
// {
//     switch (type)
//     {
//     case VulkanSuballocationType::VULKAN_BUFFER:
//     case VulkanSuballocationType::VULKAN_IMAGE_LINEAR:
//     {
//         VulkanAllocation allocation;
//
//         if (type == VulkanSuballocationType::VULKAN_BUFFER)
//         {
//             allocation.suballocationType = VulkanSuballocationType::VULKAN_BUFFER;
//         }
//         else if (type == VulkanSuballocationType::VULKAN_IMAGE_LINEAR)
//         {
//             allocation.suballocationType = VulkanSuballocationType::VULKAN_IMAGE_LINEAR;
//         }
//
//         VulkanPool &pool = m_pools[memoryTypeIndex];
//
//         for (size_t idx = 0; idx < pool.linearMemoryBlocks.get_count(); ++idx)
//         {
//             auto &memoryBlock = pool.linearMemoryBlocks[idx];
//
//             VulkanMetadata::RequestAlloc req;
//             if (memoryBlock.metadata->CanAlloc(alignment, size, req))
//             {
//                 LOG_ASSERT(memoryBlock.allocations.has_value(),
//                            "Vulkan allocation vector of memory block failed to init!");
//
//                 memoryBlock.metadata->Alloc(req, allocation);
//
//                 return memoryBlock.allocations->Insert(std::move(allocation));
//             }
//         }
//
//         // request more device memory
//
//         AllocateDeviceMemory(&m_pools[memoryTypeIndex], s_baseMemorySize, memoryTypeIndex, true);
//
//         auto &newMemoryBlock = pool.linearMemoryBlocks[pool.linearMemoryBlocks.get_count() - 1];
//         VulkanMetadata::RequestAlloc req;
//         if (newMemoryBlock.metadata->CanAlloc(alignment, size, req))
//         {
//             LOG_ASSERT(newMemoryBlock.allocations.has_value(),
//                        "Vulkan allocation vector of memory block failed to init!");
//             newMemoryBlock.metadata->Alloc(req, allocation);
//
//             return newMemoryBlock.allocations->Insert(std::move(allocation));
//         }
//
//         break;
//     }
//     case VulkanSuballocationType::VULKAN_IMAGE_OPTIMAL:
//     {
//         VulkanAllocation allocation;
//         allocation.suballocationType = VulkanSuballocationType::VULKAN_IMAGE_OPTIMAL;
//         VulkanPool &pool = m_pools[memoryTypeIndex];
//
//         for (size_t idx = 0; idx < pool.nonLinearMemoryBlocks.get_count(); ++idx)
//         {
//             auto &memoryBlock = pool.nonLinearMemoryBlocks[idx];
//
//             VulkanMetadata::RequestAlloc req;
//             if (memoryBlock.metadata->CanAlloc(alignment, size, req))
//             {
//                 LOG_ASSERT(memoryBlock.allocations.has_value(),
//                            "Vulkan allocation vector of memory block failed to init!");
//                 memoryBlock.metadata->Alloc(req, allocation);
//
//                 return memoryBlock.allocations->Insert(std::move(allocation));
//             }
//         }
//
//         // request more device memory
//         AllocateDeviceMemory(&m_pools[memoryTypeIndex], s_baseMemorySize, memoryTypeIndex, false);
//
//         auto &newMemoryBlock =
//             pool.nonLinearMemoryBlocks[pool.nonLinearMemoryBlocks.get_count() - 1];
//         VulkanMetadata::RequestAlloc req;
//         if (newMemoryBlock.metadata->CanAlloc(alignment, size, req))
//         {
//             LOG_ASSERT(newMemoryBlock.allocations.has_value(),
//                        "Vulkan allocation vector of memory block failed to init!");
//             newMemoryBlock.metadata->Alloc(req, allocation);
//
//             return newMemoryBlock.allocations->Insert(std::move(allocation));
//         }
//
//         break;
//     }
//     };
//
//     return VulkanAllocationHandle();
// }
//
// void VulkanAllocator::AllocateDeviceMemory(VulkanPool *pool, VkDeviceSize size,
//                                            uint32_t memoryTypeIndex, bool is_linear)
// {
//     if (is_linear)
//     {
//         auto &linear = pool->linearMemoryBlocks;
//         VulkanHeap &heap = m_heaps[pool->memoryType.heapIndex];
//
//         if (linear.get_capacity() == 0)
//         {
//             linear.init(m_cpuAllocator, 8);
//         }
//         linear.emplace_back(m_cpuAllocator);
//
//         // void *metadataAddr = m_cpuAllocator->Alloc(sizeof(VulkanMetadata));
//         void *metadata_addr = V_ALLOC(m_cpuAllocator, sizeof(VulkanMetadata));
//         ENGINE_ASSERT(metadata_addr, "Failed to allocate metadata for new device memory!");
//         uint32_t mem_idx = linear.get_count() - 1;
//         VulkanDeviceMemoryBlock &new_mem_block = linear[mem_idx];
//
//         VkDeviceSize remaining_size = heap.budget - heap.usage;
//         // TODO: questionable? Find better way to handle this
//         // Either force evicting resource or allocate host memory
//         if (size > remaining_size)
//         {
//             size = remaining_size;
//         }
//
//         new_mem_block.size = size;
//         new_mem_block.memoryTypeIndex = memoryTypeIndex;
//         new_mem_block.allocations.emplace(4, 8, m_cpuAllocator);
//         new_mem_block.mappingCount = 0;
//         new_mem_block.pool = pool;
//         new_mem_block.metadata =
//             new (metadata_addr) VulkanMetadata(&linear[mem_idx], m_cpuAllocator);
//
//         VkMemoryAllocateInfo alloc_info{
//             .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
//             .pNext = nullptr,
//             .allocationSize = size,
//             .memoryTypeIndex = memoryTypeIndex,
//         };
//
//         VkDeviceMemory memory;
//         vkAllocateMemory(m_device, &alloc_info, nullptr, &memory);
//         new_mem_block.memory = memory;
//     }
//     else
//     {
//         auto &non_linear = pool->nonLinearMemoryBlocks;
//         VulkanHeap &heap = m_heaps[pool->memoryType.heapIndex];
//
//         if (non_linear.get_capacity() == 0)
//         {
//             non_linear.init(m_cpuAllocator, 8);
//         }
//         non_linear.emplace_back(m_cpuAllocator);
//
//         // void *metadataAddr = m_cpuAllocator->Alloc(sizeof(VulkanMetadata));
//         void *metadata_addr = V_ALLOC(m_cpuAllocator, sizeof(VulkanMetadata));
//         ENGINE_ASSERT(metadata_addr, "Failed to allocate metadata for new device memory!");
//         uint32_t mem_idx = non_linear.get_count() - 1;
//         VulkanDeviceMemoryBlock &new_mem_block = non_linear[mem_idx];
//
//         VkDeviceSize remaining_size = heap.budget - heap.usage;
//         // TODO: questionable? Find better way to handle this
//         // Either force evicting resource or allocate host memory
//         if (size > remaining_size)
//         {
//             size = remaining_size;
//         }
//
//         new_mem_block.size = size;
//         new_mem_block.memoryTypeIndex = memoryTypeIndex;
//         new_mem_block.allocations.emplace(4, 8, m_cpuAllocator);
//         new_mem_block.mappingCount = 0;
//         new_mem_block.pool = pool;
//         new_mem_block.metadata =
//             new (metadata_addr) VulkanMetadata(&non_linear[mem_idx], m_cpuAllocator);
//
//         VkMemoryAllocateInfo alloc_info{
//             .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
//             .pNext = nullptr,
//             .allocationSize = size,
//             .memoryTypeIndex = memoryTypeIndex,
//         };
//
//         VkDeviceMemory memory;
//         vkAllocateMemory(m_device, &alloc_info, nullptr, &memory);
//         new_mem_block.memory = memory;
//     }
//
//     UpdateBudget();
// }
//
// void VulkanAllocator::Free(VulkanAllocationHandle handle)
// {
//     VulkanAllocation &allocation = *handle;
//     VulkanDeviceMemoryBlock &memoryBlock = *allocation.vlkMemoryBlock;
//     VulkanMetadata::Block *block = RCAST(VulkanMetadata::Block *, allocation.metadataHandle);
//
//     if (allocation.isPersistentMapped)
//     {
//         --memoryBlock.mappingCount;
//         LOG_ASSERT(memoryBlock.mappingCount >= 0, "Mapping Count is less than 0!");
//     }
//
//     memoryBlock.metadata->Free(block);
//     memoryBlock.allocations->Erase(handle);
//
//     UpdateBudget();
// }
//
// void VulkanAllocator::AllocateBufferArena(uint32_t tag, size_t size) {}
//
// static constexpr uint32_t k_invalidMemoryTypeIndex = UINT32_MAX;
//
// uint32_t VulkanAllocator::ChooseSuitableMemoryTypeIndex(uint32_t memoryTypeBits,
//                                                         VulkanMemoryUsage usage,
//                                                         VulkanMemoryFlag flag)
// {
//     uint32_t bestIndex = k_invalidMemoryTypeIndex;
//     bool isCached = (flag & VULKAN_MEMORY_FLAG_CPU_RANDOM_ACCESS_BIT) > 0;
//     bool isSequentialWrite = (flag & VULKAN_MEMORY_FLAG_CPU_SEQUENTIAL_WRITE_BIT) > 0;
//
//     int32_t bestScore = 0;
//
//     switch (usage)
//     {
//     case VulkanMemoryUsage::VULKAN_MEMORY_USAGE_GPU_ONLY:
//     {
//         for (size_t idx = 0; idx < m_memoryTypeCount; ++idx)
//         {
//             if ((memoryTypeBits & (1U << idx)) == 0)
//             {
//                 continue;
//             }
//
//             int32_t score = 0;
//             VkMemoryType memoryType = m_pools[idx].memoryType;
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) > 0)
//             {
//                 score += 100;
//             }
//             else
//             {
//                 score -= 100;
//             }
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) > 0)
//             {
//                 score -= 10;
//             }
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) > 0)
//             {
//                 score -= 5;
//             }
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) > 0)
//             {
//                 score -= 5;
//             }
//
//             if (score > bestScore)
//             {
//                 bestScore = score;
//                 bestIndex = idx;
//             }
//         }
//         break;
//     }
//     case VulkanMemoryUsage::VULKAN_MEMORY_USAGE_CPU_ONLY:
//     {
//         for (size_t idx = 0; idx < m_memoryTypeCount; ++idx)
//         {
//             if ((memoryTypeBits & (1U << idx)) == 0)
//             {
//                 continue;
//             }
//
//             int32_t score = 0;
//             VkMemoryType memoryType = m_pools[idx].memoryType;
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) > 0)
//             {
//                 score += 100;
//             }
//             else
//             {
//                 score -= 100;
//             }
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) > 0)
//             {
//                 score -= 20;
//             }
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) > 0)
//             {
//                 score += 5;
//             }
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) > 0)
//             {
//                 if (isCached)
//                 {
//                     score += 5;
//                 }
//                 if (isSequentialWrite)
//                 {
//                     score -= 10;
//                 }
//             }
//
//             if (score > bestScore)
//             {
//                 bestScore = score;
//                 bestIndex = idx;
//             }
//         }
//         break;
//     }
//     case VulkanMemoryUsage::VULKAN_MEMORY_USAGE_CPU_AND_GPU:
//     {
//         for (size_t idx = 0; idx < m_memoryTypeCount; ++idx)
//         {
//             if ((memoryTypeBits & (1U << idx)) == 0)
//             {
//                 continue;
//             }
//
//             int32_t score = 0;
//             VkMemoryType memoryType = m_pools[idx].memoryType;
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) > 0)
//             {
//                 score += 50;
//             }
//             else
//             {
//                 score -= 100;
//             }
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) > 0)
//             {
//                 score += 50;
//             }
//             else
//             {
//                 score -= 100;
//             }
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) > 0)
//             {
//                 score += 10;
//             }
//
//             if ((memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) > 0)
//             {
//                 score += 10;
//             }
//
//             if (score > bestScore)
//             {
//                 bestScore = score;
//                 bestIndex = idx;
//             }
//         }
//         break;
//     }
//     };
//
//     return bestIndex;
// }
//
// bool VulkanAllocator::IsMemoryTypeIndexLocal(uint32_t memoryTypeIndex)
// {
//     if ((m_pools[memoryTypeIndex].memoryType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) >
//         0)
//     {
//         return true;
//     }
//
//     return false;
// }
//
// bool VulkanAllocator::IsMemoryTypeIndexHost(uint32_t memoryTypeIndex)
// {
//     if ((m_pools[memoryTypeIndex].memoryType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) >
//         0)
//     {
//         return true;
//     }
//
//     return false;
// }
//
// //void VulkanAllocator::PrintMemoryType()
// //{
// //    for (size_t idx = 0; idx < m_memoryTypeCount; ++idx)
// //    {
// //        MemoryTypeFlagsToString(m_pools[idx].memoryType.propertyFlags);
// //    }
// //}
//
// void VulkanAllocator::PrintPool()
// {
//     for (size_t idx = 0; idx < m_memoryTypeCount; ++idx)
//     {
//         for (size_t linearIdx = 0; linearIdx < m_pools[idx].linearMemoryBlocks.get_count();
//              ++linearIdx)
//         {
//             std::cout << "--------------- Linear Pool ---------------" << std::endl;
//             m_pools[idx].linearMemoryBlocks[linearIdx].metadata->Print();
//         }
//
//         for (size_t nonLinearIdx = 0; nonLinearIdx < m_pools[idx].nonLinearMemoryBlocks.get_count();
//              ++nonLinearIdx)
//         {
//             std::cout << "--------------- Non Linear Pool ---------------" << std::endl;
//             m_pools[idx].nonLinearMemoryBlocks[nonLinearIdx].metadata->Print();
//         }
//     }
// }
//
// // ====================================================
// //                    VulkanBackend
// // ====================================================
//
// void MemoryTypeFlagsToString(VkMemoryPropertyFlags flags, char* buffer, size_t size)
// {
//     // static char buffer[256];
//     LOG_ASSERT(size >= 128, "");
//     buffer[0] = '\0';
//
//     if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
//         strcat_s(buffer, size, "DEVICE_LOCAL | ");
//
//     if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
//         strcat_s(buffer, size, "HOST_VISIBLE | ");
//
//     if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
//         strcat_s(buffer, size, "HOST_COHERENT | ");
//
//     if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
//         strcat_s(buffer, size, "HOST_CACHED | ");
//
//     if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
//         strcat_s(buffer, size, "LAZILY_ALLOCATED | ");
//
// }
