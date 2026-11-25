#include "vulkan_backend.h"
#include "application.h"
#include "arena.h"
#include "logger/log.h"
#include "stb/stb_image.h"

#ifdef VOID_WIN32
#include "win32_window.h"
#endif

namespace VoidEngine::Vulkan
{

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
//     ENGINE_ASSERT(m_vulkanAllocator->IsMemoryTypeIndexHost(memoryBlock.memoryTypeIndex),
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
//     ENGINE_ASSERT(m_vulkanAllocator->IsMemoryTypeIndexHost(memoryBlock.memoryTypeIndex),
//                "Memory Type does not contain HOST_VISIBLE!");
//     ENGINE_ASSERT(memoryBlock.mappingCount > 0, "Memory block is already unmapped!");
//
//     --memoryBlock.mappingCount;
//     if (memoryBlock.mappingCount == 0)
//     {
//         vkUnmapMemory(m_context.device, memoryBlock.memory);
//     }
// }

bool VulkanBackend::startup(Window *window)
{
    ENGINE_ASSERT(window, "Allocator cannot be null!");
    window_ = window;
    vallocator_.init(gib(8), mib(1), gib(8), mib(1));
    // Must be ready before the first Vulkan call: the instance is created with these callbacks.
    host_alloc_.init(gib(1), mib(4), true);
    profile_supported_ = VK_TRUE;
    create_instance();

#ifdef VOID_DEBUG
    setup_debug_messenger();
#endif // VOID_DEBUG

    create_surface();

    create_physical_device();
    create_logical_device();
    create_vma_allocator();

    create_swapchain();
    create_swapchain_image_views();
    create_present_semaphores();

    create_sync_objects();
    create_command_pool();
    create_command_buffers();

#if VOID_VK_SCENE_PASS
    create_tex_image();
    create_tex_sampler();

    create_descriptor_set_layout();
    create_uniform_buffer();
    create_descriptor_pool();
    create_descriptor_sets();

    create_graphic_pipeline();
    create_depth_tex();

    CreateVertexBuffer();
    CreateIndexBuffer();
#endif // VOID_VK_SCENE_PASS

    return true;
}

void VulkanBackend::shutdown()
{
    // Nothing may still be executing when objects are destroyed.
    vkDeviceWaitIdle(context_.device);

#if VOID_VK_SCENE_PASS
    // vkDestroySampler(m_context.device, m_sampler, nullptr);
    // vkDestroyImageView(m_context.device, m_texture.view, nullptr);
    // DestroyImage(m_texture.image);

    vkDestroyDescriptorPool(context_.device, m_descriptorPool, vk_alloc());
    vkDestroyDescriptorSetLayout(context_.device, descriptor_set_layout_, vk_alloc());

    // DestroyBuffer(m_vertexBuffer);
    // DestroyBuffer(m_indexBuffer);

    CleanUpDepthTexture();

    vkDestroyPipeline(context_.device, m_pipeline, vk_alloc());
    vkDestroyPipelineLayout(context_.device, m_pipelineLayout, vk_alloc());
#endif // VOID_VK_SCENE_PASS

    for (uint32_t idx = 0; idx < MAX_FRAME_IN_FLIGHT; ++idx)
    {
        vkDestroySemaphore(context_.device, sync_objs[idx].present_complete_sem, vk_alloc());
        vkDestroyFence(context_.device, sync_objs[idx].draw_fence, vk_alloc());
    }
    // Frees the command buffers allocated from it too.
    vkDestroyCommandPool(context_.device, context_.graphic_command_pool, vk_alloc());

    destroy_present_semaphores();
    destroy_swapchain_views_and_images();
    vkDestroySwapchainKHR(context_.device, context_.swapchain, vk_alloc());

    vkDestroySurfaceKHR(context_.instance, context_.surface, vk_alloc());

    // Prints host usage, tracked GPU usage and VMA heap budgets while the allocator is still alive.
    host_alloc_.log_stats(vma_allocator_);

    // VMA must go before the device, it frees its VkDeviceMemory blocks here.
    vmaDestroyAllocator(vma_allocator_);
    vma_allocator_ = VK_NULL_HANDLE;
    vkDestroyDevice(context_.device, vk_alloc());

#ifdef VOID_DEBUG
    PFN_vkDestroyDebugUtilsMessengerEXT debug_fn =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context_.instance,
                                                                   "vkDestroyDebugUtilsMessengerEXT");

    if (debug_fn)
    {
        debug_fn(context_.instance, debug_messenger_, vk_alloc());
    }
    else
    {
        Logger::show_fatal_mbox("Vulkan engine", "Vulkan failed to acquire debug messenger destruction");
    }

#endif // VOID_DEBUG
    vkDestroyInstance(context_.instance, vk_alloc());
    // Asserts on any leaked host allocation, which means a Vulkan object leaked or a create/destroy pair
    // used different callbacks.
    host_alloc_.destroy();
    vallocator_.destroy();
}

static bool is_ext_available(const VkExtensionProperties *ext_properties, uint32_t ext_count,
                             const char *checking_ext)
{
    for (uint64_t idx = 0; idx < ext_count; ++idx)
    {
        const VkExtensionProperties &ext = ext_properties[idx];
        if (strcmp(checking_ext, ext.extensionName) == 0)
        {
            return true;
        }
    }
    return false;
}

static bool is_layer_available(const VkLayerProperties *layer_properties, uint32_t layer_count,
                               const char *checking_layer)
{
    for (uint64_t idx = 0; idx < layer_count; ++idx)
    {
        const VkLayerProperties &layer = layer_properties[idx];
        if (strcmp(checking_layer, layer.layerName) == 0)
        {
            return true;
        }
    }
    return false;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_types,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data)
{
    switch (message_severity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    {
        LOG_TRACE("VULKAN", "Trace: %s", callback_data->pMessage);
        break;
    }
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    {
        LOG_INFO("VULKAN", "Info: %s", callback_data->pMessage);
        break;
    }
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    {
        LOG_WARN("VULKAN", "Warn: %s", callback_data->pMessage);
        break;
    }
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    {
        LOG_ERROR("VULKAN", "Error: %s", callback_data->pMessage);
        break;
    }
    default:
    {
        LOG_DEBUG("VULKAN", "Unknown: %s", callback_data->pMessage);
        break;
    }
    }

    return VK_FALSE;
}

void VulkanBackend::create_surface()
{
    ENGINE_ASSERT(window_, "Window handle must not be null!");
#ifdef VOID_WIN32
    Win32_Window *window = RCAST(Win32_Window *, window_);

    VkWin32SurfaceCreateInfoKHR surface_create_info
    {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .hinstance = window->get_hinstance(),
        .hwnd = window->get_hwnd(),
    };

    if (vkCreateWin32SurfaceKHR(context_.instance, &surface_create_info, vk_alloc(), &context_.surface) !=
        VK_SUCCESS)
    {
        Logger::show_fatal_mbox("Vulkan engine", "Failed to create Win32 surface!");
    }
#else
    //TODO: 
#endif
}

