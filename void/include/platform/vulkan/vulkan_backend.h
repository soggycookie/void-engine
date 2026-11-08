// #pragma once
// #include "dynamic_array.h"
// #include "stable_vector.h"
// #include "math_utils.h"
// #include "pch.h"
// #include "utils.h"
//
// #ifdef _WIN32
// #include "win32_window.h"
// #define VK_USE_PLATFORM_WIN32_KHR
// #else
//
// #endif
//
// #include <vulkan/vulkan.h>
// #include <vulkan/vulkan_profiles.hpp>
//
// namespace VoidEngine
// {
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
// struct VulkanBuffer
// {
//     VkBuffer buffer;
//     VulkanAllocationHandle handle;
// };
//
// struct VulkanImage
// {
//     VkImage image;
//     VulkanAllocationHandle handle;
// };
//
// struct VulkanImageView
// {
//     VulkanImage image;
//     VkImageView view;
// };
//
// struct UniformBufferObject
// {
//     Mat4 model;
//     Mat4 view;
//     Mat4 proj;
// };
//
// constexpr VpProfileProperties profile = {VP_KHR_ROADMAP_2022_NAME, VP_KHR_ROADMAP_2022_SPEC_VERSION};
//
// struct VulkanRendererContext
// {
//     VkInstance instance;
//     VkPhysicalDevice physicalDevice;
//     VkDevice device;
//     VkSurfaceKHR surface;
//     VkSurfaceFormatKHR surfaceFormat;
//     VkSwapchainKHR swapchain;
//     VkExtent2D extent;
//     VkImage *pSwapchainImages;
//     VkImageView *pSwapchainImageViews;
//     VkCommandPool graphicCommandPool;
//     VkQueue graphicQueue;
//     uint32_t swapchainImageCount;
//     uint32_t graphicQueueFamilyIndex = ~0;
// };
//
// class VulkanBackend
// {
// private:
//     struct Vertex
//     {
//         Vec3 pos;
//         Vec2 texCoord;
//
//         static VkVertexInputBindingDescription GetBindingDescription()
//         {
//             return {
//                 .binding = 0,
//                 .stride = sizeof(Vertex),
//                 .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
//             };
//         }
//
//         static VkVertexInputAttributeDescription *GetAttributeDescriptions()
//         {
//             static VkVertexInputAttributeDescription descriptions[] = {
//                 {.location = 0,
//                  .binding = 0,
//                  .format = VK_FORMAT_R32G32B32_SFLOAT,
//                  .offset = offsetof(Vertex, pos)},
//
//                 {.location = 1,
//                  .binding = 0,
//                  .format = VK_FORMAT_R32G32_SFLOAT,
//                  .offset = offsetof(Vertex, texCoord)},
//             };
//
//             return descriptions;
//         }
//     };
//
//     struct FrameSyncObjs
//     {
//         VkSemaphore presentCompleteSemaphore;
//         VkSemaphore renderFinishedSemaphore;
//         VkFence drawFence;
//     };
//
//     static constexpr Vertex vertices[] = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
//                                           {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
//                                           {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},
//                                           {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},
//
//                                           {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}},
//                                           {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}},
//                                           {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}},
//                                           {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}}
//
//     };
//
//     static constexpr uint16_t indices[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};
//     static constexpr uint32_t s_kMaxFrameInFlight = 2;
//
// private:
//     Window *m_window;
//
//     VulkanAllocator *m_vulkanAllocator;
//     Vallocator *m_allocator;
//
//     VulkanImageView m_depthTex;
//     VulkanImageView m_texture;
//     VkSampler m_sampler;
//     VulkanRendererContext m_context;
//     VkPipelineLayout m_pipelineLayout;
//     VkPipeline m_pipeline;
//
//     VulkanBuffer m_vertexBuffer;
//     VulkanBuffer m_indexBuffer;
//
//     VkDescriptorSetLayout m_descriptorSetLayout;
//     VkDescriptorPool m_descriptorPool;
//
//     uint32_t m_frameIndex = 0;
//
//     FrameSyncObjs m_frames[s_kMaxFrameInFlight];
//     VkCommandBuffer m_graphicCmdBuffers[s_kMaxFrameInFlight];
//
//     std::vector<VulkanBuffer> m_uniformBuffers;
//     std::vector<void *> m_mappedUniformBuffers;
//     std::vector<VkDescriptorSet> m_descriptorSets;
//
//     VkBool32 m_minProfileSupported;
//
// #ifdef VOID_DEBUG
//     VkDebugUtilsMessengerEXT m_debugMessenger;
// #endif // VOID_DEBUG
// public:
//     void DrawTest();
//
//     void *GetContext()
//     {
//         return nullptr;
//     }
//
// private:
//     friend class Application;
//
//     DEFAULT_CTOR(VulkanBackend);
//
//     void Shutdown();
//     bool StartUp(Window *window, Vallocator *allocator);
//
//     void CreateInstance();
//     void CreatePhysicalDevice();
//
//     void CreateSurface();
//     void SetUpDebugMessenger();
//     void CreateLogicalDevice();
//     void CreateSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
//     void CreateSwapchainImageViews();
//     void CreateGraphicPipeline();
//     VkShaderModule CreateShaderModule(const std::vector<char> &binaryCode);
//     void RecordCommandBuffer(uint32_t imageIndex);
//     void CreateCommandPool();
//
//     void CreateVertexBuffer();
//     void CreateIndexBuffer();
//
//     void CreateDescriptorPool();
//     void CreateDescriptorSetLayout();
//     void CreateDescriptorSets();
//     void CreateUniformBuffer();
//     void UpdateUniformBuffer(uint32_t frameIndex);
//
//     void CreateCommandBuffers();
//     void CreateSyncObjects();
//     void RecreateSwapchain();
//
//     void CreateTextureImage();
//     void CreateTextureImageView();
//     void CreateTextureSampler();
//
//     void CreateDepthTexture();
//     void CleanUpDepthTexture();
//
//     VkImageView CreateImageView(const VkImage &image, VkFormat format, VkImageAspectFlags imageFlags);
//
//     void DrawFrame();
//
//     void TransitionSwapchainImageLayout(uint32_t imageIndex, VkImageLayout oldLayout,
//                                         VkImageLayout newLayout, VkAccessFlags2 srcAccessMask,
//                                         VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask,
//                                         VkPipelineStageFlags2 dstStageMask);
//
//     void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout,
//                                VkImageLayout newLayout);
//
//     void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkPipelineStageFlags2 srcStage,
//                                VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess,
//                                VkAccessFlags2 dstAccess, VkImageLayout oldLayout,
//                                VkImageLayout newLayout, uint32_t srcQueueFamilyIndex,
//                                uint32_t dstQueueFamilyIndex, VkImageAspectFlags imageAspect);
//
//     void CopyBuffer(VkBuffer dst, VkBuffer src, VkDeviceSize size);
//
//     void CopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer src, VkImage image, uint32_t width,
//                            uint32_t height);
//
//     VkPresentModeKHR ChoosePresentMode(VkPresentModeKHR *modes, uint32_t modeCount);
//
//     VkSurfaceFormatKHR ChooseSurfaceFormat(VkSurfaceFormatKHR *formats, uint32_t formatCount);
//
//     VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities);
//
//     VulkanBuffer CreateBuffer(VulkanAllocationInfo &allocationInfo, VkBufferCreateInfo *bufferCreateInfo,
//                               VulkanMemoryUsage usage, VulkanMemoryFlag flags);
//
//     void DestroyBuffer(VulkanBuffer buffer);
//
//     VulkanImage CreateImage(VkImageCreateInfo *imageCreateInfo, VulkanMemoryUsage usage,
//                             VulkanMemoryFlag flags);
//
//     void DestroyImage(VulkanImage image);
//
//     void *MapMemory(VulkanBuffer &buffer);
//     void UnmapMemory(VulkanBuffer &buffer);
//
//     // VlkBufferArena VlkCreateBufferArena(VlkAllocator &allocator,
//     //                                     VkBufferCreateInfo &bufferCreateInfo, uint32_t tag);
//     //
//     // void VlkDestroyBufferArena(VlkAllocator &allocator, uint32_t tag);
// };
// } // namespace VoidEngine
