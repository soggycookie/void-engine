#pragma once
//#include "math_utils.h"
#include "pch.h"
#include "renderer_api.h"
#include "resource.h"
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
    //struct Vertex
    //{
    //    //Vec2 pos;
    //    //Vec3 color;

    //    static VkVertexInputBindingDescription GetBindingDescription()
    //    {
    //        return {
    //            .binding = 0,
    //            .stride = sizeof(Vertex),
    //            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    //        };
    //    }

    //    static std::array<VkVertexInputAttributeDescription, 2>
    //    GetAttributeDescriptions()
    //    {
    //        return {{
    //            {.location = 0,
    //             .binding = 0,
    //             .format = VK_FORMAT_R32G32_SFLOAT,
    //             .offset = offsetof(Vertex, pos)},
    //            {.location = 1,
    //             .binding = 0,
    //             .format = VK_FORMAT_R32G32B32_SFLOAT,
    //             .offset = offsetof(Vertex, color)},
    //        }};
    //    }
    //};

    //static constexpr Vertex vertices[] = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    //                                      {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    //                                      {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

    void CreateInstance();
    void CreateSurface();
    void SetUpDebugMessenger();
    void SelectPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    void CreateImageViews();
    void CreateGraphicPipeline();
    VkShaderModule CreateShaderModule(const std::vector<char> &binaryCode);
    void RecordCommandBuffer(uint32_t imageIndex);
    void CreateCommandPool();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void RecreateSwapchain();

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

    VkExtent2D
    ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities);

private:
    struct FrameSyncObjs
    {

        VkSemaphore presentCompleteSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence drawFence;
    };

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

    uint32_t m_queueFamilyIndex = ~0;
    uint32_t m_frameIndex = 0;
    static constexpr uint32_t s_kMaxFrameInFlight = 2;

    FrameSyncObjs m_frames[s_kMaxFrameInFlight];
    VkCommandBuffer m_frameCmdBuffers[s_kMaxFrameInFlight];

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

#ifdef VOID_DEBUG
    VkDebugUtilsMessengerEXT m_debugMessenger;
#endif // VOID_DEBUG
};
} // namespace VoidEngine
