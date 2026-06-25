#pragma once

#include "math_utils.h"
#include "pch.h"
#include "renderer_api.h"
#include "resource.h"
#include <vector>
#ifdef _WIN32
#include "win32_window.h"
#define VK_USE_PLATFORM_WIN32_KHR
#else

#endif

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_profiles.hpp>

struct UniformBufferObject
{
    Mat4 model;
    Mat4 view;
    Mat4 proj;
};

namespace VoidEngine
{

constexpr VpProfileProperties profile = {VP_KHR_ROADMAP_2022_NAME,
                                         VP_KHR_ROADMAP_2022_SPEC_VERSION};

struct RendererContext
{
};

struct VulkanRendererContext : public RendererContext
{
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkSwapchainKHR swapchain;
    VkExtent2D extent;
    VkImage *pSwapchainImages;
    VkImageView *pSwapchainImageViews;
    VkCommandPool graphicCommandPool;
    VkPhysicalDeviceProperties2 physicalDeviceProperties;
    VkQueue graphicQueue;
    uint32_t swapchainImageCount;
    uint32_t graphicQueueFamilyIndex = ~0;
};

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

    void *CreateAndSubmitBuffer(void *const data, size_t byteSize, BufferType type) override;

    void DestroyBuffer(GraphicBuffer &buffer) override;

    void *CompileShader(const wchar_t *file, const char *entry, const char *target) override;
    void *CreateShader(void **compiledSrc, ShaderType type) override;
    void DestroyShader(GraphicShader &shader) override;

    // void Draw(MeshResource *mesh, MaterialResource *material) override;

    void DrawTest() override;

    void CreateInstance();
    void CreatePhysicalDevice();

private:
    struct Vertex
    {
        Vec3 pos;
        Vec2 texCoord;

        static VkVertexInputBindingDescription GetBindingDescription()
        {
            return {
                .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            };
        }

        static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
        {
            return {{
                {.location = 0,
                 .binding = 0,
                 .format = VK_FORMAT_R32G32B32_SFLOAT,
                 .offset = offsetof(Vertex, pos)},
                {.location = 1,
                 .binding = 0,
                 .format = VK_FORMAT_R32G32_SFLOAT,
                 .offset = offsetof(Vertex, texCoord)},
            }};
        }
    };

    struct Buffer
    {
        VkBuffer buffer;
        VkDeviceMemory memory;
    };

    struct Texture
    {
        VkImage image;
        VkImageView view;
        VkDeviceMemory memory;
    };

    Texture m_depthTex;

    VkSampler m_sampler;

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

    // void CreateInstance();
    void CreateSurface();
    void SetUpDebugMessenger();
    void SelectPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    void CreateSwapchainImageViews();
    void CreateGraphicPipeline();
    VkShaderModule CreateShaderModule(const std::vector<char> &binaryCode);
    void RecordCommandBuffer(uint32_t imageIndex);
    void CreateCommandPool();

    void CreateVertexBuffer();
    void CreateIndexBuffer();

    void CreateDescriptorPool();
    void CreateDescriptorSetLayout();
    void CreateDescriptorSets();
    void CreateUniformBuffer();
    void UpdateUniformBuffer(uint32_t frameIndex);

    void CreateCommandBuffers();
    void CreateSyncObjects();
    void RecreateSwapchain();

    void CreateTextureImage();
    void CreateTextureImageView();
    void CreateTextureSampler();

    void CreateDepthTexture();
    void CleanUpDepthTexture();

    VkImageView CreateImageView(const VkImage &image, VkFormat format,
                                VkImageAspectFlags imageFlags);

    void DrawFrame();

    void TransitionSwapchainImageLayout(uint32_t imageIndex, VkImageLayout oldLayout,
                                        VkImageLayout newLayout, VkAccessFlags2 srcAccessMask,
                                        VkAccessFlags2 dstAccessMask,
                                        VkPipelineStageFlags2 srcStageMask,
                                        VkPipelineStageFlags2 dstStageMask);

    void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout oldLayout,
                               VkImageLayout newLayout);

    void TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
                               VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage,
                               VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex,
                               VkImageAspectFlags imageAspect);

    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    Buffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlag, VkSharingMode sharingMode,
                        VkMemoryPropertyFlags propertyFlag, uint32_t queueFamilyCount = 0,
                        uint32_t *queueFamilyIndices = nullptr);

    Texture CreateImage(uint32_t texWidth, uint32_t texHeight, VkFormat format, VkImageTiling tilt,
                        VkImageUsageFlags usage, VkMemoryPropertyFlags properties);

    void CopyBuffer(Buffer dst, Buffer src, VkDeviceSize size);

    void CopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer src, VkImage image, uint32_t width,
                           uint32_t height);

    VkPresentModeKHR ChoosePresentMode(VkPresentModeKHR *modes, uint32_t modeCount);

    VkSurfaceFormatKHR ChooseSurfaceFormat(VkSurfaceFormatKHR *formats, uint32_t formatCount);

    VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities);

private:
    struct FrameSyncObjs
    {

        VkSemaphore presentCompleteSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence drawFence;
    };

    VulkanRendererContext m_context;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_pipeline;

    VkBuffer m_vertexBuffer;
    VkDeviceMemory m_vertexBufferMemory;
    VkBuffer m_indexBuffer;
    VkDeviceMemory m_indexBufferMemory;

    VkDescriptorSetLayout m_descriptorSetLayout;
    VkDescriptorPool m_descriptorPool;

    uint32_t m_frameIndex = 0;
    static constexpr uint32_t s_kMaxFrameInFlight = 2;

    FrameSyncObjs m_frames[s_kMaxFrameInFlight];
    VkCommandBuffer m_graphicCmdBuffers[s_kMaxFrameInFlight];

    Texture m_texture;

    std::vector<VkBuffer> m_uniformBuffers;
    std::vector<VkDeviceMemory> m_uniformBufferMemories;
    std::vector<void *> m_mappedUniformBuffers;
    std::vector<VkDescriptorSet> m_descriptorSets;

    VkBool32 m_minProfileSupported;

#ifdef VOID_DEBUG
    VkDebugUtilsMessengerEXT m_debugMessenger;
#endif // VOID_DEBUG
};
} // namespace VoidEngine