void VulkanBackend::setup_debug_messenger()
{
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &debug_callback,
    };

    PFN_vkCreateDebugUtilsMessengerEXT fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        context_.instance, "vkCreateDebugUtilsMessengerEXT");

    if (fn)
    {
        if (fn(context_.instance, &debug_messenger_create_info, vk_alloc(),
                                           &debug_messenger_) != VK_SUCCESS)
        {
            Logger::show_fatal_mbox("Vulkan engine", "Vulkan debug messenger creation failed!");
        }
    }
    else
    {
        Logger::show_fatal_mbox("Vulkan engine", "Failed to acquire vkCreateDebugUtilsMessengerEXT!");
    }
}

void VulkanBackend::create_logical_device()
{
    ScopedScratchTemp temp;
    Arena *scratch = t_scratches.get_scratch();
    temp.start_scratch(scratch);

    uint32_t queue_fam_prop_count;
    vkGetPhysicalDeviceQueueFamilyProperties2(context_.physical_device, &queue_fam_prop_count, nullptr);

    VkQueueFamilyProperties2 *queue_fam_props =
        arena_push_array<VkQueueFamilyProperties2>(*scratch, queue_fam_prop_count);
    for (uint32_t qidx = 0; qidx < queue_fam_prop_count; ++qidx)
    {
        queue_fam_props[qidx].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        queue_fam_props[qidx].pNext = nullptr;
    }

    vkGetPhysicalDeviceQueueFamilyProperties2(context_.physical_device, &queue_fam_prop_count,
                                              queue_fam_props);

    context_.graphic_queue_family_index = ~0;
    for (uint32_t idx = 0; idx < queue_fam_prop_count; ++idx)
    {
        if (context_.graphic_queue_family_index != ~0)
            break;

        if (context_.graphic_queue_family_index == ~0 && // only look if not found yet
            (queue_fam_props[idx].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0)
        {
            VkBool32 support_surface = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(context_.physical_device, idx, context_.surface,
                                                 &support_surface);
            if (support_surface)
            {
                context_.graphic_queue_family_index = idx;
            }
        }
    }

    if (context_.graphic_queue_family_index == ~0)
    {
        Logger::show_fatal_mbox("Vulkan engine", "Could not find a queue for graphics and present!");
    }

    float queue_priority = 0.5f;
    VkDeviceQueueCreateInfo graphic_queue_create_info
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .queueFamilyIndex = context_.graphic_queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    uint32_t queue_create_info_count = 1;
    VkDeviceQueueCreateInfo device_queue_create_infos[] = {graphic_queue_create_info};

    VkPhysicalDeviceVulkan11Features vulkan11_features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = nullptr,
        .shaderDrawParameters = VK_TRUE,
    };

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state_features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = &vulkan11_features,
        .extendedDynamicState = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 features2
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &extended_dynamic_state_features
    };

    const char *wanted_device_exts[] = {"VK_KHR_swapchain", "VK_EXT_memory_budget"};
    const uint32_t wanted_device_ext_count = sizeof(wanted_device_exts) / sizeof(const char*);

    VkDeviceCreateInfo device_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .flags = 0,
        .queueCreateInfoCount = queue_create_info_count,
        .pQueueCreateInfos = device_queue_create_infos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = wanted_device_ext_count,
        .ppEnabledExtensionNames = wanted_device_exts,
        .pEnabledFeatures = nullptr,
    };

    VpDeviceCreateInfo vp_device_create_info{
        .pCreateInfo = &device_create_info,
        .enabledFullProfileCount = 1,
        .pEnabledFullProfiles = &PROFILE,
    };

    VkResult result =
        vpCreateDevice(context_.physical_device, &vp_device_create_info, vk_alloc(), &context_.device);

    if (result != VK_SUCCESS)
    {
        Logger::show_fatal_mbox("Vulkan engine", "Failed to create logical device!");
    }

    vkGetDeviceQueue(context_.device, context_.graphic_queue_family_index, 0, &context_.graphic_queue);

    if (context_.graphic_queue == VK_NULL_HANDLE)
    {
        Logger::show_fatal_mbox("Vulkan engine", "Failed to get graphic queue!");
    }
    temp.end_scratch();
}

VkSurfaceFormatKHR VulkanBackend::choose_surface_format(VkSurfaceFormatKHR *formats, uint32_t format_count)
{
    // UNORM, not *_SRGB: ImGui's colors (and later the scene's final, tonemapped output) are already
    // sRGB-encoded. An *_SRGB image would encode them a second time on write, which is what made the UI
    // look washed out. A future scene pass must write sRGB-encoded values to the swapchain (encode in
    // its final/tonemap pass) or render into an *_SRGB offscreen target first.
    const VkFormat preferred[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};
    for (VkFormat wanted : preferred)
    {
        for (size_t idx = 0; idx < format_count; ++idx)
        {
            const VkSurfaceFormatKHR &format = formats[idx];
            if (format.format == wanted && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return format;
            }
        }
    }

    LOG_WARN("[VULKAN]", "Surface does not have ideal format!");
    return formats[0];
}

VkPresentModeKHR VulkanBackend::choose_present_mode(VkPresentModeKHR *modes, uint32_t mode_count)
{
    for (size_t idx = 0; idx < mode_count; ++idx)
    {
        const VkPresentModeKHR &mode = modes[idx];
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return mode;
        }
    }

    LOG_INFO("VULKAN", "Present mode does not support MAILBOX!");

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanBackend::choose_swapchain_extent(const VkSurfaceCapabilitiesKHR &capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }

    uint32_t width = window_->get_framebuffer_size().width;
    uint32_t height = window_->get_framebuffer_size().height;

    VkExtent2D extent = {
        .width = width,
        .height = height,
    };
    extent.width =
        std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
    return extent;
}

void VulkanBackend::create_vma_allocator()
{
    VmaAllocatorCreateInfo info = {};
    info.vulkanApiVersion = VK_API_VERSION_1_3; // must not exceed the instance's apiVersion
    info.instance = context_.instance;
    info.physicalDevice = context_.physical_device;
    info.device = context_.device;
    // Same callbacks as the device: VMA passes them to vkAllocateMemory/vkCreateBuffer/... and their
    // destroy counterparts, and uses them for its own bookkeeping.
    info.pAllocationCallbacks = vk_alloc();
    // GPU memory statistics, VMA copies the struct.
    info.pDeviceMemoryCallbacks = host_alloc_.get_device_memory_callbacks();

    VkResult result = vmaCreateAllocator(&info, &vma_allocator_);
    ENGINE_ASSERT(result == VK_SUCCESS, "Failed to create VMA allocator!");
}

