#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "application.h"
#include "log.h"
#include "math_utils.h"
#include "renderer.h"
#include "vulkan_renderer_api.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

// CURRENTLY ONLY ALLOW VULKAN API > 1.3

namespace VoidEngine
{

bool Vulkan_RendererAPI::Init(int width, int height, void *outputWindow)
{
    CreateInstance();

#ifdef VOID_DEBUG
    SetUpDebugMessenger();
#endif // VOID_DEBUG

    CreateSurface();

    CreatePhysicalDevice();
    // SelectPhysicalDevice();
    CreateLogicalDevice();

    CreateSwapchain();
    CreateSwapchainImageViews();

    CreateSyncObjects();
    CreateCommandPool();
    CreateCommandBuffers();

    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();

    CreateDescriptorSetLayout();
    CreateUniformBuffer();
    CreateDescriptorPool();
    CreateDescriptorSets();

    CreateGraphicPipeline();
    CreateDepthTexture();

    CreateVertexBuffer();
    CreateIndexBuffer();

    return true;
}

void Vulkan_RendererAPI::NewFrame() {}

void Vulkan_RendererAPI::EndFrame() {}

void *Vulkan_RendererAPI::CreateAndSubmitBuffer(void *const data, size_t byteSize, BufferType type)
{
    return nullptr;
}

void Vulkan_RendererAPI::DestroyBuffer(GraphicBuffer &buffer) {}

void *Vulkan_RendererAPI::CompileShader(const wchar_t *file, const char *entry, const char *target)
{
    return nullptr;
}
void *Vulkan_RendererAPI::CreateShader(void **compiledSrc, ShaderType type) { return nullptr; }
void Vulkan_RendererAPI::DestroyShader(GraphicShader &shader) {}

// void Vulkan_RendererAPI::Draw(MeshResource *mesh, MaterialResource *material) {}

void Vulkan_RendererAPI::Shutdown()
{
    vkDestroySampler(m_context.device, m_sampler, nullptr);
    vkDestroyImageView(m_context.device, m_texture.view, nullptr);
    vkDestroyImage(m_context.device, m_texture.image, nullptr);
    vkFreeMemory(m_context.device, m_texture.memory, nullptr);

    for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
    {
        vkDestroyBuffer(m_context.device, m_uniformBuffers[idx], nullptr);
        vkFreeMemory(m_context.device, m_uniformBufferMemories[idx], nullptr);
        FLUSH_LOG();
    }

    vkDestroyDescriptorPool(m_context.device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_context.device, m_descriptorSetLayout, nullptr);

    vkDestroyBuffer(m_context.device, m_vertexBuffer, nullptr);
    vkFreeMemory(m_context.device, m_vertexBufferMemory, nullptr);
    vkDestroyBuffer(m_context.device, m_indexBuffer, nullptr);
    vkFreeMemory(m_context.device, m_indexBufferMemory, nullptr);

    CleanUpDepthTexture();

    for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
    {
        vkDestroySemaphore(m_context.device, m_frames[idx].presentCompleteSemaphore, nullptr);
        vkDestroySemaphore(m_context.device, m_frames[idx].renderFinishedSemaphore, nullptr);
        vkDestroyFence(m_context.device, m_frames[idx].drawFence, nullptr);
    }
    vkDestroyCommandPool(m_context.device, m_context.graphicCommandPool, nullptr);

    vkDestroyPipeline(m_context.device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_context.device, m_pipelineLayout, nullptr);

    for (size_t idx = 0; idx < m_context.swapchainImageCount; ++idx)
    {
        vkDestroyImageView(m_context.device, m_context.pSwapchainImageViews[idx], nullptr);
    }
    vkDestroySwapchainKHR(m_context.device, m_context.swapchain, nullptr);

    std::free(m_context.pSwapchainImageViews);
    std::free(m_context.pSwapchainImages);

    vkDestroySurfaceKHR(m_context.instance, m_context.surface, nullptr);
    vkDestroyDevice(m_context.device, nullptr);

    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_context.instance, "vkDestroyDebugUtilsMessengerEXT");

    if (vkDestroyDebugUtilsMessengerEXT)
    {
        vkDestroyDebugUtilsMessengerEXT(m_context.instance, m_debugMessenger, nullptr);
    }
    else
    {
        LOG_ERROR("VULKAN", "Vulkan failed to acquire debug messenger destruction");

        throw std::runtime_error("[VULKAN] Failed to acquire vkDestroyDebugUtilsMessengerEXT!");
    }

    vkDestroyInstance(m_context.instance, nullptr);
}

static bool IsExtAvailable(const std::vector<VkExtensionProperties> &extProperties,
                           const char *checkingExt)
{
    for (const VkExtensionProperties &ext : extProperties)
    {
        if (strcmp(checkingExt, ext.extensionName))
        {
            return true;
        }
    }

    return false;
}

static bool IsLayerAvailable(const std::vector<VkLayerProperties> &layerProperties,
                             const char *checkinglayer)
{
    for (const VkLayerProperties &layer : layerProperties)
    {
        if (strcmp(checkinglayer, layer.layerName))
        {
            return true;
        }
    }

    return false;
}

