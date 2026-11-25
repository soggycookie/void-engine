#include "vulkan_backend.h"
#include "logger/log.h"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

namespace VoidEngine::Vulkan
{
namespace
{
void imgui_check_vk_result(VkResult result)
{
    // Log only: ENGINE_ASSERT would abort the process.
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("IMGUI", "imgui_impl_vulkan VkResult = %d", SCAST(int, result));
    }
}
} // namespace

bool VulkanBackend::init_imgui()
{
    ENGINE_ASSERT(!imgui_initialized_, "ImGui Vulkan backend is already initialized!");
    ENGINE_ASSERT(ImGui::GetCurrentContext(), "ImGuiHost::init must run before init_imgui!");

    // Must stay valid until ImGui_ImplVulkan_Init returns (the backend copies the struct).
    VkFormat color_format = context_.surface_format.format;

    ImGui_ImplVulkan_InitInfo info = {};
    info.ApiVersion = VK_API_VERSION_1_3;
    info.Instance = context_.instance;
    info.PhysicalDevice = context_.physical_device;
    info.Device = context_.device;
    info.QueueFamily = context_.graphic_queue_family_index;
    info.Queue = context_.graphic_queue;
    // The backend owns its descriptor pool (font atlas + user textures).
    info.DescriptorPoolSize = 64;
    info.MinImageCount = std::max(2u, context_.swapchain_min_image_count);
    info.ImageCount = std::max(info.MinImageCount, context_.swapchain_image_count);
    info.PipelineCache = VK_NULL_HANDLE;

    info.UseDynamicRendering = true;
    info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    info.PipelineInfoMain.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
        .stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
    };

    // Same host callbacks as every other Vulkan object, so HostAllocator::destroy's leak check covers ImGui.
    info.Allocator = vk_alloc();
    info.CheckVkResultFn = &imgui_check_vk_result;

    if (!ImGui_ImplVulkan_Init(&info))
    {
        LOG_ERROR("IMGUI", "ImGui_ImplVulkan_Init failed");
        return false;
    }
    imgui_initialized_ = true;
    return true;
}

void VulkanBackend::shutdown_imgui()
{
    if (!imgui_initialized_)
    {
        return;
    }
    // The UI's vertex/index buffers and font texture may still be in use by a frame in flight.
    vkDeviceWaitIdle(context_.device);
    ImGui_ImplVulkan_Shutdown();
    imgui_initialized_ = false;
}

void VulkanBackend::imgui_new_frame()
{
    if (imgui_initialized_)
    {
        ImGui_ImplVulkan_NewFrame();
    }
}

void VulkanBackend::record_imgui(VkCommandBuffer cmd, ImDrawData *draw_data)
{
    if (!imgui_initialized_ || !draw_data)
    {
        return;
    }
    // Font/texture uploads happen inside this call and submit to graphic_queue (main thread only).
    ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
}
} // namespace VoidEngine::Vulkan