void VulkanBackend::create_swapchain(VkSwapchainKHR old_swapchain)
{
    ScopedScratchTemp temp;
    Arena *scratch = t_scratches.get_scratch();
    temp.start_scratch(scratch);

    VkSurfaceCapabilitiesKHR surface_capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physical_device, context_.surface,
                                              &surface_capabilities);

    uint32_t surface_format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physical_device, context_.surface, &surface_format_count,
                                         nullptr);

    VkSurfaceFormatKHR *surface_formats =
        arena_push_array<VkSurfaceFormatKHR>(*scratch, surface_format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(context_.physical_device, context_.surface, &surface_format_count,
                                         surface_formats);

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physical_device, context_.surface,
                                              &present_mode_count, nullptr);

    VkPresentModeKHR *present_modes = arena_push_array<VkPresentModeKHR>(*scratch, present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context_.physical_device, context_.surface,
                                              &present_mode_count, present_modes);

    VkSurfaceFormatKHR format = choose_surface_format(surface_formats, surface_format_count);
    VkPresentModeKHR mode = choose_present_mode(present_modes, present_mode_count);
    VkExtent2D extent2d = choose_swapchain_extent(surface_capabilities);

    context_.surface_format = format;
    context_.extent = extent2d;

    uint32_t image_count = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount)
    {
        image_count = surface_capabilities.maxImageCount;
    }
    context_.swapchain_min_image_count = image_count;

    VkSwapchainCreateInfoKHR swapchain_create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = context_.surface,
        .minImageCount = image_count,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent2d,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = mode,
        .clipped = VK_TRUE,
        .oldSwapchain = old_swapchain,
    };

    if (vkCreateSwapchainKHR(context_.device, &swapchain_create_info, vk_alloc(), &context_.swapchain) !=
        VK_SUCCESS)
    {
        Logger::show_fatal_mbox("Vulkan engine", "Failed to create swapchain!");
    }

    vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &context_.swapchain_image_count,
                            nullptr);

    if (context_.swapchain_image_count == 0)
    {
        Logger::show_fatal_mbox("Vulkan engine", "Swapchain image count is 0!");
    }

    context_.swapchain_images =
        SCAST(VkImage *,
              vallocator_.valloc(sizeof(VkImage) * context_.swapchain_image_count, alignof(VkImage)));
    ENGINE_ASSERT(context_.swapchain_images, "Failed to allocate!");
    vkGetSwapchainImagesKHR(context_.device, context_.swapchain, &context_.swapchain_image_count,
                            context_.swapchain_images);
}

void VulkanBackend::create_swapchain_image_views()
{
    context_.swapchain_image_views =
        SCAST(VkImageView *, vallocator_.valloc(sizeof(VkImageView) * context_.swapchain_image_count,
                                                alignof(VkImageView)));
    for (uint32_t idx = 0; idx < context_.swapchain_image_count; ++idx)
    {
        context_.swapchain_image_views[idx] = CreateImageView(
            context_.swapchain_images[idx], context_.surface_format.format, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

#if VOID_VK_SCENE_PASS // scene pass compiled out while ImGui is brought up
void VulkanBackend::create_graphic_pipeline()
{
    auto shader = ReadFile("D:\\Dev\\nvim\\void-engine\\void-"
                           "engine\\resource\\shader\\spirv\\triangle.spv");
    VkShaderModule module = CreateShaderModule(shader);

    VkPipelineShaderStageCreateInfo vertStage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = module,
        .pName = "vertMain",
        .pSpecializationInfo = nullptr,
    };
    VkPipelineShaderStageCreateInfo fragStage{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = module,
        .pName = "fragMain",
        .pSpecializationInfo = nullptr,
    };

    VkPipelineShaderStageCreateInfo shaderStages[]{vertStage, fragStage};

    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attributeDescriptions,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(context_.extent.width),
        .height = static_cast<float>(context_.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,

    };

    VkRect2D scissor{
        .offset = VkOffset2D{0, 0},
        .extent = context_.extent,
    };

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterizationState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampleState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,

    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlendState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    VkPipelineLayoutCreateInfo pipelineLayout{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_set_layout_,
        .pushConstantRangeCount = 0,
    };

    vkCreatePipelineLayout(context_.device, &pipelineLayout, vk_alloc(), &m_pipelineLayout);

    VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    VkPipelineRenderingCreateInfo pipelineRendering{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &context_.surface_format.format,
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
    };

    VkGraphicsPipelineCreateInfo graphicPipeline{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipelineRendering,
        .flags = 0,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputState,
        .pInputAssemblyState = &inputAssemblyState,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = m_pipelineLayout,
        .renderPass = nullptr,
    };

    vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &graphicPipeline, vk_alloc(),
                              &m_pipeline);

    vkDestroyShaderModule(context_.device, module, vk_alloc());
}

VkShaderModule VulkanBackend::CreateShaderModule(const std::vector<char> &binaryCode)
{
    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = binaryCode.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(binaryCode.data()),
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;

    vkCreateShaderModule(context_.device, &createInfo, vk_alloc(), &shaderModule);

    return shaderModule;
}

#endif // VOID_VK_SCENE_PASS

void VulkanBackend::create_command_pool()
{
    VkCommandPoolCreateInfo cmd_pool_create_info
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context_.graphic_queue_family_index,
    };
    vkCreateCommandPool(context_.device, &cmd_pool_create_info, vk_alloc(), &context_.graphic_command_pool);
}

#if VOID_VK_SCENE_PASS
void VulkanBackend::CreateVertexBuffer()
{
    VkDeviceSize size = sizeof(vertices);

    VkBufferCreateInfo stageBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VulkanAllocationInfo allocInfo;

    VulkanBuffer stageBuffer =
        CreateBuffer(allocInfo, &stageBufferCreateInfo, VulkanMemoryUsage::VULKAN_MEMORY_USAGE_CPU_ONLY,
                     VulkanMemoryFlagBit::VULKAN_MEMORY_FLAG_CPU_SEQUENTIAL_WRITE_BIT);

    void *stageData = MapMemory(stageBuffer);

    std::memcpy(stageData, vertices, size);
    UnmapMemory(stageBuffer);

    VkBufferCreateInfo vertexBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VulkanAllocationInfo vertexAllocInfo;
    m_vertexBuffer =
        CreateBuffer(vertexAllocInfo, &vertexBufferCreateInfo, VULKAN_MEMORY_USAGE_GPU_ONLY, 0);

    CopyBuffer(m_vertexBuffer.buffer, stageBuffer.buffer, size);

    DestroyBuffer(stageBuffer);
}

void VulkanBackend::CreateIndexBuffer()
{
    VkDeviceSize size = sizeof(indices);

    VkBufferCreateInfo stageBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VulkanAllocationInfo allocInfo;

    VulkanBuffer stageBuffer =
        CreateBuffer(allocInfo, &stageBufferCreateInfo, VulkanMemoryUsage::VULKAN_MEMORY_USAGE_CPU_ONLY,
                     VulkanMemoryFlagBit::VULKAN_MEMORY_FLAG_CPU_SEQUENTIAL_WRITE_BIT);

    void *stageData = MapMemory(stageBuffer);

    std::memcpy(stageData, indices, size);
    UnmapMemory(stageBuffer);

    VkBufferCreateInfo indexBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = size,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VulkanAllocationInfo indexAllocInfo;

    m_indexBuffer =
        CreateBuffer(indexAllocInfo, &indexBufferCreateInfo, VULKAN_MEMORY_USAGE_GPU_ONLY, 0);

    CopyBuffer(m_indexBuffer.buffer, stageBuffer.buffer, size);

    DestroyBuffer(stageBuffer);
}