// void Vulkan_RendererAPI::CreateInstance()
// {
//     uint32_t apiVersion = 0;
//
//     auto r = vkEnumerateInstanceVersion(&apiVersion);
//
//     if (r != VK_SUCCESS)
//     {
//         // LOG
//         LOG_ERROR("VULKAN", "Vulkan failed to enumerate Vulkan api version");
//         throw std::runtime_error("Failed to enumerate Vulkan api version!");
//     }
//
//     const VkApplicationInfo appInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
//                                     .pNext = nullptr,
//                                     .pApplicationName = "No App",
//                                     .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
//                                     .pEngineName = "Void Engine",
//                                     .engineVersion = VK_MAKE_VERSION(0, 0, 1),
//                                     .apiVersion = apiVersion};
//
//     uint32_t extCount = 0;
//
//     vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
//     std::vector<VkExtensionProperties> extProperties(extCount);
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
//         .pApplicationInfo = &appInfo,
//         .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
//         .ppEnabledLayerNames = requiredLayers.data(),
//         .enabledExtensionCount = static_cast<uint32_t>(requiredExts.size()),
//         .ppEnabledExtensionNames = requiredExts.data(),
//     };
//
//     if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_context.instance) != VK_SUCCESS)
//     {
//         LOG_ERROR("VULKAN", "Failed to create Vulkan instance!");
//
//         throw std::runtime_error("[VULKAN] Failed to create Vulkan instance!");
//     }
// }

static VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageTypes,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
    switch (messageSeverity)
    {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
    {
        LOG_TRACE("VULKAN", "Trace: %s", pCallbackData->pMessage);
        break;
    }
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    {
        LOG_INFO("VULKAN", "Info: %s", pCallbackData->pMessage);
        break;
    }
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    {
        LOG_WARN("VULKAN", "Warn: %s", pCallbackData->pMessage);
        break;
    }
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    {
        LOG_ERROR("VULKAN", "Error: %s", pCallbackData->pMessage);
        break;
    }
    default:
    {
        LOG_DEBUG("VULKAN", "Unknown: %s", pCallbackData->pMessage);
        break;
    }
    }

    return VK_FALSE;
}

void Vulkan_RendererAPI::CreateSurface()
{
#ifdef _WIN32
    Win32_Window *window = dynamic_cast<Win32_Window *>(m_window);

    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .hinstance = window->GetModuleInstanceHandle(),
        .hwnd = window->GetNativeWindowHandle(),
    };

    if (vkCreateWin32SurfaceKHR(m_context.instance, &surfaceCreateInfo, nullptr,
                                &m_context.surface) != VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "Failed to create Win32 surface!");
        throw std::runtime_error("[VULKAN] Failed to create Win32 surface!");
    }

#else
    LOG_ERROR("VULKAN", "Failed to detect and create platform-dependent surface!");
    throw std::runtime_error("[VULKAN] Failed to detect and create platform-dependent surface!");
#endif
}

void Vulkan_RendererAPI::SetUpDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = &DebugCallback,
    };

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_context.instance,
                                                                  "vkCreateDebugUtilsMessengerEXT");

    if (vkCreateDebugUtilsMessengerEXT)
    {
        if (vkCreateDebugUtilsMessengerEXT(m_context.instance, &debugMessengerCreateInfo, nullptr,
                                           &m_debugMessenger) != VK_SUCCESS)
        {
            LOG_ERROR("VULKAN", "Vulkan debug messenger creation failed!");

            throw std::runtime_error("[VULKAN] Failed to create debug messenger!");
        }
    }
    else
    {
        LOG_ERROR("VULKAN", "Failed to acquire vkCreateDebugUtilsMessengerEXT");

        throw std::runtime_error("[VULKAN] Failed to acquire vkCreateDebugUtilsMessengerEXT!");
    }
}

void Vulkan_RendererAPI::SelectPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_context.instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_context.instance, &deviceCount, devices.data());

    if (deviceCount == 0)
    {
        LOG_ERROR("VULKAN", "No physical devices found");

        throw std::runtime_error("[VULKAN] No physical devices found!");
    }

    const std::vector<const char *> requiredDeviceExtensions = {"VK_KHR_swapchain"};

    for (size_t idx = 0; idx < devices.size(); ++idx)
    {
        const VkPhysicalDevice &device = devices[idx];

        uint32_t deviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &deviceExtensionCount, nullptr);

        bool supportsRequiredExtensions = false;
        if (deviceExtensionCount != 0)
        {
            std::vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &deviceExtensionCount,
                                                 deviceExtensions.data());

            supportsRequiredExtensions = std::ranges::all_of(
                requiredDeviceExtensions,
                [&](const char *ext)
                {
                    return std::ranges::any_of(deviceExtensions, [&](const VkExtensionProperties &p)
                                               { return std::strcmp(ext, p.extensionName) == 0; });
                });
        }

        bool supportSwapchain = false;

        if (supportsRequiredExtensions)
        {
            VkSurfaceCapabilitiesKHR surfaceCapabilities{};
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_context.surface,
                                                      &surfaceCapabilities);

            uint32_t surfaceFormatCount = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_context.surface, &surfaceFormatCount,
                                                 nullptr);

            if (surfaceFormatCount != 0)
            {
                std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_context.surface, &surfaceFormatCount,
                                                     surfaceFormats.data());
            }

            uint32_t presentModeCount = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_context.surface, &presentModeCount,
                                                      nullptr);

            if (presentModeCount != 0)
            {
                std::vector<VkPresentModeKHR> presentModes(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_context.surface,
                                                          &presentModeCount, presentModes.data());
            }

            if (presentModeCount != 0 && surfaceFormatCount != 0)
            {
                supportSwapchain = true;
            }
        }

        m_context.physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        m_context.physicalDeviceProperties.pNext = nullptr;

        vkGetPhysicalDeviceProperties2(device, &m_context.physicalDeviceProperties);

        uint32_t queueFamilyPropCount;
        vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyPropCount, nullptr);

        std::vector<VkQueueFamilyProperties2> queueFamilyProps(
            queueFamilyPropCount,
            {.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, .pNext = nullptr});

        vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyPropCount,
                                                  queueFamilyProps.data());

        bool isDiscreteDevice = m_context.physicalDeviceProperties.properties.deviceType ==
                                VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        bool supportVulkan1_4 =
            m_context.physicalDeviceProperties.properties.apiVersion >= VK_API_VERSION_1_4;

        bool supportGraphics = std::ranges::any_of(
            queueFamilyProps, [](const VkQueueFamilyProperties2 &p)
            { return (p.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0; });

        bool supportTransfer = std::ranges::any_of(
            queueFamilyProps, [](const VkQueueFamilyProperties2 &p)
            { return (p.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) > 0; });

        VkPhysicalDeviceVulkan11Features vulkan11features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .pNext = nullptr,
        };

        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
            .pNext = &vulkan11features};

        VkPhysicalDeviceVulkan13Features vulkan13Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &extendedDynamicStateFeatures};

        VkPhysicalDeviceFeatures2 features2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                            .pNext = &vulkan13Features};

        vkGetPhysicalDeviceFeatures2(device, &features2);

        bool supportsRequiredFeatures =
            vulkan13Features.dynamicRendering && vulkan13Features.synchronization2 &&
            extendedDynamicStateFeatures.extendedDynamicState &&
            vulkan11features.shaderDrawParameters && features2.features.samplerAnisotropy;

        if (isDiscreteDevice && supportVulkan1_4 && supportGraphics && supportTransfer &&
            supportsRequiredExtensions && supportsRequiredFeatures && supportSwapchain)
        {
            m_context.physicalDevice = device;
            break;
        }
    }

    if (m_context.physicalDevice == VK_NULL_HANDLE)
    {
        LOG_ERROR("VULKAN", "No suitable physical device found!");

        throw std::runtime_error("[VULKAN] No suitable physical device found!");
    }
}

