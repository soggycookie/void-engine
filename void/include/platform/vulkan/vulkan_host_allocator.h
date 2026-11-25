#pragma once
#include "vallocator.h"

#include <atomic>
#include <cstring>
#include <mutex>

#include <vulkan/vulkan.h>

#include "vk_mem_alloc.h"

namespace VoidEngine::Vulkan
{
// Live GPU memory counters, fed by VmaDeviceMemoryCallbacks (vkAllocateMemory / vkFreeMemory).
// The callbacks can fire on any thread that creates Vulkan objects, so everything is atomic.
struct GpuMemStats
{
    std::atomic<uint64_t> live_bytes_[VK_MAX_MEMORY_TYPES];
    std::atomic<uint32_t> live_blocks_[VK_MAX_MEMORY_TYPES];
    std::atomic<uint64_t> total_live_bytes_;
    std::atomic<uint64_t> peak_live_bytes_;
    std::atomic<uint64_t> total_allocate_calls_;
    std::atomic<uint64_t> total_free_calls_;
};

// Owns the CPU-side Vulkan allocator (VkAllocationCallbacks backed by a dedicated, mutex guarded TLFS)
// and the GPU memory statistics (VmaDeviceMemoryCallbacks).
//
// Lifetime: init() before vkCreateInstance, destroy() after vkDestroyInstance. Do not move it after
// init(), the callbacks point at `this`.
// Every vkCreate*/vkDestroy* pair (including the one VMA does internally) must get get_callbacks().
class HostAllocator
{
private:
    TLFS tlfs_;
    std::mutex mutex_;
    VkAllocationCallbacks callbacks_;
    VmaDeviceMemoryCallbacks device_memory_callbacks_;
    GpuMemStats gpu_stats_;
    uint64_t scope_alloc_counts_[5]; // by VkSystemAllocationScope, guarded by mutex_
    bool enabled_;

public:
    HostAllocator() : callbacks_{}, device_memory_callbacks_{}, scope_alloc_counts_{}, enabled_(false)
    {
    }

    NO_COPY_CTOR(HostAllocator);
    NO_COPY_ASSIGNMENT(HostAllocator);

    void init(uint64_t reserve_size, uint64_t commit_size, bool enabled = true);

    // Asserts that every host allocation was freed (a Vulkan object leaked, or a create/destroy pair used
    // different callbacks).
    void destroy();

    // nullptr when disabled, so the driver falls back to its default allocator.
    const VkAllocationCallbacks *get_callbacks() const
    {
        return enabled_ ? &callbacks_ : nullptr;
    }

    // Set VmaAllocatorCreateInfo::pDeviceMemoryCallbacks to this. VMA copies the struct.
    const VmaDeviceMemoryCallbacks *get_device_memory_callbacks() const
    {
        return &device_memory_callbacks_;
    }

    const GpuMemStats &get_gpu_stats() const
    {
        return gpu_stats_;
    }

    // Number of live host allocations / bytes, taken under the lock.
    uint32_t get_host_alloc_count();
    uint64_t get_host_alloc_size();

    // Logs host usage, tracked GPU usage per memory type, and (if vma is not null) VMA's heap budgets.
    void log_stats(VmaAllocator vma = VK_NULL_HANDLE);

private:
    static VKAPI_ATTR void *VKAPI_CALL on_alloc(void *user, size_t size, size_t align,
                                                VkSystemAllocationScope scope);
    static VKAPI_ATTR void *VKAPI_CALL on_realloc(void *user, void *orig, size_t size, size_t align,
                                                  VkSystemAllocationScope scope);
    static VKAPI_ATTR void VKAPI_CALL on_free(void *user, void *mem);

    static VKAPI_ATTR void VKAPI_CALL on_device_alloc(VmaAllocator allocator, uint32_t memory_type,
                                                      VkDeviceMemory memory, VkDeviceSize size,
                                                      void *user);
    static VKAPI_ATTR void VKAPI_CALL on_device_free(VmaAllocator allocator, uint32_t memory_type,
                                                     VkDeviceMemory memory, VkDeviceSize size, void *user);
};
} // namespace VoidEngine::Vulkan