void VulkanBackend::create_descriptor_pool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{{
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = s_kMaxFrameInFlight,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = s_kMaxFrameInFlight,
        },
    }};

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = s_kMaxFrameInFlight,
        .poolSizeCount = poolSizes.size(),
        .pPoolSizes = poolSizes.data(),
    };

    vkCreateDescriptorPool(context_.device, &descriptorPoolCreateInfo, vk_alloc(), &m_descriptorPool);
}

void VulkanBackend::create_descriptor_set_layout()
{
    VkDescriptorSetLayoutBinding dset_layout_bindings[] =
    {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        },
    };

    VkDescriptorSetLayoutCreateInfo dset_layout_create_info
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = 2,
        .pBindings = dset_layout_bindings,
    };
    vkCreateDescriptorSetLayout(context_.device, &dset_layout_create_info, vk_alloc(),
                                &descriptor_set_layout_);
}

void VulkanBackend::create_descriptor_sets()
{
    VkDescriptorSetLayout dset_layout[] = {descriptor_set_layout_, descriptor_set_layout_};

    VkDescriptorSetAllocateInfo dset_alloc_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = m_descriptorPool,
        .descriptorSetCount = 2,
        .pSetLayouts = dset_layout,
    };

    descriptor_sets_.init(vallocator_, MAX_FRAME_IN_FLIGHT);
    vkAllocateDescriptorSets(context_.device, &dset_alloc_info, descriptor_sets_.mut_data());
    descriptor_sets_.set_count(MAX_FRAME_IN_FLIGHT);

    for (uint32_t idx = 0; idx < MAX_FRAME_IN_FLIGHT; ++idx)
    {
        VkDescriptorBufferInfo bufferInfo
        {
            .buffer = m_uniformBuffers[idx].buffer,
            .offset = 0,
            .range = sizeof(UniformBufferObject),
        };

        VkDescriptorImageInfo imageInfo{
            .sampler = m_sampler,
            .imageView = m_texture.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{
            {{
                 .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = m_descriptorSets[idx],
                 .dstBinding = 0,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                 .pBufferInfo = &bufferInfo,
             },
             {
                 .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .pNext = nullptr,
                 .dstSet = m_descriptorSets[idx],
                 .dstBinding = 1,
                 .dstArrayElement = 0,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 .pImageInfo = &imageInfo,
             }}};

        vkUpdateDescriptorSets(context_.device, writeDescriptorSets.size(), writeDescriptorSets.data(),
                               0, nullptr);
    }
}

void VulkanBackend::create_uniform_buffer()
{
    m_uniformBuffers.resize(s_kMaxFrameInFlight);
    m_mappedUniformBuffers.resize(s_kMaxFrameInFlight);

    VkDeviceSize size = sizeof(UniformBufferObject);
    for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
    {
        VkBufferCreateInfo bufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .size = size,
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
        };

        VulkanAllocationInfo allocInfo;
        m_uniformBuffers[idx] = CreateBuffer(allocInfo, &bufferCreateInfo, VULKAN_MEMORY_USAGE_CPU_ONLY,
                                             VULKAN_MEMORY_FLAG_CPU_SEQUENTIAL_WRITE_BIT |
                                                 VULKAN_MEMORY_FLAG_PERSISTANT_MAPPED_BIT);
        m_mappedUniformBuffers[idx] = allocInfo.mappedData;
    }
}

void VulkanBackend::UpdateUniformBuffer(uint32_t frameIndex)
{
    static float rad = Rad(90.0f);
    rad += Application::get_app().get_delta_time();

    UniformBufferObject ubo;
    ubo.model = Rotate(Mat4(1.0f), rad, Vec3(0.0f, 0.0f, 1.0f));

    ubo.view = LookAt(Vec3(2.0f, 2.0f, 2.0f), Vec3(0.0f), Vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = Perspective(Rad(45.0f),
                           static_cast<float>(context_.extent.width) /
                               static_cast<float>(context_.extent.height),
                           0.1f, 10.0f);

    // Vulkan has inverted y axis
    ubo.proj[1][1] *= -1;

    std::memcpy(m_mappedUniformBuffers[frameIndex], &ubo, sizeof(UniformBufferObject));
}

void VulkanBackend::create_tex_image()
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc *pixels = stbi_load("D:\\Dev\\nvim\\void-engine\\void-"
                                "engine\\resource\\textures\\texture.jpg",
                                &width, &height, &channels, STBI_rgb_alpha);

    VkDeviceSize imageSize = width * height * 4;

    if (!pixels)
    {
        LOG_ERROR("VULKAN", "Failed to load texture");
    }

    VkBufferCreateInfo stageBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = imageSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VulkanAllocationInfo allocationInfo;
    VulkanBuffer stageBuffer = CreateBuffer(
        allocationInfo, &stageBufferCreateInfo, VulkanMemoryUsage::VULKAN_MEMORY_USAGE_CPU_ONLY,
        VulkanMemoryFlagBit::VULKAN_MEMORY_FLAG_CPU_SEQUENTIAL_WRITE_BIT);

    void *mappedData = MapMemory(stageBuffer);
    std::memcpy(mappedData, pixels, imageSize);
    UnmapMemory(stageBuffer);

    stbi_image_free(pixels);

    VkImageCreateInfo imageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {SCAST(uint32_t, width), SCAST(uint32_t, height), 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    m_texture.image = CreateImage(&imageCreateInfo, VULKAN_MEMORY_USAGE_GPU_ONLY, 0);

    m_texture.view =
        CreateImageView(m_texture.image.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    VkCommandBufferAllocateInfo commandAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.graphic_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(context_.device, &commandAllocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    TransitionImageLayout(cmdBuffer, m_texture.image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    CopyBufferToImage(cmdBuffer, stageBuffer.buffer, m_texture.image.image, width, height);

    TransitionImageLayout(cmdBuffer, m_texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkEndCommandBuffer(cmdBuffer);
    VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmdBuffer,
    };

    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufferSubmitInfo,
    };

    vkQueueSubmit2(context_.graphic_queue, 1, &submitInfo, VK_NULL_HANDLE);

    vkQueueWaitIdle(context_.graphic_queue);

    DestroyBuffer(stageBuffer);
}

void VulkanBackend::CreateTextureImageView()
{
}

void VulkanBackend::create_tex_sampler()
{
    VkPhysicalDeviceProperties2 physicalDeviceProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = nullptr,
    };

    vkGetPhysicalDeviceProperties2(context_.physical_device, &physicalDeviceProperties);

    VkSamplerCreateInfo samplerCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = physicalDeviceProperties.properties.limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    vkCreateSampler(context_.device, &samplerCreateInfo, vk_alloc(), &m_sampler);
}

void VulkanBackend::create_depth_tex()
{
    VkImageCreateInfo imageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = {context_.extent.width, context_.extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    m_depthTex.image = CreateImage(&imageCreateInfo, VULKAN_MEMORY_USAGE_GPU_ONLY, 0);

    m_depthTex.view =
        CreateImageView(m_depthTex.image.image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanBackend::CleanUpDepthTexture()
{

    vkDestroyImageView(context_.device, m_depthTex.view, vk_alloc());
    DestroyImage(m_depthTex.image);
}

#endif // VOID_VK_SCENE_PASS

VkImageView VulkanBackend::CreateImageView(const VkImage &image, VkFormat format,
                                           VkImageAspectFlags imageFlags)
{
    VkImageViewCreateInfo imageViewCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {.aspectMask = imageFlags,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1},
    };

    VkImageView view;
    vkCreateImageView(context_.device, &imageViewCreateInfo, vk_alloc(), &view);

    return view;
}

// VulkanBackend::Texture VulkanBackend::CreateImage(uint32_t texWidth, uint32_t texHeight,
//                                                   VkFormat format, VkImageTiling tilt,
//                                                   VkImageUsageFlags usage,
//                                                   VkMemoryPropertyFlags properties)
// {
//     VkImageCreateInfo imageCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//         .pNext = nullptr,
//         .imageType = VK_IMAGE_TYPE_2D,
//         .format = format,
//         .extent = {texWidth, texHeight, 1},
//         .mipLevels = 1,
//         .arrayLayers = 1,
//         .samples = VK_SAMPLE_COUNT_1_BIT,
//         .tiling = tilt,
//         .usage = usage,
//         .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
//     };
//
//     VkImage image;
//
//     vkCreateImage(m_context.device, &imageCreateInfo, nullptr, &image);
//
//     VkMemoryRequirements memReqs;
//
//     vkGetImageMemoryRequirements(m_context.device, image, &memReqs);
//     VkMemoryAllocateInfo allocInfo{
//         .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
//         .pNext = nullptr,
//         .allocationSize = memReqs.size,
//         .memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, properties),
//     };
//
//     VkDeviceMemory memory;
//
//     vkAllocateMemory(m_context.device, &allocInfo, nullptr, &memory);
//
//     vkBindImageMemory(m_context.device, image, memory, 0);
//
//     return Texture{.image = image, .memory = memory};
// }

#if VOID_VK_SCENE_PASS
void VulkanBackend::CopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer src, VkImage image,
                                      uint32_t width, uint32_t height)
{
    VkBufferImageCopy bufferImageCopy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1},
    };

    vkCmdCopyBufferToImage(cmdBuffer, src, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &bufferImageCopy);
}

void VulkanBackend::CopyBuffer(VkBuffer dst, VkBuffer src, VkDeviceSize size)
{

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.graphic_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(context_.device, &allocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    VkBufferCopy region{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size,
    };

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    vkCmdCopyBuffer(cmdBuffer, src, dst, 1, &region);

    vkEndCommandBuffer(cmdBuffer);

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmdBuffer,
    };

    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufferSubmitInfo,
    };

    vkQueueSubmit2(context_.graphic_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_.graphic_queue);
}