void Vulkan_RendererAPI::CreateLogicalDevice()
{
    uint32_t queueFamilyPropCount = 0;

    vkGetPhysicalDeviceQueueFamilyProperties2(m_context.physicalDevice, &queueFamilyPropCount,
                                              nullptr);
    std::array<VkQueueFamilyProperties2, 16> queueFamilyProps;
    LOG_ASSERT(queueFamilyPropCount <= 16, "queueFamilyPropCount is bigger than 16");

    for (auto &prop : queueFamilyProps)
    {
        prop.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
        prop.pNext = nullptr;
    }

    vkGetPhysicalDeviceQueueFamilyProperties2(m_context.physicalDevice, &queueFamilyPropCount,
                                              queueFamilyProps.data());

    for (size_t idx = 0; idx < queueFamilyProps.size(); ++idx)
    {
        if (m_context.graphicQueueFamilyIndex != ~0)
            break;

        if (m_context.graphicQueueFamilyIndex == ~0 && // only look if not found yet
            (queueFamilyProps[idx].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0)
        {
            VkBool32 supportSurface = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_context.physicalDevice, idx, m_context.surface,
                                                 &supportSurface);
            if (supportSurface)
            {
                m_context.graphicQueueFamilyIndex = idx;
            }
        }
    }

    if (m_context.graphicQueueFamilyIndex == ~0)
    {
        LOG_ERROR("VULKAN", "Could not find a queue for graphics and present -> terminating");
        throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
    }

    float queuePriority = 0.5f;
    VkDeviceQueueCreateInfo graphicDeviceQueueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .queueFamilyIndex = m_context.graphicQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    uint32_t deviceQueueCreateInfoCount = 1;
    VkDeviceQueueCreateInfo deviceQueueCreateInfos[] = {graphicDeviceQueueCreateInfo};

    VkPhysicalDeviceVulkan11Features vulkan11features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = nullptr,
        .shaderDrawParameters = VK_TRUE,
    };

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = &vulkan11features,
        .extendedDynamicState = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 features2{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                        .pNext = &extendedDynamicStateFeatures};

    std::array<const char *, 1> requiredDeviceExtension = {"VK_KHR_swapchain"};

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .flags = 0,
        .queueCreateInfoCount = deviceQueueCreateInfoCount,
        .pQueueCreateInfos = deviceQueueCreateInfos,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
        .ppEnabledExtensionNames = requiredDeviceExtension.data(),
        .pEnabledFeatures = nullptr,
    };

    VpDeviceCreateInfo vpDeviceCreateInfo{
        .pCreateInfo = &deviceCreateInfo,
        .enabledFullProfileCount = 1,
        .pEnabledFullProfiles = &profile,
    };

    VkResult result =
        vpCreateDevice(m_context.physicalDevice, &vpDeviceCreateInfo, nullptr, &m_context.device);

    if (result != VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "Failed to create logical device!");

        throw std::runtime_error("[VULKAN] Failed to create logical device!");
    }

    vkGetDeviceQueue(m_context.device, m_context.graphicQueueFamilyIndex, 0,
                     &m_context.graphicQueue);

    LOG_ASSERT(m_context.graphicQueue != VK_NULL_HANDLE, "Graphic Queue is null!");
}

VkSurfaceFormatKHR Vulkan_RendererAPI::ChooseSurfaceFormat(VkSurfaceFormatKHR *formats,
                                                           uint32_t formatCount)
{
    for (size_t idx = 0; idx < formatCount; ++idx)
    {
        const VkSurfaceFormatKHR &format = formats[idx];
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    LOG_WARN("[VULKAN]", "Surface does not have ideal format!");

    return formats[0];
}

VkPresentModeKHR Vulkan_RendererAPI::ChoosePresentMode(VkPresentModeKHR *modes, uint32_t modeCount)
{
    for (size_t idx = 0; idx < modeCount; ++idx)
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

VkExtent2D Vulkan_RendererAPI::ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR &capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }

    uint32_t width = m_window->GetFramebufferSize().width;
    uint32_t height = m_window->GetFramebufferSize().height;

    VkExtent2D extent = {
        .width = width,
        .height = height,
    };

    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);

    return extent;
}

