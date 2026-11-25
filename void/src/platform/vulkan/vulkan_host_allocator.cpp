#include "platform/vulkan/vulkan_host_allocator.h"
#include "logger/log.h"

namespace VoidEngine::Vulkan
{
void HostAllocator::init(uint64_t reserve_size, uint64_t commit_size, bool enabled)
{
    enabled_ = enabled;
    std::memset(scope_alloc_counts_, 0, sizeof(scope_alloc_counts_));

    for (uint32_t idx = 0; idx < VK_MAX_MEMORY_TYPES; ++idx)
    {
        gpu_stats_.live_bytes_[idx].store(0, std::memory_order_relaxed);
        gpu_stats_.live_blocks_[idx].store(0, std::memory_order_relaxed);
    }
    gpu_stats_.total_live_bytes_.store(0, std::memory_order_relaxed);
    gpu_stats_.peak_live_bytes_.store(0, std::memory_order_relaxed);
    gpu_stats_.total_allocate_calls_.store(0, std::memory_order_relaxed);
    gpu_stats_.total_free_calls_.store(0, std::memory_order_relaxed);

    tlfs_.init(reserve_size, commit_size);
#ifdef VOID_DEBUG
    tlfs_.set_name("VulkanHost");
#endif // VOID_DEBUG

    callbacks_ = {
        .pUserData = this,
        .pfnAllocation = &HostAllocator::on_alloc,
        .pfnReallocation = &HostAllocator::on_realloc,
        .pfnFree = &HostAllocator::on_free,
        // must both be null or both be set
        .pfnInternalAllocation = nullptr,
        .pfnInternalFree = nullptr,
    };

    device_memory_callbacks_ = {
        .pfnAllocate = &HostAllocator::on_device_alloc,
        .pfnFree = &HostAllocator::on_device_free,
        .pUserData = this,
    };
}

void HostAllocator::destroy()
{
    // TLFS::destroy prints the block list in Debug and asserts when allocations are still alive.
    tlfs_.destroy();
    enabled_ = false;
}

uint32_t HostAllocator::get_host_alloc_count()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return tlfs_.get_alloc_count();
}

uint64_t HostAllocator::get_host_alloc_size()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return tlfs_.get_alloc_size();
}

void HostAllocator::log_stats(VmaAllocator vma)
{
    uint64_t scope_counts[5];
    uint32_t host_count;
    uint64_t host_size;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::memcpy(scope_counts, scope_alloc_counts_, sizeof(scope_counts));
        host_count = tlfs_.get_alloc_count();
        host_size = tlfs_.get_alloc_size();
    }

    LOG_INFO("VULKAN", "Host allocator (%s): %u live allocations, %llu bytes", enabled_ ? "on" : "off",
             host_count, to_ull(host_size));
    LOG_INFO("VULKAN",
             "Host allocations by scope: command %llu, object %llu, cache %llu, device %llu, instance %llu",
             to_ull(scope_counts[VK_SYSTEM_ALLOCATION_SCOPE_COMMAND]),
             to_ull(scope_counts[VK_SYSTEM_ALLOCATION_SCOPE_OBJECT]),
             to_ull(scope_counts[VK_SYSTEM_ALLOCATION_SCOPE_CACHE]),
             to_ull(scope_counts[VK_SYSTEM_ALLOCATION_SCOPE_DEVICE]),
             to_ull(scope_counts[VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE]));

    LOG_INFO("VULKAN", "GPU memory: live %llu bytes, peak %llu bytes, %llu vkAllocateMemory, %llu vkFreeMemory",
             to_ull(gpu_stats_.total_live_bytes_.load(std::memory_order_relaxed)),
             to_ull(gpu_stats_.peak_live_bytes_.load(std::memory_order_relaxed)),
             to_ull(gpu_stats_.total_allocate_calls_.load(std::memory_order_relaxed)),
             to_ull(gpu_stats_.total_free_calls_.load(std::memory_order_relaxed)));

    for (uint32_t idx = 0; idx < VK_MAX_MEMORY_TYPES; ++idx)
    {
        uint32_t blocks = gpu_stats_.live_blocks_[idx].load(std::memory_order_relaxed);
        uint64_t bytes = gpu_stats_.live_bytes_[idx].load(std::memory_order_relaxed);
        if (blocks != 0 || bytes != 0)
        {
            LOG_INFO("VULKAN", "  memory type %u: %u blocks, %llu bytes", idx, blocks, to_ull(bytes));
        }
    }

    if (vma)
    {
        const VkPhysicalDeviceMemoryProperties *props = nullptr;
        vmaGetMemoryProperties(vma, &props);

        VmaBudget budgets[VK_MAX_MEMORY_HEAPS] = {};
        vmaGetHeapBudgets(vma, budgets);
        for (uint32_t idx = 0; idx < props->memoryHeapCount; ++idx)
        {
            LOG_INFO("VULKAN",
                     "  heap %u: usage %llu / budget %llu bytes (VMA blocks %llu bytes, allocations %llu bytes)",
                     idx, to_ull(budgets[idx].usage), to_ull(budgets[idx].budget),
                     to_ull(budgets[idx].statistics.blockBytes),
                     to_ull(budgets[idx].statistics.allocationBytes));
        }
    }
}