#endif // VOID_VK_SCENE_PASS

void VulkanBackend::create_command_buffers()
{
    VkCommandBufferAllocateInfo cmd_buf_alloc_info
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.graphic_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAME_IN_FLIGHT,
    };
    vkAllocateCommandBuffers(context_.device, &cmd_buf_alloc_info, m_graphicCmdBuffers);
}

void VulkanBackend::TransitionSwapchainImageLayout(uint32_t imageIndex, VkImageLayout oldLayout,
                                                   VkImageLayout newLayout, VkAccessFlags2 srcAccessMask,
                                                   VkAccessFlags2 dstAccessMask,
                                                   VkPipelineStageFlags2 srcStageMask,
                                                   VkPipelineStageFlags2 dstStageMask)
{
    VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = context_.swapchain_images[imageIndex],
    };

    barrier.subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(m_graphicCmdBuffers[m_frameIndex], &dependencyInfo);
}

void VulkanBackend::TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
                                          VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier2 imageMemBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        // .srcStageMask = ,
        // .srcAccessMask = ,
        // .dstStageMask = ,
        // .dstAccessMask = ,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1},
    };

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        imageMemBarrier.srcAccessMask = {};
        imageMemBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

        imageMemBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        imageMemBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        imageMemBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        imageMemBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        imageMemBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        imageMemBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    }
    else
    {
        LOG_ERROR("VULKAN", "Error!");
        throw std::runtime_error("VULKAN Error");
    }

    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageMemBarrier,
    };

    vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
}

void VulkanBackend::TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
                                          VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage,
                                          VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess,
                                          VkImageLayout oldLayout, VkImageLayout newLayout,
                                          uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex,
                                          VkImageAspectFlags imageAspect)
{
    VkImageMemoryBarrier2 imageMemBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .pNext = nullptr,
        .srcStageMask = srcStage,
        .srcAccessMask = srcAccess,
        .dstStageMask = dstStage,
        .dstAccessMask = dstAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = srcQueueFamilyIndex,
        .dstQueueFamilyIndex = dstQueueFamilyIndex,
        .image = image,
        .subresourceRange = {.aspectMask = imageAspect, .levelCount = 1, .layerCount = 1},
    };

    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageMemBarrier,
    };

    vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
}

// VulkanBackend::Buffer VulkanBackend::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags
// usageFlag,
//                                                   VkSharingMode sharingMode,
//                                                   VkMemoryPropertyFlags propertyFlag,
//                                                   uint32_t queueFamilyCount,
//                                                   uint32_t *queueFamilyIndices)
// {
//     Buffer buffer;
//
//     VkBufferCreateInfo bufferCreateInfo{
//         .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
//         .pNext = nullptr,
//         .size = size,
//         .usage = usageFlag,
//         .sharingMode = sharingMode,
//         .queueFamilyIndexCount = queueFamilyCount,
//         .pQueueFamilyIndices = queueFamilyIndices,
//     };
//
//     vkCreateBuffer(m_context.device, &bufferCreateInfo, nullptr, &buffer.buffer);
//
//     VkMemoryRequirements memoryReqs;
//
//     vkGetBufferMemoryRequirements(m_context.device, buffer.buffer, &memoryReqs);
//
//     VkMemoryAllocateInfo memoryAllocInfo{
//         .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
//         .pNext = nullptr,
//         .allocationSize = memoryReqs.size,
//         .memoryTypeIndex = FindMemoryType(memoryReqs.memoryTypeBits, propertyFlag),
//     };
//
//     vkAllocateMemory(m_context.device, &memoryAllocInfo, nullptr, &buffer.memory);
//     vkBindBufferMemory(m_context.device, buffer.buffer, buffer.memory, 0);
//
//     return buffer;
// }

