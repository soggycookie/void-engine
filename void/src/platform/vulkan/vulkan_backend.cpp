// #include "vulkan_backend.h"
// #include "application.h"
// #include "logger/log.h"
// #include "stb/stb_image.h"
//
// namespace VoidEngine
// {
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
//
// VulkanBuffer VulkanBackend::CreateBuffer(VulkanAllocationInfo &allocationInfo,
//                                          VkBufferCreateInfo *bufferCreateInfo,
//                                          VulkanMemoryUsage usage, VulkanMemoryFlag flag)
// {
//     VulkanBuffer vulkanBuffer;
//
//     vkCreateBuffer(m_context.device, bufferCreateInfo, nullptr, &vulkanBuffer.buffer);
//
//     VkMemoryRequirements2 memoryReqs{
//         .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
//         .pNext = nullptr,
//     };
//
//     VkBufferMemoryRequirementsInfo2 bufferMemReq2{
//         .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
//         .pNext = nullptr,
//         .buffer = vulkanBuffer.buffer,
//     };
//     vkGetBufferMemoryRequirements2(m_context.device, &bufferMemReq2, &memoryReqs);
//
//     uint32_t memoryTypeIndex = m_vulkanAllocator->ChooseSuitableMemoryTypeIndex(
//         memoryReqs.memoryRequirements.memoryTypeBits, usage, flag);
//
//     if (memoryTypeIndex == k_invalidMemoryTypeIndex)
//     {
//         LOG_WARN("VULKAN", "No memory type index of buffer is found!");
//         vkDestroyBuffer(m_context.device, vulkanBuffer.buffer, nullptr);
//         return {VK_NULL_HANDLE, VulkanAllocationHandle()};
//     }
//
//     VkDeviceSize alignment = memoryReqs.memoryRequirements.alignment;
//
//     if (bufferCreateInfo->usage == VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
//     {
//         alignment = std::max(
//             alignment, m_vulkanAllocator->GetAlignmentLimits().minUniformBufferOffsetAlignment);
//     }
//     else if (bufferCreateInfo->usage == VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
//     {
//         alignment = std::max(
//             alignment, m_vulkanAllocator->GetAlignmentLimits().minStorageBufferOffsetAlignment);
//     }
//
//     vulkanBuffer.handle =
//         m_vulkanAllocator->Alloc(alignment, memoryReqs.memoryRequirements.size, memoryTypeIndex,
//                                  VulkanSuballocationType::VULKAN_BUFFER);
//
//     if (vulkanBuffer.handle == VulkanAllocationHandle())
//     {
//         // TODO: evict resource or whatever
//         LOG_FATAL("Internal Vulkan", "Failed to suballocate buffer!");
//     }
//     else
//     {
//         vkBindBufferMemory(m_context.device, vulkanBuffer.buffer, (*vulkanBuffer.handle).memory,
//                            (*vulkanBuffer.handle).offset);
//     }
//
//     if (m_vulkanAllocator->IsMemoryTypeIndexHost(memoryTypeIndex) &&
//         (flag & VULKAN_MEMORY_FLAG_PERSISTANT_MAPPED_BIT) > 0)
//     {
//         VulkanAllocation &allocation = *vulkanBuffer.handle;
//         VulkanDeviceMemoryBlock &memoryBlock = *allocation.vlkMemoryBlock;
//
//         if (memoryBlock.mappingCount == 0)
//         {
//             vkMapMemory(m_context.device, allocation.memory, 0, memoryBlock.size, 0,
//                         &memoryBlock.mapped);
//         }
//
//         ++memoryBlock.mappingCount;
//         allocationInfo.mappedData = OFFSET_ADDR(memoryBlock.mapped, allocation.offset);
//     }
//
//     return vulkanBuffer;
// }
//
// void VulkanBackend::DestroyBuffer(VulkanBuffer buffer)
// {
//     VulkanAllocation &allocation = *buffer.handle;
//
//     VulkanDeviceMemoryBlock &memoryBlock = *allocation.vlkMemoryBlock;
//
//     memoryBlock.metadata->Free(RCAST(VulkanMetadata::Block *, allocation.metadataHandle));
//     memoryBlock.allocations->Erase(buffer.handle);
//
//     vkDestroyBuffer(m_context.device, buffer.buffer, nullptr);
// }
//
// VulkanImage VulkanBackend::CreateImage(VkImageCreateInfo *imageCreateInfo, VulkanMemoryUsage usage,
//                                        VulkanMemoryFlag flag)
// {
//     VulkanImage vulkanImage;
//     vkCreateImage(m_context.device, imageCreateInfo, nullptr, &vulkanImage.image);
//
//     VkMemoryRequirements2 memoryReqs{
//         .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
//         .pNext = nullptr,
//     };
//
//     VkImageMemoryRequirementsInfo2 imageMemReqs{
//         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
//         .pNext = nullptr,
//         .image = vulkanImage.image,
//     };
//
//     vkGetImageMemoryRequirements2(m_context.device, &imageMemReqs, &memoryReqs);
//
//     uint32_t memoryTypeIndex = m_vulkanAllocator->ChooseSuitableMemoryTypeIndex(
//         memoryReqs.memoryRequirements.memoryTypeBits, usage, flag);
//
//     if (memoryTypeIndex == k_invalidMemoryTypeIndex)
//     {
//         LOG_WARN("VULKAN", "No memory type index of image is found!");
//         vkDestroyImage(m_context.device, vulkanImage.image, nullptr);
//         return {VK_NULL_HANDLE, VulkanAllocationHandle()};
//     }
//
//     VkDeviceSize alignment = memoryReqs.memoryRequirements.alignment;
//
//     if (imageCreateInfo->tiling == VK_IMAGE_TILING_LINEAR)
//     {
//         vulkanImage.handle =
//             m_vulkanAllocator->Alloc(alignment, memoryReqs.memoryRequirements.size, memoryTypeIndex,
//                                      VulkanSuballocationType::VULKAN_IMAGE_LINEAR);
//     }
//     else
//     {
//         vulkanImage.handle =
//             m_vulkanAllocator->Alloc(alignment, memoryReqs.memoryRequirements.size, memoryTypeIndex,
//                                      VulkanSuballocationType::VULKAN_IMAGE_OPTIMAL);
//     }
//
//     if (vulkanImage.handle == VulkanAllocationHandle())
//     {
//         // TODO: evict resource or whatever
//         LOG_FATAL("Internal Vulkan", "Failed to suballocate buffer!");
//     }
//     else
//     {
//         vkBindImageMemory(m_context.device, vulkanImage.image, (*vulkanImage.handle).memory,
//                           (*vulkanImage.handle).offset);
//     }
//
//     return vulkanImage;
// }
//
// void VulkanBackend::DestroyImage(VulkanImage image)
// {
//     VulkanAllocation &allocation = *image.handle;
//
//     VulkanDeviceMemoryBlock &memoryBlock = *allocation.vlkMemoryBlock;
//
//     memoryBlock.metadata->Free(RCAST(VulkanMetadata::Block *, allocation.metadataHandle));
//     memoryBlock.allocations->Erase(image.handle);
//
//     vkDestroyImage(m_context.device, image.image, nullptr);
// }
//
// void *VulkanBackend::MapMemory(VulkanBuffer &buffer)
// {
//     VulkanAllocation &allocation = *buffer.handle;
//     VulkanDeviceMemoryBlock &memoryBlock = *allocation.vlkMemoryBlock;
//     LOG_ASSERT(m_vulkanAllocator->IsMemoryTypeIndexHost(memoryBlock.memoryTypeIndex),
//                "Memory Type does not contain HOST_VISIBLE!");
//
//     if (memoryBlock.mappingCount == 0)
//     {
//         vkMapMemory(m_context.device, allocation.memory, 0, memoryBlock.size, 0,
//                     &memoryBlock.mapped);
//     }
//
//     ++memoryBlock.mappingCount;
//     return OFFSET_ADDR(memoryBlock.mapped, allocation.offset);
// }
//
// void VulkanBackend::UnmapMemory(VulkanBuffer &buffer)
// {
//     VulkanAllocation &allocation = *buffer.handle;
//     VulkanDeviceMemoryBlock &memoryBlock = *allocation.vlkMemoryBlock;
//     LOG_ASSERT(m_vulkanAllocator->IsMemoryTypeIndexHost(memoryBlock.memoryTypeIndex),
//                "Memory Type does not contain HOST_VISIBLE!");
//     LOG_ASSERT(memoryBlock.mappingCount > 0, "Memory block is already unmapped!");
//
//     --memoryBlock.mappingCount;
//     if (memoryBlock.mappingCount == 0)
//     {
//         vkUnmapMemory(m_context.device, memoryBlock.memory);
//     }
// }
//
// bool VulkanBackend::StartUp(Window *window, IAllocator *allocator)
// {
//     LOG_ASSERT(allocator, "Allocator cannot be null!");
//     m_window = window;
//     m_allocator = allocator;
//
//     CreateInstance();
//
// #ifdef VOID_DEBUG
//     SetUpDebugMessenger();
// #endif // VOID_DEBUG
//
//     CreateSurface();
//
//     CreatePhysicalDevice();
//     CreateLogicalDevice();
//
//     void *addr = V_ALIGNED_ALLOC(m_allocator, sizeof(VulkanAllocator), alignof(VulkanAllocator));
//     ENGINE_ASSERT(addr, "Failed to allocate vulkan allocator");
//
//     m_vulkanAllocator = new (addr) VulkanAllocator();
//     m_vulkanAllocator->Init(m_context.physicalDevice, m_context.device, true, true, m_allocator);
//
//     CreateSwapchain();
//     CreateSwapchainImageViews();
//
//     CreateSyncObjects();
//     CreateCommandPool();
//     CreateCommandBuffers();
//
//     CreateTextureImage();
//     CreateTextureSampler();
//
//     CreateDescriptorSetLayout();
//     CreateUniformBuffer();
//     CreateDescriptorPool();
//     CreateDescriptorSets();
//
//     CreateGraphicPipeline();
//     CreateDepthTexture();
//
//     CreateVertexBuffer();
//     CreateIndexBuffer();
//
//     return true;
// }
//
// void VulkanBackend::Shutdown()
// {
//     vkDestroySampler(m_context.device, m_sampler, nullptr);
//     vkDestroyImageView(m_context.device, m_texture.view, nullptr);
//     DestroyImage(m_texture.image);
//
//     for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
//     {
//         DestroyBuffer(m_uniformBuffers[idx]);
//         FLUSH_LOG();
//     }
//
//     vkDestroyDescriptorPool(m_context.device, m_descriptorPool, nullptr);
//     vkDestroyDescriptorSetLayout(m_context.device, m_descriptorSetLayout, nullptr);
//
//     DestroyBuffer(m_vertexBuffer);
//     DestroyBuffer(m_indexBuffer);
//
//     CleanUpDepthTexture();
//
//     for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
//     {
//         vkDestroySemaphore(m_context.device, m_frames[idx].presentCompleteSemaphore, nullptr);
//         vkDestroySemaphore(m_context.device, m_frames[idx].renderFinishedSemaphore, nullptr);
//         vkDestroyFence(m_context.device, m_frames[idx].drawFence, nullptr);
//     }
//     vkDestroyCommandPool(m_context.device, m_context.graphicCommandPool, nullptr);
//
//     vkDestroyPipeline(m_context.device, m_pipeline, nullptr);
//     vkDestroyPipelineLayout(m_context.device, m_pipelineLayout, nullptr);
//
//     for (size_t idx = 0; idx < m_context.swapchainImageCount; ++idx)
//     {
//         vkDestroyImageView(m_context.device, m_context.pSwapchainImageViews[idx], nullptr);
//     }
//     vkDestroySwapchainKHR(m_context.device, m_context.swapchain, nullptr);
//
//     V_FREE(m_allocator, m_context.pSwapchainImageViews);
//     V_FREE(m_allocator, m_context.pSwapchainImages);
//
//     m_vulkanAllocator->Destroy();
//     m_vulkanAllocator->~VulkanAllocator();
//     V_FREE(m_allocator, m_vulkanAllocator);
//
//     vkDestroySurfaceKHR(m_context.instance, m_context.surface, nullptr);
//     vkDestroyDevice(m_context.device, nullptr);
//
//     PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
//         (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
//             m_context.instance, "vkDestroyDebugUtilsMessengerEXT");
//
//     if (vkDestroyDebugUtilsMessengerEXT)
//     {
//         vkDestroyDebugUtilsMessengerEXT(m_context.instance, m_debugMessenger, nullptr);
//     }
//     else
//     {
//         LOG_ERROR("VULKAN", "Vulkan failed to acquire debug messenger destruction");
//
//         throw std::runtime_error("[VULKAN] Failed to acquire vkDestroyDebugUtilsMessengerEXT!");
//     }
//
//     vkDestroyInstance(m_context.instance, nullptr);
// }
//
// static bool IsExtAvailable(const std::vector<VkExtensionProperties> &extProperties,
//                            const char *checkingExt)
// {
//     for (const VkExtensionProperties &ext : extProperties)
//     {
//         if (strcmp(checkingExt, ext.extensionName))
//         {
//             return true;
//         }
//     }
//
//     return false;
// }
//
// static bool IsLayerAvailable(const std::vector<VkLayerProperties> &layerProperties,
//                              const char *checkinglayer)
// {
//     for (const VkLayerProperties &layer : layerProperties)
//     {
//         if (strcmp(checkinglayer, layer.layerName))
//         {
//             return true;
//         }
//     }
//
//     return false;
// }
//
// static VKAPI_ATTR VkBool32 VKAPI_CALL
// DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
//               VkDebugUtilsMessageTypeFlagsEXT messageTypes,
//               const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
// {
//     switch (messageSeverity)
//     {
//     case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
//     {
//         LOG_TRACE("VULKAN", "Trace: %s", pCallbackData->pMessage);
//         break;
//     }
//     case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
//     {
//         LOG_INFO("VULKAN", "Info: %s", pCallbackData->pMessage);
//         break;
//     }
//     case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
//     {
//         LOG_WARN("VULKAN", "Warn: %s", pCallbackData->pMessage);
//         break;
//     }
//     case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
//     {
//         LOG_ERROR("VULKAN", "Error: %s", pCallbackData->pMessage);
//         break;
//     }
//     default:
//     {
//         LOG_DEBUG("VULKAN", "Unknown: %s", pCallbackData->pMessage);
//         break;
//     }
//     }
//
//     return VK_FALSE;
// }
//
// void VulkanBackend::CreateSurface()
// {
//     LOG_ASSERT(m_window, "Vulkan window handle is null!");
// #ifdef _WIN32
//     Win32_Window *window = dynamic_cast<Win32_Window *>(m_window);
//
//     VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
//         .pNext = nullptr,
//         .flags = 0,
//         .hinstance = window->GetModuleInstanceHandle(),
//         .hwnd = window->GetNativeWindowHandle(),
//     };
//
//     if (vkCreateWin32SurfaceKHR(m_context.instance, &surfaceCreateInfo, nullptr,
//                                 &m_context.surface) != VK_SUCCESS)
//     {
//         LOG_ERROR("VULKAN", "Failed to create Win32 surface!");
//         throw std::runtime_error("[VULKAN] Failed to create Win32 surface!");
//     }
//
// #else
//     LOG_ERROR("VULKAN", "Failed to detect and create platform-dependent surface!");
//     throw std::runtime_error("[VULKAN] Failed to detect and create platform-dependent surface!");
// #endif
// }
//
// void VulkanBackend::SetUpDebugMessenger()
// {
//     VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
//         .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
//                            // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
//                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
//                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
//         .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
//                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
//                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
//         .pfnUserCallback = &DebugCallback,
//     };
//
//     PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
//         (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_context.instance,
//                                                                   "vkCreateDebugUtilsMessengerEXT");
//
//     if (vkCreateDebugUtilsMessengerEXT)
//     {
//         if (vkCreateDebugUtilsMessengerEXT(m_context.instance, &debugMessengerCreateInfo, nullptr,
//                                            &m_debugMessenger) != VK_SUCCESS)
//         {
//             LOG_ERROR("VULKAN", "Vulkan debug messenger creation failed!");
//
//             throw std::runtime_error("[VULKAN] Failed to create debug messenger!");
//         }
//     }
//     else
//     {
//         LOG_ERROR("VULKAN", "Failed to acquire vkCreateDebugUtilsMessengerEXT");
//
//         throw std::runtime_error("[VULKAN] Failed to acquire vkCreateDebugUtilsMessengerEXT!");
//     }
// }
//
// void VulkanBackend::CreateLogicalDevice()
// {
//     uint32_t queueFamilyPropCount = 0;
//
//     vkGetPhysicalDeviceQueueFamilyProperties2(m_context.physicalDevice, &queueFamilyPropCount,
//                                               nullptr);
//     std::array<VkQueueFamilyProperties2, 16> queueFamilyProps;
//     LOG_ASSERT(queueFamilyPropCount <= 16, "queueFamilyPropCount is bigger than 16");
//
//     for (auto &prop : queueFamilyProps)
//     {
//         prop.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
//         prop.pNext = nullptr;
//     }
//
//     vkGetPhysicalDeviceQueueFamilyProperties2(m_context.physicalDevice, &queueFamilyPropCount,
//                                               queueFamilyProps.data());
//
//     for (size_t idx = 0; idx < queueFamilyProps.size(); ++idx)
//     {
//         if (m_context.graphicQueueFamilyIndex != ~0)
//             break;
//
//         if (m_context.graphicQueueFamilyIndex == ~0 && // only look if not found yet
//             (queueFamilyProps[idx].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0)
//         {
//             VkBool32 supportSurface = false;
//             vkGetPhysicalDeviceSurfaceSupportKHR(m_context.physicalDevice, idx, m_context.surface,
//                                                  &supportSurface);
//             if (supportSurface)
//             {
//                 m_context.graphicQueueFamilyIndex = idx;
//             }
//         }
//     }
//
//     if (m_context.graphicQueueFamilyIndex == ~0)
//     {
//         LOG_ERROR("VULKAN", "Could not find a queue for graphics and present -> terminating");
//         throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
//     }
//
//     float queuePriority = 0.5f;
//     VkDeviceQueueCreateInfo graphicDeviceQueueCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
//         .pNext = nullptr,
//         .queueFamilyIndex = m_context.graphicQueueFamilyIndex,
//         .queueCount = 1,
//         .pQueuePriorities = &queuePriority,
//     };
//
//     uint32_t deviceQueueCreateInfoCount = 1;
//     VkDeviceQueueCreateInfo deviceQueueCreateInfos[] = {graphicDeviceQueueCreateInfo};
//
//     VkPhysicalDeviceVulkan11Features vulkan11features{
//         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
//         .pNext = nullptr,
//         .shaderDrawParameters = VK_TRUE,
//     };
//
//     VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{
//         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
//         .pNext = &vulkan11features,
//         .extendedDynamicState = VK_TRUE,
//     };
//
//     VkPhysicalDeviceFeatures2 features2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
//                                         .pNext = &extendedDynamicStateFeatures};
//
//     std::array<const char *, 2> requiredDeviceExtension = {"VK_KHR_swapchain",
//                                                            "VK_EXT_memory_budget"};
//
//     VkDeviceCreateInfo deviceCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
//         .pNext = &features2,
//         .flags = 0,
//         .queueCreateInfoCount = deviceQueueCreateInfoCount,
//         .pQueueCreateInfos = deviceQueueCreateInfos,
//         .enabledLayerCount = 0,
//         .ppEnabledLayerNames = nullptr,
//         .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
//         .ppEnabledExtensionNames = requiredDeviceExtension.data(),
//         .pEnabledFeatures = nullptr,
//     };
//
//     VpDeviceCreateInfo vpDeviceCreateInfo{
//         .pCreateInfo = &deviceCreateInfo,
//         .enabledFullProfileCount = 1,
//         .pEnabledFullProfiles = &profile,
//     };
//
//     VkResult result =
//         vpCreateDevice(m_context.physicalDevice, &vpDeviceCreateInfo, nullptr, &m_context.device);
//
//     if (result != VK_SUCCESS)
//     {
//         LOG_ERROR("VULKAN", "Failed to create logical device!");
//
//         throw std::runtime_error("[VULKAN] Failed to create logical device!");
//     }
//
//     vkGetDeviceQueue(m_context.device, m_context.graphicQueueFamilyIndex, 0,
//                      &m_context.graphicQueue);
//
//     LOG_ASSERT(m_context.graphicQueue != VK_NULL_HANDLE, "Graphic Queue is null!");
// }
//
// VkSurfaceFormatKHR VulkanBackend::ChooseSurfaceFormat(VkSurfaceFormatKHR *formats,
//                                                       uint32_t formatCount)
// {
//     for (size_t idx = 0; idx < formatCount; ++idx)
//     {
//         const VkSurfaceFormatKHR &format = formats[idx];
//         if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
//             format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
//         {
//             return format;
//         }
//     }
//
//     LOG_WARN("[VULKAN]", "Surface does not have ideal format!");
//
//     return formats[0];
// }
//
// VkPresentModeKHR VulkanBackend::ChoosePresentMode(VkPresentModeKHR *modes, uint32_t modeCount)
// {
//     for (size_t idx = 0; idx < modeCount; ++idx)
//     {
//         const VkPresentModeKHR &mode = modes[idx];
//         if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
//         {
//             return mode;
//         }
//     }
//
//     LOG_INFO("VULKAN", "Present mode does not support MAILBOX!");
//
//     return VK_PRESENT_MODE_FIFO_KHR;
// }
//
// VkExtent2D VulkanBackend::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities)
// {
//     if (capabilities.currentExtent.width != UINT32_MAX)
//     {
//         return capabilities.currentExtent;
//     }
//
//     uint32_t width = m_window->GetFramebufferSize().width;
//     uint32_t height = m_window->GetFramebufferSize().height;
//
//     VkExtent2D extent = {
//         .width = width,
//         .height = height,
//     };
//
//     extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
//                               capabilities.maxImageExtent.width);
//     extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
//                                capabilities.maxImageExtent.height);
//
//     return extent;
// }
//
// void VulkanBackend::CreateSwapchain(VkSwapchainKHR oldSwapchain)
// {
//     VkSurfaceCapabilitiesKHR surfaceCapabilities{};
//     vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_context.physicalDevice, m_context.surface,
//                                               &surfaceCapabilities);
//
//     uint32_t surfaceFormatCount = 0;
//     vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.physicalDevice, m_context.surface,
//                                          &surfaceFormatCount, nullptr);
//
//     std::array<VkSurfaceFormatKHR, 16> surfaceFormats;
//     LOG_ASSERT(surfaceFormatCount <= 16, "surfaceFormatCount: %u", surfaceFormatCount);
//
//     vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.physicalDevice, m_context.surface,
//                                          &surfaceFormatCount, surfaceFormats.data());
//
//     uint32_t presentModeCount = 0;
//     vkGetPhysicalDeviceSurfacePresentModesKHR(m_context.physicalDevice, m_context.surface,
//                                               &presentModeCount, nullptr);
//
//     std::array<VkPresentModeKHR, 16> presentModes;
//     LOG_ASSERT(presentModeCount <= 16, "presentModeCount: %u", presentModeCount);
//
//     vkGetPhysicalDeviceSurfacePresentModesKHR(m_context.physicalDevice, m_context.surface,
//                                               &presentModeCount, presentModes.data());
//
//     VkSurfaceFormatKHR format = ChooseSurfaceFormat(surfaceFormats.data(), surfaceFormatCount);
//     VkPresentModeKHR mode = ChoosePresentMode(presentModes.data(), presentModeCount);
//     VkExtent2D extent2d = ChooseSwapchainExtent(surfaceCapabilities);
//
//     m_context.surfaceFormat = format;
//     m_context.extent = extent2d;
//
//     uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
//     if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
//     {
//         imageCount = surfaceCapabilities.maxImageCount;
//     }
//
//     VkSwapchainCreateInfoKHR swapchainCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
//         .pNext = nullptr,
//         .flags = 0,
//         .surface = m_context.surface,
//         .minImageCount = imageCount,
//         .imageFormat = format.format,
//         .imageColorSpace = format.colorSpace,
//         .imageExtent = extent2d,
//         .imageArrayLayers = 1,
//         .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
//         .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         .preTransform = surfaceCapabilities.currentTransform,
//         .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
//         .presentMode = mode,
//         .clipped = VK_TRUE,
//         .oldSwapchain = oldSwapchain,
//     };
//
//     if (vkCreateSwapchainKHR(m_context.device, &swapchainCreateInfo, nullptr,
//                              &m_context.swapchain) != VK_SUCCESS)
//     {
//         LOG_ERROR("VULKAN", "Failed to create swapchain!");
//         throw std::runtime_error("[VULKAN] Failed to create swapchain!");
//     }
//
//     vkGetSwapchainImagesKHR(m_context.device, m_context.swapchain, &m_context.swapchainImageCount,
//                             nullptr);
//
//     if (m_context.swapchainImageCount == 0)
//     {
//         LOG_ERROR("VULKAN", "Swapchain image count is 0!");
//         throw std::runtime_error("[VULKAN] Swapchain image count is 0!");
//     }
//
//     m_context.pSwapchainImages = SCAST(
//         VkImage *, V_ALIGNED_ALLOC(m_allocator, sizeof(VkImage) * m_context.swapchainImageCount, alignof(VkImage)));
//     vkGetSwapchainImagesKHR(m_context.device, m_context.swapchain, &m_context.swapchainImageCount,
//                             m_context.pSwapchainImages);
// }
//
// void VulkanBackend::CreateSwapchainImageViews()
// {
//     m_context.pSwapchainImageViews =
//         SCAST(VkImageView *,
//               V_ALIGNED_ALLOC(m_allocator, sizeof(VkImageView) * m_context.swapchainImageCount, alignof(VkImageView)));
//     for (size_t idx = 0; idx < m_context.swapchainImageCount; ++idx)
//     {
//         m_context.pSwapchainImageViews[idx] =
//             CreateImageView(m_context.pSwapchainImages[idx], m_context.surfaceFormat.format,
//                             VK_IMAGE_ASPECT_COLOR_BIT);
//     }
// }
//
// static std::vector<char> ReadFile(const char *fileName)
// {
//     std::ifstream file(fileName, std::ios::ate | std::ios::binary);
//
//     if (!file.is_open())
//     {
//         LOG_ERROR("RESOURCE", "Failed to open file %s", fileName);
//     }
//
//     std::vector<char> buffer(file.tellg());
//
//     file.seekg(0, std::ios::beg);
//     file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
//
//     file.close();
//
//     return buffer;
// }
//
// void VulkanBackend::CreateGraphicPipeline()
// {
//     auto shader = ReadFile("D:\\Dev\\nvim\\void-engine\\void-"
//                            "engine\\resource\\shader\\spirv\\triangle.spv");
//     VkShaderModule module = CreateShaderModule(shader);
//
//     VkPipelineShaderStageCreateInfo vertStage{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .stage = VK_SHADER_STAGE_VERTEX_BIT,
//         .module = module,
//         .pName = "vertMain",
//         .pSpecializationInfo = nullptr,
//     };
//     VkPipelineShaderStageCreateInfo fragStage{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
//         .module = module,
//         .pName = "fragMain",
//         .pSpecializationInfo = nullptr,
//     };
//
//     VkPipelineShaderStageCreateInfo shaderStages[]{vertStage, fragStage};
//
//     auto bindingDescription = Vertex::GetBindingDescription();
//     auto attributeDescriptions = Vertex::GetAttributeDescriptions();
//
//     VkPipelineVertexInputStateCreateInfo vertexInputState{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
//         .pNext = nullptr,
//         .vertexBindingDescriptionCount = 1,
//         .pVertexBindingDescriptions = &bindingDescription,
//         .vertexAttributeDescriptionCount = 2,
//         .pVertexAttributeDescriptions = attributeDescriptions,
//     };
//
//     VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
//     };
//
//     VkViewport viewport{
//         .x = 0.0f,
//         .y = 0.0f,
//         .width = static_cast<float>(m_context.extent.width),
//         .height = static_cast<float>(m_context.extent.height),
//         .minDepth = 0.0f,
//         .maxDepth = 1.0f,
//
//     };
//
//     VkRect2D scissor{
//         .offset = VkOffset2D{0, 0},
//         .extent = m_context.extent,
//     };
//
//     std::vector<VkDynamicState> dynamicStates = {
//         VK_DYNAMIC_STATE_VIEWPORT,
//         VK_DYNAMIC_STATE_SCISSOR,
//     };
//
//     VkPipelineDynamicStateCreateInfo dynamicState{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
//         .pDynamicStates = dynamicStates.data(),
//     };
//
//     VkPipelineViewportStateCreateInfo viewportState{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .viewportCount = 1,
//         .pViewports = &viewport,
//         .scissorCount = 1,
//         .pScissors = &scissor,
//     };
//
//     VkPipelineRasterizationStateCreateInfo rasterizationState{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .depthClampEnable = VK_FALSE,
//         .rasterizerDiscardEnable = VK_FALSE,
//         .polygonMode = VK_POLYGON_MODE_FILL,
//         .cullMode = VK_CULL_MODE_BACK_BIT,
//         .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
//         .depthBiasEnable = VK_FALSE,
//         .lineWidth = 1.0f,
//     };
//
//     VkPipelineMultisampleStateCreateInfo multisampleState{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
//         .sampleShadingEnable = VK_FALSE,
//
//     };
//
//     VkPipelineColorBlendAttachmentState colorBlendAttachment{
//         .blendEnable = VK_FALSE,
//         .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
//                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
//     };
//
//     VkPipelineColorBlendStateCreateInfo colorBlendState{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .logicOpEnable = VK_FALSE,
//         .logicOp = VK_LOGIC_OP_COPY,
//         .attachmentCount = 1,
//         .pAttachments = &colorBlendAttachment,
//     };
//
//     VkPipelineLayoutCreateInfo pipelineLayout{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .setLayoutCount = 1,
//         .pSetLayouts = &m_descriptorSetLayout,
//         .pushConstantRangeCount = 0,
//     };
//
//     vkCreatePipelineLayout(m_context.device, &pipelineLayout, nullptr, &m_pipelineLayout);
//
//     VkPipelineDepthStencilStateCreateInfo depthStencil{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
//         .pNext = nullptr,
//         .depthTestEnable = VK_TRUE,
//         .depthWriteEnable = VK_TRUE,
//         .depthCompareOp = VK_COMPARE_OP_LESS,
//         .depthBoundsTestEnable = VK_FALSE,
//         .stencilTestEnable = VK_FALSE,
//     };
//
//     VkPipelineRenderingCreateInfo pipelineRendering{
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
//         .pNext = nullptr,
//         .colorAttachmentCount = 1,
//         .pColorAttachmentFormats = &m_context.surfaceFormat.format,
//         .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
//     };
//
//     VkGraphicsPipelineCreateInfo graphicPipeline{
//         .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
//         .pNext = &pipelineRendering,
//         .flags = 0,
//         .stageCount = 2,
//         .pStages = shaderStages,
//         .pVertexInputState = &vertexInputState,
//         .pInputAssemblyState = &inputAssemblyState,
//         .pViewportState = &viewportState,
//         .pRasterizationState = &rasterizationState,
//         .pMultisampleState = &multisampleState,
//         .pDepthStencilState = &depthStencil,
//         .pColorBlendState = &colorBlendState,
//         .pDynamicState = &dynamicState,
//         .layout = m_pipelineLayout,
//         .renderPass = nullptr,
//     };
//
//     vkCreateGraphicsPipelines(m_context.device, VK_NULL_HANDLE, 1, &graphicPipeline, nullptr,
//                               &m_pipeline);
//
//     vkDestroyShaderModule(m_context.device, module, nullptr);
// }
//
// VkShaderModule VulkanBackend::CreateShaderModule(const std::vector<char> &binaryCode)
// {
//     VkShaderModuleCreateInfo createInfo{
//         .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .codeSize = binaryCode.size() * sizeof(char),
//         .pCode = reinterpret_cast<const uint32_t *>(binaryCode.data()),
//     };
//
//     VkShaderModule shaderModule = VK_NULL_HANDLE;
//
//     vkCreateShaderModule(m_context.device, &createInfo, nullptr, &shaderModule);
//
//     return shaderModule;
// }
//
// void VulkanBackend::CreateCommandPool()
// {
//     VkCommandPoolCreateInfo graphicCmdPool{
//         .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
//         .queueFamilyIndex = m_context.graphicQueueFamilyIndex,
//     };
//
//     vkCreateCommandPool(m_context.device, &graphicCmdPool, nullptr, &m_context.graphicCommandPool);
// }
//
// void VulkanBackend::CreateVertexBuffer()
// {
//     VkDeviceSize size = sizeof(vertices);
//
//     VkBufferCreateInfo stageBufferCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//         .pNext = nullptr,
//         .size = size,
//         .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         .queueFamilyIndexCount = 0,
//         .pQueueFamilyIndices = nullptr,
//     };
//
//     VulkanAllocationInfo allocInfo;
//
//     VulkanBuffer stageBuffer = CreateBuffer(
//         allocInfo, &stageBufferCreateInfo, VulkanMemoryUsage::VULKAN_MEMORY_USAGE_CPU_ONLY,
//         VulkanMemoryFlagBit::VULKAN_MEMORY_FLAG_CPU_SEQUENTIAL_WRITE_BIT);
//
//     void *stageData = MapMemory(stageBuffer);
//
//     std::memcpy(stageData, vertices, size);
//     UnmapMemory(stageBuffer);
//
//     VkBufferCreateInfo vertexBufferCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//         .pNext = nullptr,
//         .size = size,
//         .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         .queueFamilyIndexCount = 0,
//         .pQueueFamilyIndices = nullptr,
//     };
//
//     VulkanAllocationInfo vertexAllocInfo;
//     m_vertexBuffer =
//         CreateBuffer(vertexAllocInfo, &vertexBufferCreateInfo, VULKAN_MEMORY_USAGE_GPU_ONLY, 0);
//
//     CopyBuffer(m_vertexBuffer.buffer, stageBuffer.buffer, size);
//
//     DestroyBuffer(stageBuffer);
// }
//
// void VulkanBackend::CreateIndexBuffer()
// {
//     VkDeviceSize size = sizeof(indices);
//
//     VkBufferCreateInfo stageBufferCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//         .pNext = nullptr,
//         .size = size,
//         .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         .queueFamilyIndexCount = 0,
//         .pQueueFamilyIndices = nullptr,
//     };
//
//     VulkanAllocationInfo allocInfo;
//
//     VulkanBuffer stageBuffer = CreateBuffer(
//         allocInfo, &stageBufferCreateInfo, VulkanMemoryUsage::VULKAN_MEMORY_USAGE_CPU_ONLY,
//         VulkanMemoryFlagBit::VULKAN_MEMORY_FLAG_CPU_SEQUENTIAL_WRITE_BIT);
//
//     void *stageData = MapMemory(stageBuffer);
//
//     std::memcpy(stageData, indices, size);
//     UnmapMemory(stageBuffer);
//
//     VkBufferCreateInfo indexBufferCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//         .pNext = nullptr,
//         .size = size,
//         .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         .queueFamilyIndexCount = 0,
//         .pQueueFamilyIndices = nullptr,
//     };
//
//     VulkanAllocationInfo indexAllocInfo;
//
//     m_indexBuffer =
//         CreateBuffer(indexAllocInfo, &indexBufferCreateInfo, VULKAN_MEMORY_USAGE_GPU_ONLY, 0);
//
//     CopyBuffer(m_indexBuffer.buffer, stageBuffer.buffer, size);
//
//     DestroyBuffer(stageBuffer);
// }
//
// void VulkanBackend::CreateDescriptorPool()
// {
//     std::array<VkDescriptorPoolSize, 2> poolSizes{{
//         {
//             .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//             .descriptorCount = s_kMaxFrameInFlight,
//         },
//         {
//             .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//             .descriptorCount = s_kMaxFrameInFlight,
//         },
//     }};
//
//     VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
//         .maxSets = s_kMaxFrameInFlight,
//         .poolSizeCount = poolSizes.size(),
//         .pPoolSizes = poolSizes.data(),
//     };
//
//     vkCreateDescriptorPool(m_context.device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool);
// }
//
// void VulkanBackend::CreateDescriptorSetLayout()
// {
//     std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings{{
//         {
//             .binding = 0,
//             .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//             .descriptorCount = 1,
//             .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
//         },
//         {
//             .binding = 1,
//             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//             .descriptorCount = 1,
//             .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
//         },
//     }};
//
//     VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
//         .pNext = nullptr,
//         .bindingCount = descriptorSetLayoutBindings.size(),
//         .pBindings = descriptorSetLayoutBindings.data(),
//     };
//
//     vkCreateDescriptorSetLayout(m_context.device, &descriptorSetLayoutCreateInfo, nullptr,
//                                 &m_descriptorSetLayout);
// }
//
// void VulkanBackend::CreateDescriptorSets()
// {
//     std::array<VkDescriptorSetLayout, s_kMaxFrameInFlight> setLayout{m_descriptorSetLayout,
//                                                                      m_descriptorSetLayout};
//
//     VkDescriptorSetAllocateInfo setAllocInfo{
//         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
//         .pNext = nullptr,
//         .descriptorPool = m_descriptorPool,
//         .descriptorSetCount = setLayout.size(),
//         .pSetLayouts = setLayout.data(),
//     };
//
//     m_descriptorSets.resize(s_kMaxFrameInFlight);
//     vkAllocateDescriptorSets(m_context.device, &setAllocInfo, m_descriptorSets.data());
//
//     for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
//     {
//         VkDescriptorBufferInfo bufferInfo{
//             .buffer = m_uniformBuffers[idx].buffer,
//             .offset = 0,
//             .range = sizeof(UniformBufferObject),
//         };
//
//         VkDescriptorImageInfo imageInfo{
//             .sampler = m_sampler,
//             .imageView = m_texture.view,
//             .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//         };
//
//         std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{
//             {{
//                  .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//                  .pNext = nullptr,
//                  .dstSet = m_descriptorSets[idx],
//                  .dstBinding = 0,
//                  .dstArrayElement = 0,
//                  .descriptorCount = 1,
//                  .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//                  .pBufferInfo = &bufferInfo,
//              },
//              {
//                  .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//                  .pNext = nullptr,
//                  .dstSet = m_descriptorSets[idx],
//                  .dstBinding = 1,
//                  .dstArrayElement = 0,
//                  .descriptorCount = 1,
//                  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//                  .pImageInfo = &imageInfo,
//              }}};
//
//         vkUpdateDescriptorSets(m_context.device, writeDescriptorSets.size(),
//                                writeDescriptorSets.data(), 0, nullptr);
//     }
// }
//
// void VulkanBackend::CreateUniformBuffer()
// {
//     m_uniformBuffers.resize(s_kMaxFrameInFlight);
//     m_mappedUniformBuffers.resize(s_kMaxFrameInFlight);
//
//     VkDeviceSize size = sizeof(UniformBufferObject);
//     for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
//     {
//         VkBufferCreateInfo bufferCreateInfo{
//             .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//             .pNext = nullptr,
//             .size = size,
//             .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//             .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//             .queueFamilyIndexCount = 0,
//             .pQueueFamilyIndices = nullptr,
//         };
//
//         VulkanAllocationInfo allocInfo;
//         m_uniformBuffers[idx] = CreateBuffer(
//             allocInfo, &bufferCreateInfo, VULKAN_MEMORY_USAGE_CPU_ONLY,
//             VULKAN_MEMORY_FLAG_CPU_SEQUENTIAL_WRITE_BIT | VULKAN_MEMORY_FLAG_PERSISTANT_MAPPED_BIT);
//         m_mappedUniformBuffers[idx] = allocInfo.mappedData;
//     }
// }
//
// void VulkanBackend::UpdateUniformBuffer(uint32_t frameIndex)
// {
//     static float rad = Rad(90.0f);
//     rad += Application::get_app().get_delta_time();
//
//     UniformBufferObject ubo;
//     ubo.model = Rotate(Mat4(1.0f), rad, Vec3(0.0f, 0.0f, 1.0f));
//
//     ubo.view = LookAt(Vec3(2.0f, 2.0f, 2.0f), Vec3(0.0f), Vec3(0.0f, 0.0f, 1.0f));
//     ubo.proj = Perspective(Rad(45.0f),
//                            static_cast<float>(m_context.extent.width) /
//                                static_cast<float>(m_context.extent.height),
//                            0.1f, 10.0f);
//
//     // Vulkan has inverted y axis
//     ubo.proj[1][1] *= -1;
//
//     std::memcpy(m_mappedUniformBuffers[frameIndex], &ubo, sizeof(UniformBufferObject));
// }
//
// void VulkanBackend::CreateTextureImage()
// {
//     int width = 0;
//     int height = 0;
//     int channels = 0;
//
//     stbi_uc *pixels = stbi_load("D:\\Dev\\nvim\\void-engine\\void-"
//                                 "engine\\resource\\textures\\texture.jpg",
//                                 &width, &height, &channels, STBI_rgb_alpha);
//
//     VkDeviceSize imageSize = width * height * 4;
//
//     if (!pixels)
//     {
//         LOG_ERROR("VULKAN", "Failed to load texture");
//     }
//
//     VkBufferCreateInfo stageBufferCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//         .pNext = nullptr,
//         .size = imageSize,
//         .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//         .queueFamilyIndexCount = 0,
//         .pQueueFamilyIndices = nullptr,
//     };
//
//     VulkanAllocationInfo allocationInfo;
//     VulkanBuffer stageBuffer = CreateBuffer(
//         allocationInfo, &stageBufferCreateInfo, VulkanMemoryUsage::VULKAN_MEMORY_USAGE_CPU_ONLY,
//         VulkanMemoryFlagBit::VULKAN_MEMORY_FLAG_CPU_SEQUENTIAL_WRITE_BIT);
//
//     void *mappedData = MapMemory(stageBuffer);
//     std::memcpy(mappedData, pixels, imageSize);
//     UnmapMemory(stageBuffer);
//
//     stbi_image_free(pixels);
//
//     VkImageCreateInfo imageCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//         .pNext = nullptr,
//         .imageType = VK_IMAGE_TYPE_2D,
//         .format = VK_FORMAT_R8G8B8A8_SRGB,
//         .extent = {SCAST(uint32_t, width), SCAST(uint32_t, height), 1},
//         .mipLevels = 1,
//         .arrayLayers = 1,
//         .samples = VK_SAMPLE_COUNT_1_BIT,
//         .tiling = VK_IMAGE_TILING_OPTIMAL,
//         .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//     };
//
//     m_texture.image = CreateImage(&imageCreateInfo, VULKAN_MEMORY_USAGE_GPU_ONLY, 0);
//
//     m_texture.view =
//         CreateImageView(m_texture.image.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
//     VkCommandBufferAllocateInfo commandAllocInfo{
//         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
//         .pNext = nullptr,
//         .commandPool = m_context.graphicCommandPool,
//         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
//         .commandBufferCount = 1,
//     };
//
//     VkCommandBuffer cmdBuffer;
//     vkAllocateCommandBuffers(m_context.device, &commandAllocInfo, &cmdBuffer);
//
//     VkCommandBufferBeginInfo beginInfo{
//         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
//         .pNext = nullptr,
//         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
//         .pInheritanceInfo = nullptr,
//     };
//
//     vkBeginCommandBuffer(cmdBuffer, &beginInfo);
//
//     TransitionImageLayout(cmdBuffer, m_texture.image.image, VK_IMAGE_LAYOUT_UNDEFINED,
//                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
//
//     CopyBufferToImage(cmdBuffer, stageBuffer.buffer, m_texture.image.image, width, height);
//
//     TransitionImageLayout(cmdBuffer, m_texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//
//     vkEndCommandBuffer(cmdBuffer);
//     VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
//         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
//         .pNext = nullptr,
//         .commandBuffer = cmdBuffer,
//     };
//
//     VkSubmitInfo2 submitInfo{
//         .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
//         .pNext = nullptr,
//         .flags = 0,
//         .commandBufferInfoCount = 1,
//         .pCommandBufferInfos = &cmdBufferSubmitInfo,
//     };
//
//     vkQueueSubmit2(m_context.graphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
//
//     vkQueueWaitIdle(m_context.graphicQueue);
//
//     DestroyBuffer(stageBuffer);
// }
//
// void VulkanBackend::CreateTextureImageView() {}
//
// void VulkanBackend::CreateTextureSampler()
// {
//     VkPhysicalDeviceProperties2 physicalDeviceProperties{
//         .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
//         .pNext = nullptr,
//     };
//
//     vkGetPhysicalDeviceProperties2(m_context.physicalDevice, &physicalDeviceProperties);
//
//     VkSamplerCreateInfo samplerCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
//         .pNext = nullptr,
//         .magFilter = VK_FILTER_LINEAR,
//         .minFilter = VK_FILTER_LINEAR,
//         .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
//         .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
//         .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
//         .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
//         .mipLodBias = 0.0f,
//         .anisotropyEnable = VK_TRUE,
//         .maxAnisotropy = physicalDeviceProperties.properties.limits.maxSamplerAnisotropy,
//         .compareEnable = VK_FALSE,
//         .compareOp = VK_COMPARE_OP_ALWAYS,
//         .minLod = 0.0f,
//         .maxLod = 0.0f,
//         .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
//         .unnormalizedCoordinates = VK_FALSE,
//     };
//
//     vkCreateSampler(m_context.device, &samplerCreateInfo, nullptr, &m_sampler);
// }
//
// void VulkanBackend::CreateDepthTexture()
// {
//     VkImageCreateInfo imageCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//         .pNext = nullptr,
//         .imageType = VK_IMAGE_TYPE_2D,
//         .format = VK_FORMAT_D32_SFLOAT,
//         .extent = {m_context.extent.width, m_context.extent.height, 1},
//         .mipLevels = 1,
//         .arrayLayers = 1,
//         .samples = VK_SAMPLE_COUNT_1_BIT,
//         .tiling = VK_IMAGE_TILING_OPTIMAL,
//         .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//     };
//
//     m_depthTex.image = CreateImage(&imageCreateInfo, VULKAN_MEMORY_USAGE_GPU_ONLY, 0);
//
//     m_depthTex.view =
//         CreateImageView(m_depthTex.image.image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
// }
//
// void VulkanBackend::CleanUpDepthTexture()
// {
//
//     vkDestroyImageView(m_context.device, m_depthTex.view, nullptr);
//     DestroyImage(m_depthTex.image);
// }
//
// VkImageView VulkanBackend::CreateImageView(const VkImage &image, VkFormat format,
//                                            VkImageAspectFlags imageFlags)
// {
//     VkImageViewCreateInfo imageViewCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
//         .pNext = nullptr,
//         .image = image,
//         .viewType = VK_IMAGE_VIEW_TYPE_2D,
//         .format = format,
//         .subresourceRange = {.aspectMask = imageFlags,
//                              .baseMipLevel = 0,
//                              .levelCount = 1,
//                              .baseArrayLayer = 0,
//                              .layerCount = 1},
//     };
//
//     VkImageView view;
//     vkCreateImageView(m_context.device, &imageViewCreateInfo, nullptr, &view);
//
//     return view;
// }
//
// // VulkanBackend::Texture VulkanBackend::CreateImage(uint32_t texWidth, uint32_t texHeight,
// //                                                   VkFormat format, VkImageTiling tilt,
// //                                                   VkImageUsageFlags usage,
// //                                                   VkMemoryPropertyFlags properties)
// // {
// //     VkImageCreateInfo imageCreateInfo{
// //         .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
// //         .pNext = nullptr,
// //         .imageType = VK_IMAGE_TYPE_2D,
// //         .format = format,
// //         .extent = {texWidth, texHeight, 1},
// //         .mipLevels = 1,
// //         .arrayLayers = 1,
// //         .samples = VK_SAMPLE_COUNT_1_BIT,
// //         .tiling = tilt,
// //         .usage = usage,
// //         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
// //     };
// //
// //     VkImage image;
// //
// //     vkCreateImage(m_context.device, &imageCreateInfo, nullptr, &image);
// //
// //     VkMemoryRequirements memReqs;
// //
// //     vkGetImageMemoryRequirements(m_context.device, image, &memReqs);
// //     VkMemoryAllocateInfo allocInfo{
// //         .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
// //         .pNext = nullptr,
// //         .allocationSize = memReqs.size,
// //         .memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, properties),
// //     };
// //
// //     VkDeviceMemory memory;
// //
// //     vkAllocateMemory(m_context.device, &allocInfo, nullptr, &memory);
// //
// //     vkBindImageMemory(m_context.device, image, memory, 0);
// //
// //     return Texture{.image = image, .memory = memory};
// // }
//
// void VulkanBackend::CopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer src, VkImage image,
//                                       uint32_t width, uint32_t height)
// {
//     VkBufferImageCopy bufferImageCopy{
//         .bufferOffset = 0,
//         .bufferRowLength = 0,
//         .bufferImageHeight = 0,
//         .imageSubresource =
//             {
//                 .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//                 .mipLevel = 0,
//                 .baseArrayLayer = 0,
//                 .layerCount = 1,
//             },
//         .imageOffset = {0, 0, 0},
//         .imageExtent = {width, height, 1},
//     };
//
//     vkCmdCopyBufferToImage(cmdBuffer, src, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
//                            &bufferImageCopy);
// }
//
// void VulkanBackend::CopyBuffer(VkBuffer dst, VkBuffer src, VkDeviceSize size)
// {
//
//     VkCommandBufferAllocateInfo allocInfo{
//         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
//         .pNext = nullptr,
//         .commandPool = m_context.graphicCommandPool,
//         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
//         .commandBufferCount = 1,
//     };
//
//     VkCommandBuffer cmdBuffer;
//     vkAllocateCommandBuffers(m_context.device, &allocInfo, &cmdBuffer);
//
//     VkCommandBufferBeginInfo beginInfo{
//         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
//         .pNext = nullptr,
//         .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
//         .pInheritanceInfo = nullptr,
//     };
//
//     VkBufferCopy region{
//         .srcOffset = 0,
//         .dstOffset = 0,
//         .size = size,
//     };
//
//     vkBeginCommandBuffer(cmdBuffer, &beginInfo);
//
//     vkCmdCopyBuffer(cmdBuffer, src, dst, 1, &region);
//
//     vkEndCommandBuffer(cmdBuffer);
//
//     VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
//         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
//         .pNext = nullptr,
//         .commandBuffer = cmdBuffer,
//     };
//
//     VkSubmitInfo2 submitInfo{
//         .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
//         .pNext = nullptr,
//         .flags = 0,
//         .commandBufferInfoCount = 1,
//         .pCommandBufferInfos = &cmdBufferSubmitInfo,
//     };
//
//     vkQueueSubmit2(m_context.graphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
//     vkQueueWaitIdle(m_context.graphicQueue);
// }
//
// void VulkanBackend::CreateCommandBuffers()
// {
//     VkCommandBufferAllocateInfo graphicCmdBufferAllocInfo{
//         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
//         .pNext = nullptr,
//         .commandPool = m_context.graphicCommandPool,
//         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
//         .commandBufferCount = s_kMaxFrameInFlight,
//     };
//
//     vkAllocateCommandBuffers(m_context.device, &graphicCmdBufferAllocInfo, m_graphicCmdBuffers);
// }
//
// void VulkanBackend::TransitionSwapchainImageLayout(uint32_t imageIndex, VkImageLayout oldLayout,
//                                                    VkImageLayout newLayout,
//                                                    VkAccessFlags2 srcAccessMask,
//                                                    VkAccessFlags2 dstAccessMask,
//                                                    VkPipelineStageFlags2 srcStageMask,
//                                                    VkPipelineStageFlags2 dstStageMask)
// {
//     VkImageMemoryBarrier2 barrier{
//         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//         .pNext = nullptr,
//         .srcStageMask = srcStageMask,
//         .srcAccessMask = srcAccessMask,
//         .dstStageMask = dstStageMask,
//         .dstAccessMask = dstAccessMask,
//         .oldLayout = oldLayout,
//         .newLayout = newLayout,
//         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//         .image = m_context.pSwapchainImages[imageIndex],
//     };
//
//     barrier.subresourceRange = {
//         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//         .baseMipLevel = 0,
//         .levelCount = 1,
//         .baseArrayLayer = 0,
//         .layerCount = 1,
//     };
//
//     VkDependencyInfo dependencyInfo{
//         .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
//         .pNext = nullptr,
//         .dependencyFlags = {},
//         .imageMemoryBarrierCount = 1,
//         .pImageMemoryBarriers = &barrier,
//     };
//
//     vkCmdPipelineBarrier2(m_graphicCmdBuffers[m_frameIndex], &dependencyInfo);
// }
//
// void VulkanBackend::TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
//                                           VkImageLayout oldLayout, VkImageLayout newLayout)
// {
//     VkImageMemoryBarrier2 imageMemBarrier{
//         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//         .pNext = nullptr,
//         // .srcStageMask = ,
//         // .srcAccessMask = ,
//         // .dstStageMask = ,
//         // .dstAccessMask = ,
//         .oldLayout = oldLayout,
//         .newLayout = newLayout,
//         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
//         .image = image,
//         .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//                              .levelCount = 1,
//                              .layerCount = 1},
//     };
//
//     if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
//     {
//         imageMemBarrier.srcAccessMask = {};
//         imageMemBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
//
//         imageMemBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
//         imageMemBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
//     }
//     else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
//              newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
//     {
//         imageMemBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
//         imageMemBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
//
//         imageMemBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
//         imageMemBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
//     }
//     else
//     {
//         LOG_ERROR("VULKAN", "Error!");
//         throw std::runtime_error("VULKAN Error");
//     }
//
//     VkDependencyInfo dependencyInfo{
//         .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
//         .pNext = nullptr,
//         .imageMemoryBarrierCount = 1,
//         .pImageMemoryBarriers = &imageMemBarrier,
//     };
//
//     vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
// }
//
// void VulkanBackend::TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
//                                           VkPipelineStageFlags2 srcStage,
//                                           VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess,
//                                           VkAccessFlags2 dstAccess, VkImageLayout oldLayout,
//                                           VkImageLayout newLayout, uint32_t srcQueueFamilyIndex,
//                                           uint32_t dstQueueFamilyIndex,
//                                           VkImageAspectFlags imageAspect)
// {
//     VkImageMemoryBarrier2 imageMemBarrier{
//         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//         .pNext = nullptr,
//         .srcStageMask = srcStage,
//         .srcAccessMask = srcAccess,
//         .dstStageMask = dstStage,
//         .dstAccessMask = dstAccess,
//         .oldLayout = oldLayout,
//         .newLayout = newLayout,
//         .srcQueueFamilyIndex = srcQueueFamilyIndex,
//         .dstQueueFamilyIndex = dstQueueFamilyIndex,
//         .image = image,
//         .subresourceRange = {.aspectMask = imageAspect, .levelCount = 1, .layerCount = 1},
//     };
//
//     VkDependencyInfo dependencyInfo{
//         .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
//         .pNext = nullptr,
//         .imageMemoryBarrierCount = 1,
//         .pImageMemoryBarriers = &imageMemBarrier,
//     };
//
//     vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
// }
//
// // VulkanBackend::Buffer VulkanBackend::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags
// // usageFlag,
// //                                                   VkSharingMode sharingMode,
// //                                                   VkMemoryPropertyFlags propertyFlag,
// //                                                   uint32_t queueFamilyCount,
// //                                                   uint32_t *queueFamilyIndices)
// // {
// //     Buffer buffer;
// //
// //     VkBufferCreateInfo bufferCreateInfo{
// //         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
// //         .pNext = nullptr,
// //         .size = size,
// //         .usage = usageFlag,
// //         .sharingMode = sharingMode,
// //         .queueFamilyIndexCount = queueFamilyCount,
// //         .pQueueFamilyIndices = queueFamilyIndices,
// //     };
// //
// //     vkCreateBuffer(m_context.device, &bufferCreateInfo, nullptr, &buffer.buffer);
// //
// //     VkMemoryRequirements memoryReqs;
// //
// //     vkGetBufferMemoryRequirements(m_context.device, buffer.buffer, &memoryReqs);
// //
// //     VkMemoryAllocateInfo memoryAllocInfo{
// //         .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
// //         .pNext = nullptr,
// //         .allocationSize = memoryReqs.size,
// //         .memoryTypeIndex = FindMemoryType(memoryReqs.memoryTypeBits, propertyFlag),
// //     };
// //
// //     vkAllocateMemory(m_context.device, &memoryAllocInfo, nullptr, &buffer.memory);
// //     vkBindBufferMemory(m_context.device, buffer.buffer, buffer.memory, 0);
// //
// //     return buffer;
// // }
//
// void VulkanBackend::RecordCommandBuffer(uint32_t imageIndex)
// {
//     VkCommandBufferBeginInfo beginInfo{
//         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .pInheritanceInfo = 0,
//     };
//
//     vkBeginCommandBuffer(m_graphicCmdBuffers[m_frameIndex], &beginInfo);
//
//     TransitionSwapchainImageLayout(
//         imageIndex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
//         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
//         VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
//
//     TransitionImageLayout(
//         m_graphicCmdBuffers[m_frameIndex], m_depthTex.image.image,
//         VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
//         VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
//         VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
//         VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
//         VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
//         VK_IMAGE_ASPECT_DEPTH_BIT);
//
//     VkClearValue clearColor{
//         .color = VkClearColorValue{{0.0f, 0.0f, 0.0f, 0.0f}},
//     };
//
//     VkClearValue clearDepth = {.depthStencil = {.depth = 1.0f, .stencil = 0}};
//
//     VkRenderingAttachmentInfo depthAttachmentInfo{
//         .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
//         .pNext = nullptr,
//         .imageView = m_depthTex.view,
//         .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
//         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
//         .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
//         .clearValue = clearDepth};
//
//     VkRenderingAttachmentInfo renderingAttachmentInfo{
//         .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
//         .pNext = nullptr,
//         .imageView = m_context.pSwapchainImageViews[imageIndex],
//         .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
//         .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
//         .clearValue = clearColor};
//
//     VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
//                                   .pNext = nullptr,
//                                   .renderArea = {.offset = {0, 0}, .extent = m_context.extent},
//                                   .layerCount = 1,
//                                   .colorAttachmentCount = 1,
//                                   .pColorAttachments = &renderingAttachmentInfo,
//                                   .pDepthAttachment = &depthAttachmentInfo};
//
//     vkCmdBeginRendering(m_graphicCmdBuffers[m_frameIndex], &renderingInfo);
//
//     vkCmdBindPipeline(m_graphicCmdBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
//                       m_pipeline);
//     VkViewport viewport{
//         .x = 0.0f,
//         .y = 0.0f,
//         .width = static_cast<float>(m_context.extent.width),
//         .height = static_cast<float>(m_context.extent.height),
//         .minDepth = 0.0f,
//         .maxDepth = 1.0f,
//
//     };
//
//     VkRect2D scissor{
//         .offset = VkOffset2D{0, 0},
//         .extent = m_context.extent,
//     };
//
//     vkCmdSetViewport(m_graphicCmdBuffers[m_frameIndex], 0, 1, &viewport);
//     vkCmdSetScissor(m_graphicCmdBuffers[m_frameIndex], 0, 1, &scissor);
//
//     vkCmdBindPipeline(m_graphicCmdBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
//                       m_pipeline);
//     VkDeviceSize offset = 0;
//     vkCmdBindVertexBuffers(m_graphicCmdBuffers[m_frameIndex], 0, 1, &m_vertexBuffer.buffer,
//                            &offset);
//     vkCmdBindIndexBuffer(m_graphicCmdBuffers[m_frameIndex], m_indexBuffer.buffer, 0,
//                          VK_INDEX_TYPE_UINT16);
//
//     vkCmdBindDescriptorSets(m_graphicCmdBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
//                             m_pipelineLayout, 0, 1, &m_descriptorSets[m_frameIndex], 0, nullptr);
//
//     vkCmdDrawIndexed(m_graphicCmdBuffers[m_frameIndex], 12, 1, 0, 0, 0);
//
//     vkCmdEndRendering(m_graphicCmdBuffers[m_frameIndex]);
//
//     TransitionSwapchainImageLayout(
//         imageIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
//         VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {}, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
//         VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
//
//     vkEndCommandBuffer(m_graphicCmdBuffers[m_frameIndex]);
// }
//
// void VulkanBackend::CreateSyncObjects()
// {
//     VkSemaphoreCreateInfo presentComplete{
//         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//     };
//
//     VkSemaphoreCreateInfo renderFinished{
//         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//     };
//
//     VkFenceCreateInfo drawFence{
//         .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = VK_FENCE_CREATE_SIGNALED_BIT,
//     };
//     for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
//     {
//         vkCreateSemaphore(m_context.device, &presentComplete, nullptr,
//                           &m_frames[idx].presentCompleteSemaphore);
//         vkCreateSemaphore(m_context.device, &renderFinished, nullptr,
//                           &m_frames[idx].renderFinishedSemaphore);
//         vkCreateFence(m_context.device, &drawFence, nullptr, &m_frames[idx].drawFence);
//     }
// }
//
// void VulkanBackend::DrawFrame()
// {
//
//     uint32_t imageIndex = UINT32_MAX;
//
//     VkResult acquireResult = vkAcquireNextImageKHR(
//         m_context.device, m_context.swapchain, UINT64_MAX,
//         m_frames[m_frameIndex].presentCompleteSemaphore, VK_NULL_HANDLE, &imageIndex);
//
//     if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
//     {
//         RecreateSwapchain();
//         return;
//     }
//     else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
//     {
//         LOG_ASSERT(acquireResult == VK_TIMEOUT || acquireResult == VK_NOT_READY,
//                    "Failed to acquire next image to present");
//     }
//
//     if (vkWaitForFences(m_context.device, 1, &m_frames[m_frameIndex].drawFence, VK_TRUE,
//                         UINT64_MAX) != VK_SUCCESS)
//     {
//         LOG_ASSERT("VULKAN", "Failed to wait for draw fence");
//     }
//
//     RecordCommandBuffer(imageIndex);
//
//     VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo{
//         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
//         .pNext = nullptr,
//         .semaphore = m_frames[m_frameIndex].presentCompleteSemaphore,
//         .value = 0,
//         .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
//     };
//
//     VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo{
//         .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
//         .pNext = nullptr,
//         .semaphore = m_frames[m_frameIndex].renderFinishedSemaphore,
//         .value = 0,
//         .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
//     };
//
//     VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
//         .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
//         .pNext = nullptr,
//         .commandBuffer = m_graphicCmdBuffers[m_frameIndex],
//     };
//
//     UpdateUniformBuffer(m_frameIndex);
//
//     VkSubmitInfo2 submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
//                              .pNext = nullptr,
//                              .flags = 0,
//                              .waitSemaphoreInfoCount = 1,
//                              .pWaitSemaphoreInfos = &waitSemaphoreSubmitInfo,
//                              .commandBufferInfoCount = 1,
//                              .pCommandBufferInfos = &cmdBufferSubmitInfo,
//                              .signalSemaphoreInfoCount = 1,
//                              .pSignalSemaphoreInfos = &signalSemaphoreSubmitInfo};
//
//     vkResetFences(m_context.device, 1, &m_frames[m_frameIndex].drawFence);
//     vkQueueSubmit2(m_context.graphicQueue, 1, &submitInfo, m_frames[m_frameIndex].drawFence);
//
//     VkPresentInfoKHR presentInfo{
//         .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
//         .pNext = nullptr,
//         .waitSemaphoreCount = 1,
//         .pWaitSemaphores = &m_frames[m_frameIndex].renderFinishedSemaphore,
//         .swapchainCount = 1,
//         .pSwapchains = &m_context.swapchain,
//         .pImageIndices = &imageIndex,
//     };
//
//     VkResult presentResult = vkQueuePresentKHR(m_context.graphicQueue, &presentInfo);
//
//     if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
//         Application::get_app().is_app_resizing())
//     {
//         RecreateSwapchain();
//     }
//     else if (presentResult != VK_SUCCESS)
//     {
//         LOG_ASSERT(presentResult == VK_TIMEOUT || presentResult == VK_NOT_READY,
//                    "Failed to acquire next image to present");
//     }
//
//     vkQueueWaitIdle(m_context.graphicQueue);
// }
//
// void VulkanBackend::DrawTest()
// {
//     DrawFrame();
//     m_frameIndex = (m_frameIndex++ % s_kMaxFrameInFlight);
//
//     // vkQueueWaitIdle(m_context.graphicQueue);
// }
//
// void VulkanBackend::RecreateSwapchain()
// {
//     vkQueueWaitIdle(m_context.graphicQueue);
//
//     for (size_t idx = 0; idx < m_context.swapchainImageCount; ++idx)
//     {
//         vkDestroyImageView(m_context.device, m_context.pSwapchainImageViews[idx], nullptr);
//     }
//
//     VkSwapchainKHR oldSwapchain = m_context.swapchain;
//     CreateSwapchain(oldSwapchain);
//
//     vkDestroySwapchainKHR(m_context.device, oldSwapchain, nullptr);
//
//     CreateSwapchainImageViews();
//
//     CleanUpDepthTexture();
//     CreateDepthTexture();
// }
//
// void VulkanBackend::CreateInstance()
// {
//     VkBool32 profileSupported;
//
//     vpGetInstanceProfileSupport(nullptr, &profile, &profileSupported);
//
//     if (!profileSupported)
//     {
//         // TODO: Fallback path
//         m_minProfileSupported = VK_FALSE;
//
//         LOG_ERROR("VULKAN", "Vulkan profile not supported");
//         throw std::runtime_error("Vulkan profile not supported");
//     }
//
//     const std::vector<const char *> requiredExts = {
//         "VK_KHR_surface",
// #ifdef VOID_WIN32
//         "VK_KHR_win32_surface",
// #else
//
// #endif // VOID_WIN32
//
// #ifdef VOID_DEBUG
//         "VK_EXT_debug_utils",
// #endif // VOID_DEBUG
//     };
//
//     uint32_t extCount = 0;
//     vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
//     std::vector<VkExtensionProperties> extProperties(extCount);
//     vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extProperties.data());
//
//     auto extensionIter = std::ranges::find_if(requiredExts, [&extProperties](const char *ext)
//                                               { return !IsExtAvailable(extProperties, ext); });
//
//     if (extensionIter != requiredExts.end())
//     {
//         LOG_ERROR("VULKAN", "No support for %s extension", *extensionIter);
//
//         throw std::runtime_error("[VULKAN] No support for " + std::string(*extensionIter));
//     }
//
//     uint32_t layerCount = 0;
//     vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
//     std::vector<VkLayerProperties> layerProperties(layerCount);
//
//     vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());
//     const std::vector<const char *> requiredLayers = {
// #ifdef VOID_DEBUG
//         "VK_LAYER_KHRONOS_validation"
// #endif // VOID_DEBUG
//     };
//
//     auto layerIter = std::ranges::find_if(requiredLayers, [&layerProperties](const char *layer)
//                                           { return !IsLayerAvailable(layerProperties, layer); });
//
//     if (layerIter != requiredLayers.end())
//     {
//         LOG_ERROR("VULKAN", "No support for %s layer", *layerIter);
//
//         throw std::runtime_error("[VULKAN] No support for " + std::string(*layerIter));
//     }
//
//     VkInstanceCreateInfo instanceCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
//         .pNext = nullptr,
//         .flags = 0,
//         .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
//         .ppEnabledLayerNames = requiredLayers.data(),
//         .enabledExtensionCount = static_cast<uint32_t>(requiredExts.size()),
//         .ppEnabledExtensionNames = requiredExts.data(),
//     };
//
//     VpInstanceCreateInfo vpInstanceCreateInfo{
//         .pCreateInfo = &instanceCreateInfo,
//         .enabledFullProfileCount = 1,
//         .pEnabledFullProfiles = &profile,
//     };
//
//     VkResult result = vpCreateInstance(&vpInstanceCreateInfo, nullptr, &m_context.instance);
//
//     if (result != VK_SUCCESS)
//     {
//         LOG_ERROR("VULKAN", "Failed to create Vulkan instance!");
//
//         throw std::runtime_error("[VULKAN] Failed to create Vulkan instance!");
//     }
// }
//
// void VulkanBackend::CreatePhysicalDevice()
// {
//     uint32_t deviceCount = 0;
//     vkEnumeratePhysicalDevices(m_context.instance, &deviceCount, nullptr);
//
//     std::array<VkPhysicalDevice, 8> devices;
//     vkEnumeratePhysicalDevices(m_context.instance, &deviceCount, devices.data());
//
//     const std::array<const char *, 2> requiredDeviceExtensions = {
//         {"VK_KHR_swapchain", "VK_EXT_memory_budget"}};
//
//     for (size_t idx = 0; idx < deviceCount; ++idx)
//     {
//         VkBool32 profileSupported;
//
//         vpGetPhysicalDeviceProfileSupport(m_context.instance, devices[idx], &profile,
//                                           &profileSupported);
//
//         if (profileSupported)
//         {
//
//             uint32_t deviceExtensionCount = 0;
//             vkEnumerateDeviceExtensionProperties(devices[idx], nullptr, &deviceExtensionCount,
//                                                  nullptr);
//
//             if (deviceExtensionCount == 0)
//             {
//                 continue;
//             }
//             bool supportsRequiredExtensions = false;
//             std::vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
//             vkEnumerateDeviceExtensionProperties(devices[idx], nullptr, &deviceExtensionCount,
//                                                  deviceExtensions.data());
//
//             supportsRequiredExtensions = std::ranges::all_of(
//                 requiredDeviceExtensions,
//                 [&](const char *ext)
//                 {
//                     return std::ranges::any_of(deviceExtensions, [&](const VkExtensionProperties &p)
//                                                { return std::strcmp(ext, p.extensionName) == 0; });
//                 });
//
//             if (!supportsRequiredExtensions)
//             {
//                 continue;
//             }
//
//             VkSurfaceCapabilitiesKHR surfaceCapabilities{};
//             vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[idx], m_context.surface,
//                                                       &surfaceCapabilities);
//
//             uint32_t surfaceFormatCount = 0;
//             vkGetPhysicalDeviceSurfaceFormatsKHR(devices[idx], m_context.surface,
//                                                  &surfaceFormatCount, nullptr);
//
//             std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
//             vkGetPhysicalDeviceSurfaceFormatsKHR(devices[idx], m_context.surface,
//                                                  &surfaceFormatCount, surfaceFormats.data());
//
//             uint32_t presentModeCount = 0;
//             vkGetPhysicalDeviceSurfacePresentModesKHR(devices[idx], m_context.surface,
//                                                       &presentModeCount, nullptr);
//
//             std::vector<VkPresentModeKHR> presentModes(presentModeCount);
//             vkGetPhysicalDeviceSurfacePresentModesKHR(devices[idx], m_context.surface,
//                                                       &presentModeCount, presentModes.data());
//
//             if (presentModeCount == 0 || surfaceFormatCount == 0)
//             {
//                 continue;
//             }
//
//             VkPhysicalDeviceProperties2 physicalDeviceProperties{
//                 .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
//                 .pNext = nullptr,
//             };
//
//             vkGetPhysicalDeviceProperties2(devices[idx], &physicalDeviceProperties);
//
//             uint32_t queueFamilyPropCount;
//             vkGetPhysicalDeviceQueueFamilyProperties2(devices[idx], &queueFamilyPropCount, nullptr);
//
//             std::vector<VkQueueFamilyProperties2> queueFamilyProps(
//                 queueFamilyPropCount,
//                 {.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, .pNext = nullptr});
//
//             vkGetPhysicalDeviceQueueFamilyProperties2(devices[idx], &queueFamilyPropCount,
//                                                       queueFamilyProps.data());
//
//             bool isDiscreteDevice = physicalDeviceProperties.properties.deviceType ==
//                                     VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
//             bool supportVulkan1_3 =
//                 physicalDeviceProperties.properties.apiVersion >= VK_API_VERSION_1_3;
//
//             bool supportGraphics = std::ranges::any_of(
//                 queueFamilyProps, [](const VkQueueFamilyProperties2 &p)
//                 { return (p.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0; });
//
//             VkPhysicalDeviceVulkan11Features vulkan11features{
//                 .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
//                 .pNext = nullptr,
//             };
//
//             VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{
//                 .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
//                 .pNext = &vulkan11features};
//
//             VkPhysicalDeviceFeatures2 features2{.sType =
//                                                     VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
//                                                 .pNext = &extendedDynamicStateFeatures};
//
//             vkGetPhysicalDeviceFeatures2(devices[idx], &features2);
//
//             if (isDiscreteDevice && supportVulkan1_3 && supportGraphics &&
//                 vulkan11features.shaderDrawParameters &&
//                 extendedDynamicStateFeatures.extendedDynamicState)
//             {
//                 m_context.physicalDevice = devices[idx];
//                 return;
//             }
//         }
//     }
//
//     // TODO: Fallback path
//     m_minProfileSupported = VK_FALSE;
//
//     LOG_ERROR("VULKAN", "Can find suitable physical device");
//
//     throw std::runtime_error("Vulkan can not find suitable physical device");
// }
//
// } // namespace VoidEngine