void *HostAllocator::on_alloc(void *user, size_t size, size_t align, VkSystemAllocationScope scope)
{
    HostAllocator *self = SCAST(HostAllocator *, user);
    std::lock_guard<std::mutex> lock(self->mutex_);
    if (scope < 5)
    {
        ++self->scope_alloc_counts_[scope];
    }
    // The spec guarantees a power-of-two alignment.
    return self->tlfs_.tlfs_alloc(size, align);
}

void *HostAllocator::on_realloc(void *user, void *orig, size_t size, size_t align,
                                VkSystemAllocationScope scope)
{
    if (!orig)
    {
        return on_alloc(user, size, align, scope);
    }
    if (size == 0)
    {
        on_free(user, orig);
        return nullptr;
    }

    HostAllocator *self = SCAST(HostAllocator *, user);
    std::lock_guard<std::mutex> lock(self->mutex_);

    uint64_t old_size = self->tlfs_.get_usable_size(orig);
    if (size <= old_size)
    {
        // Shrinking in place keeps the contents and the alignment.
        return orig;
    }

    // The spec says `align` equals the original allocation's alignment.
    void *p = self->tlfs_.tlfs_alloc(size, align);
    if (!p)
    {
        return nullptr; // the original must stay valid on failure
    }
    std::memcpy(p, orig, old_size);
    self->tlfs_.tlfs_free(orig);
    return p;
}

void HostAllocator::on_free(void *user, void *mem)
{
    if (!mem)
    {
        return; // the spec requires a null free to be accepted
    }
    HostAllocator *self = SCAST(HostAllocator *, user);
    std::lock_guard<std::mutex> lock(self->mutex_);
    self->tlfs_.tlfs_free(mem);
}

void HostAllocator::on_device_alloc(VmaAllocator, uint32_t memory_type, VkDeviceMemory, VkDeviceSize size,
                                    void *user)
{
    HostAllocator *self = SCAST(HostAllocator *, user);
    GpuMemStats &stats = self->gpu_stats_;
    ENGINE_ASSERT(memory_type < VK_MAX_MEMORY_TYPES, "Invalid memory type!");

    stats.live_bytes_[memory_type].fetch_add(size, std::memory_order_relaxed);
    stats.live_blocks_[memory_type].fetch_add(1, std::memory_order_relaxed);
    stats.total_allocate_calls_.fetch_add(1, std::memory_order_relaxed);

    uint64_t live = stats.total_live_bytes_.fetch_add(size, std::memory_order_relaxed) + size;
    uint64_t peak = stats.peak_live_bytes_.load(std::memory_order_relaxed);
    while (live > peak &&
           !stats.peak_live_bytes_.compare_exchange_weak(peak, live, std::memory_order_relaxed))
    {
    }
}

void HostAllocator::on_device_free(VmaAllocator, uint32_t memory_type, VkDeviceMemory, VkDeviceSize size,
                                   void *user)
{
    HostAllocator *self = SCAST(HostAllocator *, user);
    GpuMemStats &stats = self->gpu_stats_;
    ENGINE_ASSERT(memory_type < VK_MAX_MEMORY_TYPES, "Invalid memory type!");

    stats.live_bytes_[memory_type].fetch_sub(size, std::memory_order_relaxed);
    stats.live_blocks_[memory_type].fetch_sub(1, std::memory_order_relaxed);
    stats.total_free_calls_.fetch_add(1, std::memory_order_relaxed);
    stats.total_live_bytes_.fetch_sub(size, std::memory_order_relaxed);
}
} // namespace VoidEngine::Vulkan