void Vulkan_RendererAPI::CreateSwapchain(VkSwapchainKHR oldSwapchain)
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_context.physicalDevice, m_context.surface,
                                              &surfaceCapabilities);

    uint32_t surfaceFormatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.physicalDevice, m_context.surface,
                                         &surfaceFormatCount, nullptr);

    std::array<VkSurfaceFormatKHR, 16> surfaceFormats;
    LOG_ASSERT(surfaceFormatCount <= 16, "surfaceFormatCount: %u", surfaceFormatCount);

    vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.physicalDevice, m_context.surface,
                                         &surfaceFormatCount, surfaceFormats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_context.physicalDevice, m_context.surface,
                                              &presentModeCount, nullptr);

    std::array<VkPresentModeKHR, 16> presentModes;
    LOG_ASSERT(presentModeCount <= 16, "presentModeCount: %u", presentModeCount);

    vkGetPhysicalDeviceSurfacePresentModesKHR(m_context.physicalDevice, m_context.surface,
                                              &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR format = ChooseSurfaceFormat(surfaceFormats.data(), surfaceFormatCount);
    VkPresentModeKHR mode = ChoosePresentMode(presentModes.data(), presentModeCount);
    VkExtent2D extent2d = ChooseSwapchainExtent(surfaceCapabilities);

    m_context.surfaceFormat = format;
    m_context.extent = extent2d;

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
    {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = m_context.surface,
        .minImageCount = imageCount,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent2d,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = mode,
        .clipped = VK_TRUE,
        .oldSwapchain = oldSwapchain,
    };

    if (vkCreateSwapchainKHR(m_context.device, &swapchainCreateInfo, nullptr,
                             &m_context.swapchain) != VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "Failed to create swapchain!");
        throw std::runtime_error("[VULKAN] Failed to create swapchain!");
    }

    vkGetSwapchainImagesKHR(m_context.device, m_context.swapchain, &m_context.swapchainImageCount,
                            nullptr);

    if (m_context.swapchainImageCount == 0)
    {
        LOG_ERROR("VULKAN", "Swapchain image count is 0!");
        throw std::runtime_error("[VULKAN] Swapchain image count is 0!");
    }

    m_context.pSwapchainImages =
        static_cast<VkImage *>(std::malloc(sizeof(VkImage) * m_context.swapchainImageCount));
    vkGetSwapchainImagesKHR(m_context.device, m_context.swapchain, &m_context.swapchainImageCount,
                            m_context.pSwapchainImages);
}

void Vulkan_RendererAPI::CreateSwapchainImageViews()
{
    m_context.pSwapchainImageViews = static_cast<VkImageView *>(
        std::malloc(sizeof(VkImageView) * m_context.swapchainImageCount));
    for (size_t idx = 0; idx < m_context.swapchainImageCount; ++idx)
    {
        m_context.pSwapchainImageViews[idx] =
            CreateImageView(m_context.pSwapchainImages[idx], m_context.surfaceFormat.format,
                            VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

static std::vector<char> ReadFile(const char *fileName)
{
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        LOG_ERROR("RESOURCE", "Failed to open file %s", fileName);
    }

    std::vector<char> buffer(file.tellg());

    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

    file.close();

    return buffer;
}

void Vulkan_RendererAPI::CreateGraphicPipeline()
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
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data(),
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
        .width = static_cast<float>(m_context.extent.width),
        .height = static_cast<float>(m_context.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,

    };

    VkRect2D scissor{
        .offset = VkOffset2D{0, 0},
        .extent = m_context.extent,
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
        .pSetLayouts = &m_descriptorSetLayout,
        .pushConstantRangeCount = 0,
    };

    vkCreatePipelineLayout(m_context.device, &pipelineLayout, nullptr, &m_pipelineLayout);

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
        .pColorAttachmentFormats = &m_context.surfaceFormat.format,
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

    vkCreateGraphicsPipelines(m_context.device, VK_NULL_HANDLE, 1, &graphicPipeline, nullptr,
                              &m_pipeline);

    vkDestroyShaderModule(m_context.device, module, nullptr);
}

VkShaderModule Vulkan_RendererAPI::CreateShaderModule(const std::vector<char> &binaryCode)
{
    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = binaryCode.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(binaryCode.data()),
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;

    vkCreateShaderModule(m_context.device, &createInfo, nullptr, &shaderModule);

    return shaderModule;
}

void Vulkan_RendererAPI::CreateCommandPool()
{
    VkCommandPoolCreateInfo graphicCmdPool{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_context.graphicQueueFamilyIndex,
    };

    vkCreateCommandPool(m_context.device, &graphicCmdPool, nullptr, &m_context.graphicCommandPool);
}

void Vulkan_RendererAPI::CreateVertexBuffer()
{
    VkDeviceSize size = sizeof(vertices);

    Buffer stageBuffer =
        CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE,
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void *stageData = nullptr;

    if (vkMapMemory(m_context.device, stageBuffer.memory, 0, size, 0, &stageData) != VK_SUCCESS)
    {
        // TODO: log
    }
    std::memcpy(stageData, vertices, size);
    vkUnmapMemory(m_context.device, stageBuffer.memory);

    Buffer vertexBuffer =
        CreateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_vertexBuffer = vertexBuffer.buffer;
    m_vertexBufferMemory = vertexBuffer.memory;

    CopyBuffer(vertexBuffer, stageBuffer, size);

    vkDestroyBuffer(m_context.device, stageBuffer.buffer, nullptr);
    vkFreeMemory(m_context.device, stageBuffer.memory, nullptr);
}

void Vulkan_RendererAPI::CreateIndexBuffer()
{
    VkDeviceSize size = sizeof(indices);

    Buffer stageBuffer =
        CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE,
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void *stageData = nullptr;

    if (vkMapMemory(m_context.device, stageBuffer.memory, 0, size, 0, &stageData) != VK_SUCCESS)
    {
        // TODO: log
    }
    std::memcpy(stageData, indices, size);
    vkUnmapMemory(m_context.device, stageBuffer.memory);

    Buffer indexBuffer =
        CreateBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_indexBuffer = indexBuffer.buffer;
    m_indexBufferMemory = indexBuffer.memory;

    CopyBuffer(indexBuffer, stageBuffer, size);

    vkDestroyBuffer(m_context.device, stageBuffer.buffer, nullptr);
    vkFreeMemory(m_context.device, stageBuffer.memory, nullptr);
}

void Vulkan_RendererAPI::CreateDescriptorPool()
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

    vkCreateDescriptorPool(m_context.device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool);
}

