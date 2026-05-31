#pragma once
#include "pch.h"
#include "renderer_api.h"
#include <cstddef>

#ifdef _WIN32
#include "win32_window.h"
#define VK_USE_PLATFORM_WIN32_KHR
#else

#endif

#include <vulkan/vulkan.h>

namespace VoidEngine
{

class Vulkan_RendererAPI : public RendererAPI
{
public:
    Vulkan_RendererAPI(Window *window) : RendererAPI(window) {}
    ~Vulkan_RendererAPI() = default;

    void Shutdown() override;
    bool Init(int width, int height, void *outputWindow) override;

    void NewFrame() override;

    void EndFrame() override;

    void *GetContext() override { return nullptr; }

    void *CreateAndSubmitBuffer(void *const data, size_t byteSize,
                                BufferType type) override;

    void DestroyBuffer(GraphicBuffer &buffer) override;

    void *CompileShader(const wchar_t *file, const char *entry,
                        const char *target) override;
    void *CreateShader(void **compiledSrc, ShaderType type) override;
    void DestroyShader(GraphicShader &shader) override;

    void Draw(MeshResource *mesh, MaterialResource *material) override;

    void DrawTest() override;

private:
    void CreateInstance();
    void CreateSurface();
    void SetUpDebugMessenger();
    void SelectPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapchain();
    void CreateImageViews();
    void CreateGraphicPipeline();
    VkShaderModule CreateShaderModule(const std::vector<char> &binaryCode);
    void RecordCommandBuffer(uint32_t imageIndex);
    void CreateCommandPool();
    void CreateCommandBuffer();
    void CreateSyncObject();

    void DrawFrame();

    void TransitionImageLayout(uint32_t imageIndex, VkImageLayout oldLayout,
                               VkImageLayout newLayout,
                               VkAccessFlags2 srcAccessMask,
                               VkAccessFlags2 dstAccessMask,
                               VkPipelineStageFlags2 srcStageMask,
                               VkPipelineStageFlags2 dstStageMask);

    VkPresentModeKHR
    ChoosePresentMode(const std::vector<VkPresentModeKHR> &modes);

    VkSurfaceFormatKHR
    ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);

    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

private:
    VkInstance m_instance;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_logicalDevice;
    VkQueue m_graphicQueue;
    VkSurfaceKHR m_surface;
    VkSwapchainKHR m_swapchain;
    VkSurfaceFormatKHR m_format;
    VkExtent2D m_extent;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_pipeline;
    VkCommandPool m_commandPool;
    VkCommandBuffer m_cmdBuffer;
    uint32_t m_queueFamilyIndex = ~0;

    VkSemaphore m_presentCompleteSemaphore;
    VkSemaphore m_renderFinishedSemaphore;
    VkFence m_drawFence;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

#ifdef VOID_DEBUG
    VkDebugUtilsMessengerEXT m_debugMessenger;
#endif // VOID_DEBUG
};
} // namespace VoidEngine