#if VOID_VK_SCENE_PASS // stale: superseded by draw_frame, kept for the scene pass merge
void VulkanBackend::RecordCommandBuffer(uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = 0,
    };

    vkBeginCommandBuffer(m_graphicCmdBuffers[m_frameIndex], &beginInfo);

    TransitionSwapchainImageLayout(
        imageIndex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    TransitionImageLayout(
        m_graphicCmdBuffers[m_frameIndex], m_depthTex.image.image,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkClearValue clearColor{
        .color = VkClearColorValue{{0.0f, 0.0f, 0.0f, 0.0f}},
    };

    VkClearValue clearDepth = {.depthStencil = {.depth = 1.0f, .stencil = 0}};

    VkRenderingAttachmentInfo depthAttachmentInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .pNext = nullptr,
                                                  .imageView = m_depthTex.view,
                                                  .imageLayout =
                                                      VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                  .clearValue = clearDepth};

    VkRenderingAttachmentInfo renderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = context_.swapchain_image_views[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearColor};

    VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .pNext = nullptr,
                                  .renderArea = {.offset = {0, 0}, .extent = context_.extent},
                                  .layerCount = 1,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &renderingAttachmentInfo,
                                  .pDepthAttachment = &depthAttachmentInfo};

    vkCmdBeginRendering(m_graphicCmdBuffers[m_frameIndex], &renderingInfo);

    vkCmdBindPipeline(m_graphicCmdBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(context_.extent.width),
        .height = static_cast<float>(context_.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,

    };

    VkRect2D scissor{
        .offset = VkOffset2D{0, 0},
        .extent = context_.extent,
    };

    vkCmdSetViewport(m_graphicCmdBuffers[m_frameIndex], 0, 1, &viewport);
    vkCmdSetScissor(m_graphicCmdBuffers[m_frameIndex], 0, 1, &scissor);

    vkCmdBindPipeline(m_graphicCmdBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_graphicCmdBuffers[m_frameIndex], 0, 1, &m_vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(m_graphicCmdBuffers[m_frameIndex], m_indexBuffer.buffer, 0,
                         VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(m_graphicCmdBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 1, &descriptor_sets_[m_frameIndex], 0, nullptr);

    vkCmdDrawIndexed(m_graphicCmdBuffers[m_frameIndex], 12, 1, 0, 0, 0);

    vkCmdEndRendering(m_graphicCmdBuffers[m_frameIndex]);

    TransitionSwapchainImageLayout(
        imageIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {}, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(m_graphicCmdBuffers[m_frameIndex]);
}

#endif // VOID_VK_SCENE_PASS

void VulkanBackend::create_sync_objects()
{
    VkSemaphoreCreateInfo present_complete{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };

    VkFenceCreateInfo draw_fence{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for (uint32_t idx = 0; idx < MAX_FRAME_IN_FLIGHT; ++idx)
    {
        vkCreateSemaphore(context_.device, &present_complete, vk_alloc(),
                          &sync_objs[idx].present_complete_sem);
        vkCreateFence(context_.device, &draw_fence, vk_alloc(), &sync_objs[idx].draw_fence);
    }
}

void VulkanBackend::create_present_semaphores()
{
    VkSemaphoreCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    render_finished_sem_count_ = context_.swapchain_image_count;
    render_finished_sems_ = SCAST(VkSemaphore *, vallocator_.valloc(sizeof(VkSemaphore) * render_finished_sem_count_,
                                                                    alignof(VkSemaphore)));
    ENGINE_ASSERT(render_finished_sems_, "Failed to allocate!");
    for (uint32_t idx = 0; idx < render_finished_sem_count_; ++idx)
    {
        vkCreateSemaphore(context_.device, &info, vk_alloc(), &render_finished_sems_[idx]);
    }
}

void VulkanBackend::destroy_present_semaphores()
{
    if (!render_finished_sems_)
    {
        return;
    }
    for (uint32_t idx = 0; idx < render_finished_sem_count_; ++idx)
    {
        vkDestroySemaphore(context_.device, render_finished_sems_[idx], vk_alloc());
    }
    vallocator_.vfree(sizeof(VkSemaphore) * render_finished_sem_count_, render_finished_sems_);
    render_finished_sems_ = nullptr;
    render_finished_sem_count_ = 0;
}

void VulkanBackend::destroy_swapchain_views_and_images()
{
    if (context_.swapchain_image_views)
    {
        for (uint32_t idx = 0; idx < context_.swapchain_image_count; ++idx)
        {
            vkDestroyImageView(context_.device, context_.swapchain_image_views[idx], vk_alloc());
        }
        vallocator_.vfree(sizeof(VkImageView) * context_.swapchain_image_count,
                          context_.swapchain_image_views);
        context_.swapchain_image_views = nullptr;
    }
    // The images belong to the swapchain; only the handle array is ours.
    if (context_.swapchain_images)
    {
        vallocator_.vfree(sizeof(VkImage) * context_.swapchain_image_count, context_.swapchain_images);
        context_.swapchain_images = nullptr;
    }
}

void VulkanBackend::draw_frame(ImDrawData *ui)
{
    ClientDimension fb = window_->get_framebuffer_size();
    if (fb.width <= 0 || fb.height <= 0)
    {
        return; // minimized: a 0x0 swapchain is invalid
    }
    if (SCAST(uint32_t, fb.width) != context_.extent.width ||
        SCAST(uint32_t, fb.height) != context_.extent.height)
    {
        recreate_swapchain();
    }

    SyncObjects &sync = sync_objs[m_frameIndex];
    VkCommandBuffer cmd = m_graphicCmdBuffers[m_frameIndex];

    // 1. The previous submit that used this frame slot (cmd buffer, acquire semaphore) must be done.
    if (vkWaitForFences(context_.device, 1, &sync.draw_fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "vkWaitForFences failed");
        return;
    }

    // 2. Acquire.
    uint32_t image_index = UINT32_MAX;
    VkResult acquire_result = vkAcquireNextImageKHR(context_.device, context_.swapchain, UINT64_MAX,
                                                    sync.present_complete_sem, VK_NULL_HANDLE, &image_index);
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate_swapchain();
        return; // fence untouched, still signaled
    }
    if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR)
    {
        LOG_ERROR("VULKAN", "vkAcquireNextImageKHR failed: %d", SCAST(int, acquire_result));
        return;
    }

    // 3. Reset the fence only now that work will definitely be submitted.
    vkResetFences(context_.device, 1, &sync.draw_fence);

    // 4. Record: clear, UI, transition for present.
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(cmd, &begin_info);

    TransitionSwapchainImageLayout(image_index, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_ACCESS_2_NONE, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkRenderingAttachmentInfo color_attachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = context_.swapchain_image_views[image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = {{0.1f, 0.1f, 0.12f, 1.0f}}},
    };
    VkRenderingInfo rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .renderArea = {.offset = {0, 0}, .extent = context_.extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment,
    };
    // Scene pass goes here when VOID_VK_SCENE_PASS is re-enabled; the UI pass then uses LOAD_OP_LOAD.
    vkCmdBeginRendering(cmd, &rendering_info);
    record_imgui(cmd, ui);
    vkCmdEndRendering(cmd);

    TransitionSwapchainImageLayout(image_index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                   VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
    vkEndCommandBuffer(cmd);

    // 5. Submit: wait for acquire, signal the per-image render-finished semaphore and the frame fence.
    VkSemaphore render_finished = render_finished_sems_[image_index];
    VkSemaphoreSubmitInfo wait_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = sync.present_complete_sem,
        .value = 0,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    VkSemaphoreSubmitInfo signal_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = render_finished,
        .value = 0,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    VkCommandBufferSubmitInfo cmd_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = cmd,
    };
    VkSubmitInfo2 submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .pNext = nullptr,
        .flags = 0,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &wait_info,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_info,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal_info,
    };
    if (vkQueueSubmit2(context_.graphic_queue, 1, &submit_info, sync.draw_fence) != VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "vkQueueSubmit2 failed");
    }

    // 6. Present.
    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_finished,
        .swapchainCount = 1,
        .pSwapchains = &context_.swapchain,
        .pImageIndices = &image_index,
    };
    VkResult present_result = vkQueuePresentKHR(context_.graphic_queue, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
    {
        recreate_swapchain();
    }
    else if (present_result != VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "vkQueuePresentKHR failed: %d", SCAST(int, present_result));
    }

    // 7. Next frame slot.
    m_frameIndex = (m_frameIndex + 1) % MAX_FRAME_IN_FLIGHT;
}