void Vulkan_RendererAPI::CreateDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings{{
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
    }};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = descriptorSetLayoutBindings.size(),
        .pBindings = descriptorSetLayoutBindings.data(),
    };

    vkCreateDescriptorSetLayout(m_context.device, &descriptorSetLayoutCreateInfo, nullptr,
                                &m_descriptorSetLayout);
}

void Vulkan_RendererAPI::CreateDescriptorSets()
{
    std::array<VkDescriptorSetLayout, s_kMaxFrameInFlight> setLayout{m_descriptorSetLayout,
                                                                     m_descriptorSetLayout};

    VkDescriptorSetAllocateInfo setAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = m_descriptorPool,
        .descriptorSetCount = setLayout.size(),
        .pSetLayouts = setLayout.data(),
    };

    m_descriptorSets.resize(s_kMaxFrameInFlight);
    vkAllocateDescriptorSets(m_context.device, &setAllocInfo, m_descriptorSets.data());

    for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
    {
        VkDescriptorBufferInfo bufferInfo{
            .buffer = m_uniformBuffers[idx],
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

        vkUpdateDescriptorSets(m_context.device, writeDescriptorSets.size(),
                               writeDescriptorSets.data(), 0, nullptr);
    }
}

void Vulkan_RendererAPI::CreateUniformBuffer()
{
    m_uniformBuffers.resize(s_kMaxFrameInFlight);
    m_uniformBufferMemories.resize(s_kMaxFrameInFlight);
    m_mappedUniformBuffers.resize(s_kMaxFrameInFlight);

    VkDeviceSize size = sizeof(UniformBufferObject);
    for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
    {
        Buffer buffer = CreateBuffer(
            size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        m_uniformBuffers[idx] = buffer.buffer;
        m_uniformBufferMemories[idx] = buffer.memory;
        vkMapMemory(m_context.device, buffer.memory, 0, size, 0, &m_mappedUniformBuffers[idx]);
    }
}

void Vulkan_RendererAPI::UpdateUniformBuffer(uint32_t frameIndex)
{
    static float rad = Rad(90.0f);
    rad += Application::GetApp().GetDeltaTime();

    UniformBufferObject ubo;
    ubo.model = Rotate(Mat4(1.0f), rad, Vec3(0.0f, 0.0f, 1.0f));

    ubo.view = LookAt(Vec3(2.0f, 2.0f, 2.0f), Vec3(0.0f), Vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = Perspective(Rad(45.0f),
                           static_cast<float>(m_context.extent.width) /
                               static_cast<float>(m_context.extent.height),
                           0.1f, 10.0f);

    // Vulkan has inverted y axis
    ubo.proj[1][1] *= -1;

    std::memcpy(m_mappedUniformBuffers[frameIndex], &ubo, sizeof(UniformBufferObject));
}

void Vulkan_RendererAPI::CreateTextureImage()
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

    Buffer stageBuffer =
        CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void *mappedData = nullptr;
    vkMapMemory(m_context.device, stageBuffer.memory, 0, imageSize, 0, &mappedData);

    std::memcpy(mappedData, pixels, imageSize);
    vkUnmapMemory(m_context.device, stageBuffer.memory);

    stbi_image_free(pixels);

    Texture tex = CreateImage(width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_texture = tex;

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = m_context.graphicCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(m_context.device, &allocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    TransitionImageLayout(cmdBuffer, tex.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    CopyBufferToImage(cmdBuffer, stageBuffer.buffer, tex.image, width, height);

    TransitionImageLayout(cmdBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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

    vkQueueSubmit2(m_context.graphicQueue, 1, &submitInfo, VK_NULL_HANDLE);

    vkQueueWaitIdle(m_context.graphicQueue);

    vkDestroyBuffer(m_context.device, stageBuffer.buffer, nullptr);
    vkFreeMemory(m_context.device, stageBuffer.memory, nullptr);
}

void Vulkan_RendererAPI::CreateTextureImageView()
{
    m_texture.view =
        CreateImageView(m_texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Vulkan_RendererAPI::CreateTextureSampler()
{
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
        .maxAnisotropy = m_context.physicalDeviceProperties.properties.limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    vkCreateSampler(m_context.device, &samplerCreateInfo, nullptr, &m_sampler);
}

void Vulkan_RendererAPI::CreateDepthTexture()
{
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    m_depthTex = CreateImage(m_context.extent.width, m_context.extent.height, depthFormat,
                             VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    m_depthTex.view = CreateImageView(m_depthTex.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Vulkan_RendererAPI::CleanUpDepthTexture()
{

    vkDestroyImageView(m_context.device, m_depthTex.view, nullptr);
    vkDestroyImage(m_context.device, m_depthTex.image, nullptr);
    vkFreeMemory(m_context.device, m_depthTex.memory, nullptr);
}

VkImageView Vulkan_RendererAPI::CreateImageView(const VkImage &image, VkFormat format,
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
    vkCreateImageView(m_context.device, &imageViewCreateInfo, nullptr, &view);

    return view;
}

Vulkan_RendererAPI::Texture Vulkan_RendererAPI::CreateImage(uint32_t texWidth, uint32_t texHeight,
                                                            VkFormat format, VkImageTiling tilt,
                                                            VkImageUsageFlags usage,
                                                            VkMemoryPropertyFlags properties)
{
    VkImageCreateInfo imageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {texWidth, texHeight, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tilt,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkImage image;

    vkCreateImage(m_context.device, &imageCreateInfo, nullptr, &image);

    VkMemoryRequirements memReqs;

    vkGetImageMemoryRequirements(m_context.device, image, &memReqs);
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, properties),
    };

    VkDeviceMemory memory;

    vkAllocateMemory(m_context.device, &allocInfo, nullptr, &memory);

    vkBindImageMemory(m_context.device, image, memory, 0);

    return Texture{.image = image, .memory = memory};
}

void Vulkan_RendererAPI::CopyBufferToImage(VkCommandBuffer cmdBuffer, VkBuffer src, VkImage image,
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

void Vulkan_RendererAPI::CopyBuffer(Buffer dst, Buffer src, VkDeviceSize size)
{

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = m_context.graphicCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(m_context.device, &allocInfo, &cmdBuffer);

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

    vkCmdCopyBuffer(cmdBuffer, src.buffer, dst.buffer, 1, &region);

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

    vkQueueSubmit2(m_context.graphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_context.graphicQueue);
}

void Vulkan_RendererAPI::CreateCommandBuffers()
{
    VkCommandBufferAllocateInfo graphicCmdBufferAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = m_context.graphicCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = s_kMaxFrameInFlight,
    };

    vkAllocateCommandBuffers(m_context.device, &graphicCmdBufferAllocInfo, m_graphicCmdBuffers);
}

void Vulkan_RendererAPI::TransitionSwapchainImageLayout(
    uint32_t imageIndex, VkImageLayout oldLayout, VkImageLayout newLayout,
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask,
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
        .image = m_context.pSwapchainImages[imageIndex],
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

void Vulkan_RendererAPI::TransitionImageLayout(VkCommandBuffer cmdBuffer, VkImage image,
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
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .levelCount = 1,
                             .layerCount = 1},
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

void Vulkan_RendererAPI::TransitionImageLayout(
    VkCommandBuffer cmdBuffer, VkImage image, VkPipelineStageFlags2 srcStage,
    VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess,
    VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t srcQueueFamilyIndex,
    uint32_t dstQueueFamilyIndex, VkImageAspectFlags imageAspect)
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

Vulkan_RendererAPI::Buffer
Vulkan_RendererAPI::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlag,
                                 VkSharingMode sharingMode, VkMemoryPropertyFlags propertyFlag,
                                 uint32_t queueFamilyCount, uint32_t *queueFamilyIndices)
{
    Buffer buffer;

    VkBufferCreateInfo bufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .size = size,
        .usage = usageFlag,
        .sharingMode = sharingMode,
        .queueFamilyIndexCount = queueFamilyCount,
        .pQueueFamilyIndices = queueFamilyIndices,
    };

    vkCreateBuffer(m_context.device, &bufferCreateInfo, nullptr, &buffer.buffer);

    VkMemoryRequirements memoryReqs;

    vkGetBufferMemoryRequirements(m_context.device, buffer.buffer, &memoryReqs);

    VkMemoryAllocateInfo memoryAllocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memoryReqs.size,
        .memoryTypeIndex = FindMemoryType(memoryReqs.memoryTypeBits, propertyFlag),
    };

    vkAllocateMemory(m_context.device, &memoryAllocInfo, nullptr, &buffer.memory);
    vkBindBufferMemory(m_context.device, buffer.buffer, buffer.memory, 0);

    return buffer;
}

uint32_t Vulkan_RendererAPI::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties2 memProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
        .pNext = nullptr,
    };
    vkGetPhysicalDeviceMemoryProperties2(m_context.physicalDevice, &memProps);

    for (size_t idx = 0; idx < memProps.memoryProperties.memoryTypeCount; ++idx)
    {
        if ((typeFilter & (1 << idx)) &&
            (memProps.memoryProperties.memoryTypes[idx].propertyFlags & properties))
        {
            return idx;
        }
    }

    LOG_ERROR("VULKAN", "Failed to find suitable memory type!");
    throw std::runtime_error("[VULKAN] Failed to find suitable memory type!");
}

void Vulkan_RendererAPI::RecordCommandBuffer(uint32_t imageIndex)
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
        m_graphicCmdBuffers[m_frameIndex], m_depthTex.image,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    VkClearValue clearColor{
        .color = VkClearColorValue{{0.0f, 0.0f, 0.0f, 0.0f}},
    };

    VkClearValue clearDepth = {.depthStencil = {.depth = 1.0f, .stencil = 0}};

    VkRenderingAttachmentInfo depthAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = m_depthTex.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = clearDepth};

    VkRenderingAttachmentInfo renderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = m_context.pSwapchainImageViews[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearColor};

    VkRenderingInfo renderingInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .pNext = nullptr,
                                  .renderArea = {.offset = {0, 0}, .extent = m_context.extent},
                                  .layerCount = 1,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &renderingAttachmentInfo,
                                  .pDepthAttachment = &depthAttachmentInfo};

    vkCmdBeginRendering(m_graphicCmdBuffers[m_frameIndex], &renderingInfo);

    vkCmdBindPipeline(m_graphicCmdBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_pipeline);
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(m_context.extent.width),
        .height = static_cast<float>(m_context.extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,

    };

    VkRect2D scissor{
        .offset = VkOffset2D{0, 0},
        .extent = m_context.extent,
    };

    vkCmdSetViewport(m_graphicCmdBuffers[m_frameIndex], 0, 1, &viewport);
    vkCmdSetScissor(m_graphicCmdBuffers[m_frameIndex], 0, 1, &scissor);

    vkCmdBindPipeline(m_graphicCmdBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      m_pipeline);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_graphicCmdBuffers[m_frameIndex], 0, 1, &m_vertexBuffer, &offset);
    vkCmdBindIndexBuffer(m_graphicCmdBuffers[m_frameIndex], m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(m_graphicCmdBuffers[m_frameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 1, &m_descriptorSets[m_frameIndex], 0, nullptr);

    vkCmdDrawIndexed(m_graphicCmdBuffers[m_frameIndex], 12, 1, 0, 0, 0);

    vkCmdEndRendering(m_graphicCmdBuffers[m_frameIndex]);

    TransitionSwapchainImageLayout(
        imageIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {}, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(m_graphicCmdBuffers[m_frameIndex]);
}

void Vulkan_RendererAPI::CreateSyncObjects()
{
    VkSemaphoreCreateInfo presentComplete{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };

    VkSemaphoreCreateInfo renderFinished{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };

    VkFenceCreateInfo drawFence{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
    {
        vkCreateSemaphore(m_context.device, &presentComplete, nullptr,
                          &m_frames[idx].presentCompleteSemaphore);
        vkCreateSemaphore(m_context.device, &renderFinished, nullptr,
                          &m_frames[idx].renderFinishedSemaphore);
        vkCreateFence(m_context.device, &drawFence, nullptr, &m_frames[idx].drawFence);
    }
}

void Vulkan_RendererAPI::DrawFrame()
{
    if (vkWaitForFences(m_context.device, 1, &m_frames[m_frameIndex].drawFence, VK_TRUE,
                        UINT64_MAX) != VK_SUCCESS)
    {
        LOG_ASSERT("VULKAN", "Failed to wait for draw fence");
    }

    uint32_t imageIndex = UINT32_MAX;

    VkResult acquireResult = vkAcquireNextImageKHR(
        m_context.device, m_context.swapchain, UINT64_MAX,
        m_frames[m_frameIndex].presentCompleteSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain();
        return;
    }
    else if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
    {
        LOG_ASSERT(acquireResult == VK_TIMEOUT || acquireResult == VK_NOT_READY,
                   "Failed to acquire next image to present");
    }

    RecordCommandBuffer(imageIndex);

    VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = m_frames[m_frameIndex].presentCompleteSemaphore,
        .value = 0,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .semaphore = m_frames[m_frameIndex].renderFinishedSemaphore,
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

    vkResetFences(m_context.device, 1, &m_frames[m_frameIndex].drawFence);
    vkQueueSubmit2(m_context.graphicQueue, 1, &submitInfo, m_frames[m_frameIndex].drawFence);

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_frames[m_frameIndex].renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_context.swapchain,
        .pImageIndices = &imageIndex,
    };

    VkResult presentResult = vkQueuePresentKHR(m_context.graphicQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
        Application::GetApp().IsResizing())
    {
        RecreateSwapchain();
    }
    else if (presentResult != VK_SUCCESS)
    {
        LOG_ASSERT(presentResult == VK_TIMEOUT || presentResult == VK_NOT_READY,
                   "Failed to acquire next image to present");
    }

    vkQueueWaitIdle(m_context.graphicQueue);
}

void Vulkan_RendererAPI::DrawTest()
{
    DrawFrame();
    m_frameIndex = (m_frameIndex++ % s_kMaxFrameInFlight);

    // vkQueueWaitIdle(m_context.graphicQueue);
}

void Vulkan_RendererAPI::RecreateSwapchain()
{
    vkQueueWaitIdle(m_context.graphicQueue);

    for (size_t idx = 0; idx < m_context.swapchainImageCount; ++idx)
    {
        vkDestroyImageView(m_context.device, m_context.pSwapchainImageViews[idx], nullptr);
    }

    VkSwapchainKHR oldSwapchain = m_context.swapchain;
    CreateSwapchain(oldSwapchain);

    vkDestroySwapchainKHR(m_context.device, oldSwapchain, nullptr);

    CreateSwapchainImageViews();

    CleanUpDepthTexture();
    CreateDepthTexture();
}

void Vulkan_RendererAPI::CreateInstance()
{
    VkBool32 profileSupported;

    vpGetInstanceProfileSupport(nullptr, &profile, &profileSupported);

    if (!profileSupported)
    {
        // TODO: Fallback path
        m_minProfileSupported = VK_FALSE;

        LOG_ERROR("VULKAN", "Vulkan profile not supported");
        throw std::runtime_error("Vulkan profile not supported");
    }

    const std::vector<const char *> requiredExts = {
        "VK_KHR_surface",
#ifdef VOID_WIN32
        "VK_KHR_win32_surface",
#else

#endif // VOID_WIN32

#ifdef VOID_DEBUG
        "VK_EXT_debug_utils",
#endif // VOID_DEBUG
    };

    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> extProperties(extCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extProperties.data());

    auto extensionIter = std::ranges::find_if(requiredExts, [&extProperties](const char *ext)
                                              { return !IsExtAvailable(extProperties, ext); });

    if (extensionIter != requiredExts.end())
    {
        LOG_ERROR("VULKAN", "No support for %s extension", *extensionIter);

        throw std::runtime_error("[VULKAN] No support for " + std::string(*extensionIter));
    }

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> layerProperties(layerCount);

    vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());
    const std::vector<const char *> requiredLayers = {
#ifdef VOID_DEBUG
        "VK_LAYER_KHRONOS_validation"
#endif // VOID_DEBUG
    };

    auto layerIter = std::ranges::find_if(requiredLayers, [&layerProperties](const char *layer)
                                          { return !IsLayerAvailable(layerProperties, layer); });

    if (layerIter != requiredLayers.end())
    {
        LOG_ERROR("VULKAN", "No support for %s layer", *layerIter);

        throw std::runtime_error("[VULKAN] No support for " + std::string(*layerIter));
    }

    VkInstanceCreateInfo instanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredExts.size()),
        .ppEnabledExtensionNames = requiredExts.data(),
    };

    VpInstanceCreateInfo vpInstanceCreateInfo{
        .pCreateInfo = &instanceCreateInfo,
        .enabledFullProfileCount = 1,
        .pEnabledFullProfiles = &profile,
    };

    VkResult result = vpCreateInstance(&vpInstanceCreateInfo, nullptr, &m_context.instance);

    if (result != VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "Failed to create Vulkan instance!");

        throw std::runtime_error("[VULKAN] Failed to create Vulkan instance!");
    }
}

