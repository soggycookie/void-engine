#pragma once
#include "dynamic_array.h"
#include "pch.h"
#include "utils.h"
#include "window.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX // vulkan_win32.h pulls in windows.h
#endif
#define VK_USE_PLATFORM_WIN32_KHR
#else

#endif

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_profiles.hpp>

#include "vk_mem_alloc.h"

#include "platform/vulkan/vulkan_host_allocator.h"

struct ImDrawData;

// Scene rendering (pipeline, vertex/index buffers, texture, descriptors, depth, UBO) is compiled out
// while ImGui is brought up. Only instance -> device -> swapchain -> frames + the ImGui pass run.
// The code under this flag is stale (old DrawFrame/RecordCommandBuffer); merge it into draw_frame
// when turning it back on.
#ifndef VOID_VK_SCENE_PASS
#define VOID_VK_SCENE_PASS 0
#endif

namespace VoidEngine::Vulkan
{

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

constexpr VpProfileProperties PROFILE = {VP_KHR_ROADMAP_2022_NAME, VP_KHR_ROADMAP_2022_SPEC_VERSION};

struct Context
{
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surface_format;
    VkSwapchainKHR swapchain;
    VkExtent2D extent;
    VkImage *swapchain_images;
    VkImageView *swapchain_image_views;
    VkCommandPool graphic_command_pool;
    VkQueue graphic_queue;
    uint32_t swapchain_image_count;
    uint32_t swapchain_min_image_count; // minImageCount requested at swapchain creation
    uint32_t graphic_queue_family_index;
};

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 tex_coord;