#if VOID_VK_SCENE_PASS // stale: superseded by draw_frame
void VulkanBackend::DrawFrame()
{

    uint32_t imageIndex = UINT32_MAX;

    VkResult acquireResult = vkAcquireNextImageKHR(context_.device, context_.swapchain, UINT64_MAX,
                                                   sync_objs[m_frameIndex].present_complete_sem,
                                                   VK_NULL_HANDLE, &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate_swapchain();
        return;
    }
    else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
    {
        ENGINE_ASSERT(acquireResult == VK_TIMEOUT || acquireResult == VK_NOT_READY,
                      "Failed to acquire next image to present");
    }

    if (vkWaitForFences(context_.device, 1, &sync_objs[m_frameIndex].draw_fence, VK_TRUE, UINT64_MAX) !=
        VK_SUCCESS)
    {
        ENGINE_ASSERT("VULKAN", "Failed to wait for draw fence");
    }

    RecordCommandBuffer(imageIndex);

    VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = sync_objs[m_frameIndex].present_complete_sem,
        .value = 0,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = sync_objs[m_frameIndex].render_finished_sem,
        .value = 0,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBuffer = m_graphicCmdBuffers[m_frameIndex],
    };

    UpdateUniformBuffer(m_frameIndex);

    VkSubmitInfo2 submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                             .pNext = nullptr,
                             .flags = 0,
                             .waitSemaphoreInfoCount = 1,
                             .pWaitSemaphoreInfos = &waitSemaphoreSubmitInfo,
                             .commandBufferInfoCount = 1,
                             .pCommandBufferInfos = &cmdBufferSubmitInfo,
                             .signalSemaphoreInfoCount = 1,
                             .pSignalSemaphoreInfos = &signalSemaphoreSubmitInfo};

    vkResetFences(context_.device, 1, &sync_objs[m_frameIndex].draw_fence);
    vkQueueSubmit2(context_.graphic_queue, 1, &submitInfo, sync_objs[m_frameIndex].draw_fence);

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &sync_objs[m_frameIndex].render_finished_sem,
        .swapchainCount = 1,
        .pSwapchains = &context_.swapchain,
        .pImageIndices = &imageIndex,
    };

    VkResult presentResult = vkQueuePresentKHR(context_.graphic_queue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
        Application::get_app().is_app_resizing())
    {
        recreate_swapchain();
    }
    else if (presentResult != VK_SUCCESS)
    {
        ENGINE_ASSERT(presentResult == VK_TIMEOUT || presentResult == VK_NOT_READY,
                      "Failed to acquire next image to present");
    }

    vkQueueWaitIdle(context_.graphic_queue);
}

void VulkanBackend::DrawTest()
{
    DrawFrame();
    m_frameIndex = (m_frameIndex++ % s_kMaxFrameInFlight);

    // vkQueueWaitIdle(m_context.graphicQueue);
}

#endif // VOID_VK_SCENE_PASS

void VulkanBackend::recreate_swapchain()
{
    ClientDimension fb = window_->get_framebuffer_size();
    if (fb.width <= 0 || fb.height <= 0)
    {
        return; // minimized: try again once the window has a size
    }

    // Views, images and present semaphores may still be in use by frames in flight / the present engine.
    vkDeviceWaitIdle(context_.device);

    destroy_present_semaphores();
    destroy_swapchain_views_and_images();

    VkSwapchainKHR old_swapchain = context_.swapchain;
    create_swapchain(old_swapchain);
    vkDestroySwapchainKHR(context_.device, old_swapchain, vk_alloc());

    create_swapchain_image_views();
    create_present_semaphores();

#if VOID_VK_SCENE_PASS
    CleanUpDepthTexture();
    create_depth_tex();
#endif // VOID_VK_SCENE_PASS
    // ImGui's pipeline uses dynamic rendering + dynamic viewport, so it does not depend on the extent.
}

void VulkanBackend::create_instance()
{
    ScopedScratchTemp temp;
    Arena *scratch = t_scratches.get_scratch();
    temp.start_scratch(scratch);
    VkBool32 profile_supported;

    vpGetInstanceProfileSupport(nullptr, &PROFILE, &profile_supported);

    if (!profile_supported)
    {
        // TODO: Fallback path
        profile_supported_ = VK_FALSE;
        ENGINE_ASSERT("VULKAN", "Vulkan profile not supported");
    }

    const char *const wanted_exts[] = {
        "VK_KHR_surface",
#ifdef VOID_WIN32
        "VK_KHR_win32_surface",
#else
#endif // VOID_WIN32

#ifdef VOID_DEBUG
        "VK_EXT_debug_utils",
#endif // VOID_DEBUG
    };
    const uint32_t wanted_ext_count = sizeof(wanted_exts) / sizeof(const char *);

    uint32_t avai_ext_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &avai_ext_count, nullptr);
    VkExtensionProperties *avai_ext_properties =
        arena_push_array<VkExtensionProperties>(*scratch, avai_ext_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &avai_ext_count, avai_ext_properties);

    DynamicArray<const char *> exts;
    exts.init(*scratch, wanted_ext_count);
    for (uint32_t idx = 0; idx < wanted_ext_count; ++idx)
    {
        bool avai = is_ext_available(avai_ext_properties, avai_ext_count, wanted_exts[idx]);
        if (avai)
        {
            exts.push_back(*scratch, wanted_exts[idx]);
        }
    }

    const char *const wanted_layers[] = {
#ifdef VOID_DEBUG
        "VK_LAYER_KHRONOS_validation"
#endif // VOID_DEBUG
    };
    const uint32_t wanted_layer_count = sizeof(wanted_layers) / sizeof(const char *);

    uint32_t avai_layer_count = 0;
    vkEnumerateInstanceLayerProperties(&avai_layer_count, nullptr);
    VkLayerProperties *avai_layer_properties =
        arena_push_array<VkLayerProperties>(*scratch, avai_layer_count);
    vkEnumerateInstanceLayerProperties(&avai_layer_count, avai_layer_properties);

    DynamicArray<const char *> layers;
    layers.init(*scratch, wanted_layer_count);
    for (uint32_t idx = 0; idx < wanted_layer_count; ++idx)
    {
        bool avai = is_layer_available(avai_layer_properties, avai_layer_count, wanted_layers[idx]);
        if (avai)
        {
            layers.push_back(*scratch, wanted_layers[idx]);
        }
    }

    VkInstanceCreateInfo instanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .enabledLayerCount = SCAST(uint32_t, layers.get_count()),
        .ppEnabledLayerNames = layers.mut_data(),
        .enabledExtensionCount = SCAST(uint32_t, exts.get_count()),
        .ppEnabledExtensionNames = exts.mut_data(),
    };

    VpInstanceCreateInfo vpInstanceCreateInfo{
        .pCreateInfo = &instanceCreateInfo,
        .enabledFullProfileCount = 1,
        .pEnabledFullProfiles = &PROFILE,
    };

    VkResult result = vpCreateInstance(&vpInstanceCreateInfo, vk_alloc(), &context_.instance);

    if (result != VK_SUCCESS)
    {
        Logger::show_fatal_mbox("Vulkan engine", "Failed to create Vulkan instance!");
    }
    temp.end_scratch();
}