void Vulkan_RendererAPI::CreatePhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_context.instance, &deviceCount, nullptr);

    std::array<VkPhysicalDevice, 8> devices;
    vkEnumeratePhysicalDevices(m_context.instance, &deviceCount, devices.data());

    const std::vector<const char *> requiredDeviceExtensions = {"VK_KHR_swapchain"};

    for (size_t idx = 0; idx < deviceCount; ++idx)
    {
        VkBool32 profileSupported;

        vpGetPhysicalDeviceProfileSupport(m_context.instance, devices[idx], &profile,
                                          &profileSupported);

        if (profileSupported)
        {

            uint32_t deviceExtensionCount = 0;
            vkEnumerateDeviceExtensionProperties(devices[idx], nullptr, &deviceExtensionCount,
                                                 nullptr);

            if (deviceExtensionCount == 0)
            {
                continue;
            }
            bool supportsRequiredExtensions = false;
            std::vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
            vkEnumerateDeviceExtensionProperties(devices[idx], nullptr, &deviceExtensionCount,
                                                 deviceExtensions.data());

            supportsRequiredExtensions = std::ranges::all_of(
                requiredDeviceExtensions,
                [&](const char *ext)
                {
                    return std::ranges::any_of(deviceExtensions, [&](const VkExtensionProperties &p)
                                               { return std::strcmp(ext, p.extensionName) == 0; });
                });

            if (!supportsRequiredExtensions)
            {
                continue;
            }

            VkSurfaceCapabilitiesKHR surfaceCapabilities{};
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[idx], m_context.surface,
                                                      &surfaceCapabilities);

            uint32_t surfaceFormatCount = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(devices[idx], m_context.surface,
                                                 &surfaceFormatCount, nullptr);

            std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(devices[idx], m_context.surface,
                                                 &surfaceFormatCount, surfaceFormats.data());

            uint32_t presentModeCount = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(devices[idx], m_context.surface,
                                                      &presentModeCount, nullptr);

            std::vector<VkPresentModeKHR> presentModes(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(devices[idx], m_context.surface,
                                                      &presentModeCount, presentModes.data());

            if (presentModeCount == 0 || surfaceFormatCount == 0)
            {
                continue;
            }

            m_context.physicalDeviceProperties.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            m_context.physicalDeviceProperties.pNext = nullptr;

            vkGetPhysicalDeviceProperties2(devices[idx], &m_context.physicalDeviceProperties);

            uint32_t queueFamilyPropCount;
            vkGetPhysicalDeviceQueueFamilyProperties2(devices[idx], &queueFamilyPropCount, nullptr);

            std::vector<VkQueueFamilyProperties2> queueFamilyProps(
                queueFamilyPropCount,
                {.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2, .pNext = nullptr});

            vkGetPhysicalDeviceQueueFamilyProperties2(devices[idx], &queueFamilyPropCount,
                                                      queueFamilyProps.data());

            bool isDiscreteDevice = m_context.physicalDeviceProperties.properties.deviceType ==
                                    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            bool supportVulkan1_3 =
                m_context.physicalDeviceProperties.properties.apiVersion >= VK_API_VERSION_1_3;

            bool supportGraphics = std::ranges::any_of(
                queueFamilyProps, [](const VkQueueFamilyProperties2 &p)
                { return (p.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0; });

            VkPhysicalDeviceVulkan11Features vulkan11features{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
                .pNext = nullptr,
            };

            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
                .pNext = &vulkan11features};

            VkPhysicalDeviceFeatures2 features2{.sType =
                                                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                                .pNext = &extendedDynamicStateFeatures};

            vkGetPhysicalDeviceFeatures2(devices[idx], &features2);

            if (isDiscreteDevice && supportVulkan1_3 && supportGraphics &&
                vulkan11features.shaderDrawParameters &&
                extendedDynamicStateFeatures.extendedDynamicState)
            {
                m_context.physicalDevice = devices[idx];
                return;
            }
        }
    }

    // TODO: Fallback path
    m_minProfileSupported = VK_FALSE;

    LOG_ERROR("VULKAN", "Can find suitable physical device");

    throw std::runtime_error("Vulkan can not find suitable physical device");
}

} // namespace VoidEngine