    static VkVertexInputBindingDescription GetBindingDescription()
    {
        return {
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
    }

    static VkVertexInputAttributeDescription *GetAttributeDescriptions()
    {
        static VkVertexInputAttributeDescription descriptions[] = {
            {.location = 0,
             .binding = 0,
             .format = VK_FORMAT_R32G32B32_SFLOAT,
             .offset = offsetof(Vertex, pos)},

            {.location = 1,
             .binding = 0,
             .format = VK_FORMAT_R32G32_SFLOAT,
             .offset = offsetof(Vertex, tex_coord)},
        };

        return descriptions;
    }
};

// Per frame in flight. The render-finished semaphore that present waits on is per swapchain image
// (render_finished_sems_), because present has no fence and the semaphore may still be in use when
// the same frame slot comes around again.
struct SyncObjects
{
    VkSemaphore present_complete_sem;
    VkFence draw_fence;
};

constexpr uint32_t MAX_FRAME_IN_FLIGHT = 2;

class VulkanBackend
{
private:
    static constexpr Vertex vertices[] = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}},
                                          {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}},
                                          {{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}},
                                          {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}},

                                          {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}},
                                          {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}},
                                          {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}},
                                          {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}}

    };

    static constexpr uint16_t indices[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

private:
    Window *window_;
    Vallocator vallocator_;
    // CPU-side allocator for the driver and VMA bookkeeping (thread-safe) plus GPU memory stats.
    // Declared before vma_allocator_ and must outlive it, the device and the instance.
    HostAllocator host_alloc_;
    VmaAllocator vma_allocator_ = VK_NULL_HANDLE;

    Context context_{};
    VkSemaphore *render_finished_sems_ = nullptr; // one per swapchain image
    uint32_t render_finished_sem_count_ = 0;
    bool imgui_initialized_ = false;
    VkSampler m_sampler;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_pipeline;

    VkDescriptorSetLayout descriptor_set_layout_;
    VkDescriptorPool m_descriptorPool;

    uint32_t m_frameIndex = 0;

    SyncObjects sync_objs[MAX_FRAME_IN_FLIGHT];
    VkCommandBuffer m_graphicCmdBuffers[MAX_FRAME_IN_FLIGHT];

    // std::vector<VulkanBuffer> m_uniformBuffers;
    std::vector<void *> m_mappedUniformBuffers;
    DynamicArray<VkDescriptorSet> descriptor_sets_;

    VkBool32 profile_supported_;
#ifdef VOID_DEBUG
    VkDebugUtilsMessengerEXT debug_messenger_;
#endif // VOID_DEBUG
public:
    // Public so Application can construct<VulkanBackend>(vallocator). Use startup()/shutdown().
    DEFAULT_CTOR(VulkanBackend);

    void DrawTest();

private:
    friend class Application;

    void shutdown();
    bool startup(Window *window);

    // ImGui renderer half (vulkan_imgui.cpp). init after startup() and ImGuiHost::init;
    // shutdown before ImGuiHost::destroy and shutdown().
    bool init_imgui();
    void shutdown_imgui();
    void imgui_new_frame();
    void record_imgui(VkCommandBuffer cmd, ImDrawData *draw_data);

    // Clear the swapchain image and draw the UI on top. Skips the frame while minimized.
    void draw_frame(ImDrawData *ui);

    void create_present_semaphores();
    void destroy_present_semaphores();
    void destroy_swapchain_views_and_images();

    void create_instance();
    void create_physical_device();

    void create_surface();
    void setup_debug_messenger();
    void create_logical_device();
    void create_vma_allocator();

    // Pass this as pAllocator to every vkCreate*/vkDestroy* call. nullptr when the host allocator is off.
    const VkAllocationCallbacks *vk_alloc() const
    {
        return host_alloc_.get_callbacks();
    }

    void create_swapchain(VkSwapchainKHR old_swapchain = VK_NULL_HANDLE);
    void create_swapchain_image_views();
    void create_graphic_pipeline();
    VkShaderModule CreateShaderModule(const std::vector<char> &binaryCode);
    void RecordCommandBuffer(uint32_t imageIndex);
    void create_command_pool();

    void CreateVertexBuffer();
    void CreateIndexBuffer();

    void create_descriptor_pool();
    void create_descriptor_set_layout();
    void create_descriptor_sets();
    void create_uniform_buffer();
    void UpdateUniformBuffer(uint32_t frameIndex);

    void create_command_buffers();
    void create_sync_objects();
    void recreate_swapchain();

    void create_tex_image();
    void CreateTextureImageView();
    void create_tex_sampler();

    void create_depth_tex();
    void CleanUpDepthTexture();

    VkImageView CreateImageView(const VkImage &image, VkFormat format, VkImageAspectFlags imageFlags);

    void DrawFrame();

    void TransitionSwapchainImageLayout(uint32_t imageIndex, VkImageLayout oldLayout,
                                        VkImageLayout newLayout, VkAccessFlags2 srcAccessMask,
                                        VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask,
                                        VkPipelineStageFlags2 dstStageMask);

    void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout,
                               VkImageLayout newLayout);

    void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkPipelineStageFlags2 srcStage,
                               VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess,
                               VkAccessFlags2 dstAccess, VkImageLayout oldLayout,
                               VkImageLayout newLayout, uint32_t srcQueueFamilyIndex,
                               uint32_t dstQueueFamilyIndex, VkImageAspectFlags imageAspect);

    void CopyBuffer(VkBuffer dst, VkBuffer src, VkDeviceSize size);

    void CopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer src, VkImage image, uint32_t width,
                           uint32_t height);

    VkPresentModeKHR choose_present_mode(VkPresentModeKHR *modes, uint32_t mode_count);

    VkSurfaceFormatKHR choose_surface_format(VkSurfaceFormatKHR *formats, uint32_t format_count);

    VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR &capabilities);

    // VulkanBuffer CreateBuffer(VulkanAllocationInfo &allocationInfo, VkBufferCreateInfo *bufferCreateInfo,
    //                           VulkanMemoryUsage usage, VulkanMemoryFlag flags);
    //
    // void DestroyBuffer(VulkanBuffer buffer);
    //
    // VulkanImage CreateImage(VkImageCreateInfo *imageCreateInfo, VulkanMemoryUsage usage,
    //                         VulkanMemoryFlag flags);
    //
    // void DestroyImage(VulkanImage image);
    //
    // void *MapMemory(VulkanBuffer &buffer);
    // void UnmapMemory(VulkanBuffer &buffer);

    // VlkBufferArena VlkCreateBufferArena(VlkAllocator &allocator,
    //                                     VkBufferCreateInfo &bufferCreateInfo, uint32_t tag);
    //
    // void VlkDestroyBufferArena(VlkAllocator &allocator, uint32_t tag);
};
} // namespace VoidEngine::Vulkan