void VulkanBackend::create_physical_device()
{
    ScopedScratchTemp temp;
    Arena *scratch = t_scratches.get_scratch();
    temp.start_scratch(scratch);

    uint32_t avai_phydevice_count = 0;
    vkEnumeratePhysicalDevices(context_.instance, &avai_phydevice_count, nullptr);
    VkPhysicalDevice *avai_phydevices = arena_push_array<VkPhysicalDevice>(*scratch, avai_phydevice_count);
    vkEnumeratePhysicalDevices(context_.instance, &avai_phydevice_count, avai_phydevices);

    const char *const wanted_phydevice_exts[] = {"VK_KHR_swapchain", "VK_EXT_memory_budget"};
    const uint32_t wanted_phydevice_ext_count = sizeof(wanted_phydevice_exts) / sizeof(const char *);

    bool found_suitable_phydevice = false;
    for (uint32_t idx = 0; idx < avai_phydevice_count; ++idx)
    {
        VkBool32 phydevice_profile_supported;
        vpGetPhysicalDeviceProfileSupport(context_.instance, avai_phydevices[idx], &PROFILE,
                                          &phydevice_profile_supported);

        if (phydevice_profile_supported && profile_supported_)
        {

            uint32_t avai_phydevice_ext_count = 0;
            vkEnumerateDeviceExtensionProperties(avai_phydevices[idx], nullptr, &avai_phydevice_ext_count,
                                                 nullptr);
            if (avai_phydevice_ext_count == 0)
            {
                continue;
            }
            bool support_wanted_exts = true;
            VkExtensionProperties *avai_phydevice_exts =
                arena_push_array<VkExtensionProperties>(*scratch, avai_phydevice_ext_count);
            vkEnumerateDeviceExtensionProperties(avai_phydevices[idx], nullptr, &avai_phydevice_ext_count,
                                                 avai_phydevice_exts);

            for (uint32_t ext_idx = 0; ext_idx < wanted_phydevice_ext_count; ++ext_idx)
            {
                if (!is_ext_available(avai_phydevice_exts, avai_phydevice_ext_count,
                                      wanted_phydevice_exts[ext_idx]))
                {
                    support_wanted_exts = false;
                    break;
                }
            }

            if (!support_wanted_exts)
            {
                continue;
            }

            VkSurfaceCapabilitiesKHR surface_capabilities{};
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(avai_phydevices[idx], context_.surface,
                                                      &surface_capabilities);

            uint32_t surface_format_count = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(avai_phydevices[idx], context_.surface,
                                                 &surface_format_count, nullptr);

            VkSurfaceFormatKHR *surface_formats =
                arena_push_array<VkSurfaceFormatKHR>(*scratch, surface_format_count);
            vkGetPhysicalDeviceSurfaceFormatsKHR(avai_phydevices[idx], context_.surface,
                                                 &surface_format_count, surface_formats);

            uint32_t present_mode_count = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(avai_phydevices[idx], context_.surface,
                                                      &present_mode_count, nullptr);

            VkPresentModeKHR *present_modes =
                arena_push_array<VkPresentModeKHR>(*scratch, present_mode_count);
            vkGetPhysicalDeviceSurfacePresentModesKHR(avai_phydevices[idx], context_.surface,
                                                      &present_mode_count, present_modes);

            if (present_mode_count == 0 || surface_format_count == 0)
            {
                continue;
            }

            VkPhysicalDeviceProperties2 phydevice_props
            {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = nullptr,
            };

            vkGetPhysicalDeviceProperties2(avai_phydevices[idx], &phydevice_props);

            uint32_t queue_fam_prop_count;
            vkGetPhysicalDeviceQueueFamilyProperties2(avai_phydevices[idx], &queue_fam_prop_count, nullptr);

            VkQueueFamilyProperties2 *queue_fam_props =
                arena_push_array<VkQueueFamilyProperties2>(*scratch, queue_fam_prop_count);
            for (uint32_t qidx = 0; qidx < queue_fam_prop_count; ++qidx)
            {
                queue_fam_props[qidx].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
                queue_fam_props[qidx].pNext = nullptr;
            }

            vkGetPhysicalDeviceQueueFamilyProperties2(avai_phydevices[idx], &queue_fam_prop_count,
                                                      queue_fam_props);

            bool is_discrete_phydevice =
                phydevice_props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            bool support_vulkan1_3 = phydevice_props.properties.apiVersion >= VK_API_VERSION_1_3;

            bool support_graphics = false;
            for (uint32_t qidx = 0; qidx < queue_fam_prop_count; ++qidx)
            {
                const VkQueueFamilyProperties2 &fam_prop = queue_fam_props[qidx];
                if ((fam_prop.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0)
                {
                    support_graphics = true;
                    break;
                }
            }

            VkPhysicalDeviceVulkan11Features vulkan11_features
            {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
                .pNext = nullptr,
            };

            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extended_dynamic_state_features
            {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
                .pNext = &vulkan11_features
            };

            VkPhysicalDeviceFeatures2 features2
            {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &extended_dynamic_state_features
            };

            vkGetPhysicalDeviceFeatures2(avai_phydevices[idx], &features2);

            if (is_discrete_phydevice && support_vulkan1_3 && support_graphics &&
                vulkan11_features.shaderDrawParameters &&
                extended_dynamic_state_features.extendedDynamicState)
            {
                context_.physical_device = avai_phydevices[idx];
                found_suitable_phydevice = true;
                break;
            }
        }
        // else: this device lacks the profile; skip it only. (Setting profile_supported_ = VK_FALSE
        // here used to reject every later device too, e.g. the dGPU enumerated after an iGPU.)
        // TODO: fallback path for machines with no profile-capable device.
    }
    if (!found_suitable_phydevice)
    {
        Logger::show_fatal_mbox("Vulkan engine", "Can't find suitable physical device!");
    }
    temp.end_scratch();
}

} // namespace VoidEngine::Vulkan
