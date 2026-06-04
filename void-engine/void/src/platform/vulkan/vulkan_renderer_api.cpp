#include "vulkan_renderer_api.h"
#include "application.h"
#include "log.h"
#include <fstream>

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
    SelectPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapchain();
    CreateImageViews();
    CreateGraphicPipeline();
    CreateCommandPool();
    CreateCommandBuffers();
    CreateSyncObjects();

    return true;
}

void Vulkan_RendererAPI::NewFrame() {}

void Vulkan_RendererAPI::EndFrame() {}

void *Vulkan_RendererAPI::CreateAndSubmitBuffer(void *const data,
                                                size_t byteSize,
                                                BufferType type)
{
    return nullptr;
}

void Vulkan_RendererAPI::DestroyBuffer(GraphicBuffer &buffer) {}

void *Vulkan_RendererAPI::CompileShader(const wchar_t *file, const char *entry,
                                        const char *target)
{
    return nullptr;
}
void *Vulkan_RendererAPI::CreateShader(void **compiledSrc, ShaderType type)
{
    return nullptr;
}
void Vulkan_RendererAPI::DestroyShader(GraphicShader &shader) {}

void Vulkan_RendererAPI::Draw(MeshResource *mesh, MaterialResource *material) {}

void Vulkan_RendererAPI::Shutdown()
{
    for (size_t idx = 0; idx < s_kMaxFrameInFlight; ++idx)
    {
        vkDestroySemaphore(m_logicalDevice,
                           m_frames[idx].presentCompleteSemaphore, nullptr);
        vkDestroySemaphore(m_logicalDevice,
                           m_frames[idx].renderFinishedSemaphore, nullptr);
        vkDestroyFence(m_logicalDevice, m_frames[idx].drawFence, nullptr);
    }
    vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);

    vkDestroyPipeline(m_logicalDevice, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_logicalDevice, m_pipelineLayout, nullptr);

    for (VkImageView &view : m_imageViews)
    {
        vkDestroyImageView(m_logicalDevice, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    vkDestroySwapchainKHR(m_logicalDevice, m_swapchain, nullptr);

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkDestroyDevice(m_logicalDevice, nullptr);

    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_instance, "vkDestroyDebugUtilsMessengerEXT");

    if (vkDestroyDebugUtilsMessengerEXT)
    {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }
    else
    {
        LOG_ERROR("VULKAN",
                  "Vulkan failed to acquire debug messenger destruction");

        throw std::runtime_error(
            "[VULKAN] Failed to acquire vkDestroyDebugUtilsMessengerEXT!");
    }

    vkDestroyInstance(m_instance, nullptr);
}

static bool
IsExtAvailable(const std::vector<VkExtensionProperties> &extProperties,
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

static bool
IsLayerAvailable(const std::vector<VkLayerProperties> &layerProperties,
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

void Vulkan_RendererAPI::CreateInstance()
{
    uint32_t apiVersion = 0;

    auto r = vkEnumerateInstanceVersion(&apiVersion);

    if (r != VK_SUCCESS)
    {
        // LOG
        LOG_ERROR("VULKAN", "Vulkan failed to enumerate Vulkan api version");
        throw std::runtime_error("Failed to enumerate Vulkan api version!");
    }

    const VkApplicationInfo appInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pNext = nullptr,
                                    .pApplicationName = "No App",
                                    .applicationVersion =
                                        VK_MAKE_VERSION(1, 0, 0),
                                    .pEngineName = "Void Engine",
                                    .engineVersion = VK_MAKE_VERSION(0, 0, 1),
                                    .apiVersion = apiVersion};

    uint32_t extCount = 0;

    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> extProperties(extCount);

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

    vkEnumerateInstanceExtensionProperties(nullptr, &extCount,
                                           extProperties.data());

    auto extensionIter =
        std::ranges::find_if(requiredExts, [&extProperties](const char *ext)
                             { return !IsExtAvailable(extProperties, ext); });

    if (extensionIter != requiredExts.end())
    {
        LOG_ERROR("VULKAN", "No support for %s extension", *extensionIter);

        throw std::runtime_error("[VULKAN] No support for " +
                                 std::string(*extensionIter));
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

    auto layerIter = std::ranges::find_if(
        requiredLayers, [&layerProperties](const char *layer)
        { return !IsLayerAvailable(layerProperties, layer); });

    if (layerIter != requiredLayers.end())
    {
        LOG_ERROR("VULKAN", "No support for %s layer", *layerIter);

        throw std::runtime_error("[VULKAN] No support for " +
                                 std::string(*layerIter));
    }

    VkInstanceCreateInfo instanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames = requiredLayers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredExts.size()),
        .ppEnabledExtensionNames = requiredExts.data(),
    };

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance) !=
        VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "Failed to create Vulkan instance!");

        throw std::runtime_error("[VULKAN] Failed to create Vulkan instance!");
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
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

    if (vkCreateWin32SurfaceKHR(m_instance, &surfaceCreateInfo, nullptr,
                                &m_surface) != VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "Failed to create Win32 surface!");
        throw std::runtime_error("[VULKAN] Failed to create Win32 surface!");
    }

#else
    LOG_ERROR("VULKAN",
              "Failed to detect and create platform-dependent surface!");
    throw std::runtime_error(
        "[VULKAN] Failed to detect and create platform-dependent surface!");
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
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_instance, "vkCreateDebugUtilsMessengerEXT");

    if (vkCreateDebugUtilsMessengerEXT)
    {
        if (vkCreateDebugUtilsMessengerEXT(m_instance,
                                           &debugMessengerCreateInfo, nullptr,
                                           &m_debugMessenger) != VK_SUCCESS)
        {
            LOG_ERROR("VULKAN", "Vulkan debug messenger creation failed!");

            throw std::runtime_error(
                "[VULKAN] Failed to create debug messenger!");
        }
    }
    else
    {
        LOG_ERROR("VULKAN", "Failed to acquire vkCreateDebugUtilsMessengerEXT");

        throw std::runtime_error(
            "[VULKAN] Failed to acquire vkCreateDebugUtilsMessengerEXT!");
    }
}

void Vulkan_RendererAPI::SelectPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    if (deviceCount == 0)
    {
        LOG_ERROR("VULKAN", "No physical devices found");

        throw std::runtime_error("[VULKAN] No physical devices found!");
    }

    const std::vector<const char *> requiredDeviceExtensions = {
        "VK_KHR_swapchain"};

    for (size_t idx = 0; idx < devices.size(); ++idx)
    {
        const VkPhysicalDevice &device = devices[idx];

        uint32_t deviceExtensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr,
                                             &deviceExtensionCount, nullptr);

        bool supportsRequiredExtensions = false;
        if (deviceExtensionCount != 0)
        {
            std::vector<VkExtensionProperties> deviceExtensions(
                deviceExtensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr,
                                                 &deviceExtensionCount,
                                                 deviceExtensions.data());

            supportsRequiredExtensions = std::ranges::all_of(
                requiredDeviceExtensions,
                [&](const char *ext)
                {
                    return std::ranges::any_of(
                        deviceExtensions, [&](const VkExtensionProperties &p)
                        { return std::strcmp(ext, p.extensionName) == 0; });
                });
        }

        bool supportSwapchain = false;

        if (supportsRequiredExtensions)
        {
            VkSurfaceCapabilitiesKHR surfaceCapabilities{};
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface,
                                                      &surfaceCapabilities);

            uint32_t surfaceFormatCount = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface,
                                                 &surfaceFormatCount, nullptr);

            if (surfaceFormatCount != 0)
            {
                std::vector<VkSurfaceFormatKHR> surfaceFormats(
                    surfaceFormatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface,
                                                     &surfaceFormatCount,
                                                     surfaceFormats.data());
            }

            uint32_t presentModeCount = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                device, m_surface, &presentModeCount, nullptr);

            if (presentModeCount != 0)
            {
                std::vector<VkPresentModeKHR> presentModes(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(
                    device, m_surface, &presentModeCount, presentModes.data());
            }

            if (presentModeCount != 0 && surfaceFormatCount != 0)
            {
                supportSwapchain = true;
            }
        }

        VkPhysicalDeviceProperties2 property{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = nullptr};
        vkGetPhysicalDeviceProperties2(device, &property);

        uint32_t queueFamilyPropCount;
        vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyPropCount,
                                                  nullptr);

        std::vector<VkQueueFamilyProperties2> queueFamilyProps(
            queueFamilyPropCount,
            {.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
             .pNext = nullptr});

        vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyPropCount,
                                                  queueFamilyProps.data());

        bool isDiscreteDevice = property.properties.deviceType ==
                                VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        bool supportVulkan1_4 =
            property.properties.apiVersion >= VK_API_VERSION_1_4;

        bool supportGraphics =
            std::ranges::any_of(queueFamilyProps,
                                [](const VkQueueFamilyProperties2 &p)
                                {
                                    return (p.queueFamilyProperties.queueFlags &
                                            VK_QUEUE_GRAPHICS_BIT) > 0;
                                });

        VkPhysicalDeviceVulkan11Features vulkan11features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .pNext = nullptr,
        };

        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT
            extendedDynamicStateFeatures{
                .sType =
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
                .pNext = &vulkan11features};

        VkPhysicalDeviceVulkan13Features vulkan13Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &extendedDynamicStateFeatures};

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &vulkan13Features};

        vkGetPhysicalDeviceFeatures2(device, &features2);

        bool supportsRequiredFeatures =
            vulkan13Features.dynamicRendering &&
            vulkan13Features.synchronization2 &&
            extendedDynamicStateFeatures.extendedDynamicState &&
            vulkan11features.shaderDrawParameters;

        if (isDiscreteDevice && supportVulkan1_4 && supportGraphics &&
            supportsRequiredExtensions && supportsRequiredFeatures &&
            supportSwapchain)
        {
            m_physicalDevice = device;
            break;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        LOG_ERROR("VULKAN", "No suitable physical device found!");

        throw std::runtime_error("[VULKAN] No suitable physical device found!");
    }
}

void Vulkan_RendererAPI::CreateLogicalDevice()
{
    uint32_t queueFamilyPropCount = 0;

    vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice,
                                              &queueFamilyPropCount, nullptr);
    std::vector<VkQueueFamilyProperties2> queueFamilyProps(
        queueFamilyPropCount,
        {
            .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
            .pNext = nullptr,
        });

    vkGetPhysicalDeviceQueueFamilyProperties2(
        m_physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());

    for (size_t idx = 0; idx < queueFamilyProps.size(); ++idx)
    {
        if ((queueFamilyProps[idx].queueFamilyProperties.queueFlags &
             VK_QUEUE_GRAPHICS_BIT) > 0)
        {
            VkBool32 supportSurface = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, idx,
                                                 m_surface, &supportSurface);

            if (supportSurface)
            {
                m_queueFamilyIndex = idx;
                break;
            }
        }
    }

    if (m_queueFamilyIndex == ~0)
    {
        LOG_ERROR(
            "VULKAN",
            "Could not find a queue for graphics and present -> terminating");
        throw std::runtime_error(
            "Could not find a queue for graphics and present -> terminating");
    }

    float queuePriority = 0.5f;
    VkDeviceQueueCreateInfo deviceQueueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .queueFamilyIndex = m_queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    VkPhysicalDeviceVulkan11Features vulkan11features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = nullptr,
        .shaderDrawParameters = VK_TRUE,
    };

    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .pNext = &vulkan11features,
        .extendedDynamicState = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features vulkan13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &extendedDynamicStateFeatures,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &vulkan13Features};

    std::vector<const char *> requiredDeviceExtension = {"VK_KHR_swapchain"};

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &deviceQueueCreateInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount =
            static_cast<uint32_t>(requiredDeviceExtension.size()),
        .ppEnabledExtensionNames = requiredDeviceExtension.data(),
        .pEnabledFeatures = nullptr,
    };

    if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr,
                       &m_logicalDevice) != VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "Failed to create logical device!");

        throw std::runtime_error("[VULKAN] Failed to create logical device!");
    }

    vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndex, 0, &m_graphicQueue);

    if (m_graphicQueue == VK_NULL_HANDLE)
    {
        LOG_ERROR("VULKAN", "Failed to retrieve graphic queue");

        throw std::runtime_error("[VULKAN] Failed to retrieve graphic queue");
    }
}

VkSurfaceFormatKHR Vulkan_RendererAPI::ChooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &formats)
{
    for (const VkSurfaceFormatKHR &format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    LOG_WARN("[VULKAN]", "Surface does not have ideal format!");

    return formats[0];
}

VkPresentModeKHR Vulkan_RendererAPI::ChoosePresentMode(
    const std::vector<VkPresentModeKHR> &modes)
{
    for (const VkPresentModeKHR &mode : modes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return mode;
        }
    }

    LOG_INFO("VULKAN", "Present mode does not support MAILBOX!");

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Vulkan_RendererAPI::ChooseSwapchainExtent(
    const VkSurfaceCapabilitiesKHR &capabilities)
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
    extent.height =
        std::clamp(extent.height, capabilities.minImageExtent.height,
                   capabilities.maxImageExtent.height);

    return extent;
}

void Vulkan_RendererAPI::CreateSwapchain(VkSwapchainKHR oldSwapchain)
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface,
                                              &surfaceCapabilities);

    uint32_t surfaceFormatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface,
                                         &surfaceFormatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);

    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface,
                                         &surfaceFormatCount,
                                         surfaceFormats.data());

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface,
                                              &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);

    vkGetPhysicalDeviceSurfacePresentModesKHR(
        m_physicalDevice, m_surface, &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR format = ChooseSurfaceFormat(surfaceFormats);
    VkPresentModeKHR mode = ChoosePresentMode(presentModes);
    VkExtent2D extent2d = ChooseSwapchainExtent(surfaceCapabilities);

    m_format = format;
    m_extent = extent2d;

    uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 &&
        imageCount > surfaceCapabilities.maxImageCount)
    {
        imageCount = surfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = m_surface,
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

    if (vkCreateSwapchainKHR(m_logicalDevice, &swapchainCreateInfo, nullptr,
                             &m_swapchain) != VK_SUCCESS)
    {
        LOG_ERROR("VULKAN", "Failed to create swapchain!");
        throw std::runtime_error("[VULKAN] Failed to create swapchain!");
    }

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(m_logicalDevice, m_swapchain, &swapchainImageCount,
                            nullptr);

    if (swapchainImageCount == 0)
    {
        LOG_ERROR("VULKAN", "Swapchain image count is 0!");
        throw std::runtime_error("[VULKAN] Swapchain image count is 0!");
    }

    m_images.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(m_logicalDevice, m_swapchain, &swapchainImageCount,
                            m_images.data());
}

void Vulkan_RendererAPI::CreateImageViews()
{
    m_imageViews.resize(m_images.size());

    for (size_t idx = 0; idx < m_images.size(); ++idx)
    {
        VkImageViewCreateInfo viewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .image = m_images[idx],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_format.format,
        };

        viewCreateInfo.components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        };

        viewCreateInfo.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        };

        if (vkCreateImageView(m_logicalDevice, &viewCreateInfo, nullptr,
                              &m_imageViews[idx]) != VK_SUCCESS)
        {
            LOG_ERROR("VULKAN", "Failed to create image view of swapchain!");
            throw std::runtime_error(
                "[VULKAN] Failed to create image view of swapchain!");
        }
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
                           "engine\\void\\src\\shader\\spirv\\triangle.spv");
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

    //auto bindingDescription = Vertex::GetBindingDescription();
    //auto attributeDescriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        //.vertexBindingDescriptionCount = 1,
        //.pVertexBindingDescriptions = &bindingDescription,
        //.vertexAttributeDescriptionCount =
        //    static_cast<uint32_t>(attributeDescriptions.size()),
        //.pVertexAttributeDescriptions = attributeDescriptions.data(),
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
        .width = static_cast<float>(m_extent.width),
        .height = static_cast<float>(m_extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,

    };

    VkRect2D scissor{
        .offset = VkOffset2D{0, 0},
        .extent = m_extent,
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
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
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
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0,
    };

    vkCreatePipelineLayout(m_logicalDevice, &pipelineLayout, nullptr,
                           &m_pipelineLayout);

    VkPipelineRenderingCreateInfo pipelineRendering{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &m_format.format,
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
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = m_pipelineLayout,
        .renderPass = nullptr,
    };

    vkCreateGraphicsPipelines(m_logicalDevice, VK_NULL_HANDLE, 1,
                              &graphicPipeline, nullptr, &m_pipeline);

    vkDestroyShaderModule(m_logicalDevice, module, nullptr);
}

VkShaderModule
Vulkan_RendererAPI::CreateShaderModule(const std::vector<char> &binaryCode)
{
    VkShaderModuleCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = binaryCode.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t *>(binaryCode.data()),
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;

    vkCreateShaderModule(m_logicalDevice, &createInfo, nullptr, &shaderModule);

    return shaderModule;
}

void Vulkan_RendererAPI::CreateCommandPool()
{
    VkCommandPoolCreateInfo cmdPool{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_queueFamilyIndex,
    };

    vkCreateCommandPool(m_logicalDevice, &cmdPool, nullptr, &m_commandPool);
}

void Vulkan_RendererAPI::CreateCommandBuffers()
{
    VkCommandBufferAllocateInfo cmdBufferAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = m_commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = s_kMaxFrameInFlight,
    };

    vkAllocateCommandBuffers(m_logicalDevice, &cmdBufferAllocInfo,
                             m_frameCmdBuffers);
}

void Vulkan_RendererAPI::TransitionImageLayout(
    uint32_t imageIndex, VkImageLayout oldLayout, VkImageLayout newLayout,
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
    VkPipelineStageFlags2 srcStageMask, VkPipelineStageFlags2 dstStageMask)
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
        .image = m_images[imageIndex],
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

    vkCmdPipelineBarrier2(m_frameCmdBuffers[m_frameIndex], &dependencyInfo);
}

void Vulkan_RendererAPI::RecordCommandBuffer(uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = 0,
    };

    vkBeginCommandBuffer(m_frameCmdBuffers[m_frameIndex], &beginInfo);

    TransitionImageLayout(imageIndex, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkClearValue clearVal{
        .color = VkClearColorValue{{0.0f, 0.0f, 0.0f, 0.0f}},
    };

    VkRenderingAttachmentInfo renderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = m_imageViews[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearVal};

    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .renderArea = {.offset = {0, 0}, .extent = m_extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &renderingAttachmentInfo,
    };

    vkCmdBeginRendering(m_frameCmdBuffers[m_frameIndex], &renderingInfo);

    vkCmdBindPipeline(m_frameCmdBuffers[m_frameIndex],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(m_extent.width),
        .height = static_cast<float>(m_extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,

    };

    VkRect2D scissor{
        .offset = VkOffset2D{0, 0},
        .extent = m_extent,
    };

    vkCmdSetViewport(m_frameCmdBuffers[m_frameIndex], 0, 1, &viewport);
    vkCmdSetScissor(m_frameCmdBuffers[m_frameIndex], 0, 1, &scissor);

    vkCmdDraw(m_frameCmdBuffers[m_frameIndex], 3, 1, 0, 0);

    vkCmdEndRendering(m_frameCmdBuffers[m_frameIndex]);

    TransitionImageLayout(imageIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(m_frameCmdBuffers[m_frameIndex]);
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

    vkCreateSemaphore(m_logicalDevice, &presentComplete, nullptr,
                      &m_frames[m_frameIndex].presentCompleteSemaphore);
    vkCreateSemaphore(m_logicalDevice, &renderFinished, nullptr,
                      &m_frames[m_frameIndex].renderFinishedSemaphore);
    vkCreateFence(m_logicalDevice, &drawFence, nullptr,
                  &m_frames[m_frameIndex].drawFence);
}

void Vulkan_RendererAPI::DrawFrame()
{
    if (vkWaitForFences(m_logicalDevice, 1, &m_frames[m_frameIndex].drawFence,
                        VK_TRUE, UINT64_MAX) != VK_SUCCESS)
    {
        LOG_ASSERT("VULKAN", "Failed to wait for draw fence");
    }

    uint32_t imageIndex = UINT32_MAX;

    VkResult acquireResult =
        vkAcquireNextImageKHR(m_logicalDevice, m_swapchain, UINT64_MAX,
                              m_frames[m_frameIndex].presentCompleteSemaphore,
                              VK_NULL_HANDLE, &imageIndex);

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
        .commandBuffer = m_frameCmdBuffers[m_frameIndex],
    };

    VkSubmitInfo2 submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                             .pNext = nullptr,
                             .flags = 0,
                             .waitSemaphoreInfoCount = 1,
                             .pWaitSemaphoreInfos = &waitSemaphoreSubmitInfo,
                             .commandBufferInfoCount = 1,
                             .pCommandBufferInfos = &cmdBufferSubmitInfo,
                             .signalSemaphoreInfoCount = 1,
                             .pSignalSemaphoreInfos =
                                 &signalSemaphoreSubmitInfo};

    vkResetFences(m_logicalDevice, 1, &m_frames[m_frameIndex].drawFence);
    vkQueueSubmit2(m_graphicQueue, 1, &submitInfo,
                   m_frames[m_frameIndex].drawFence);

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_frames[m_frameIndex].renderFinishedSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &imageIndex,
    };

    VkResult presentResult = vkQueuePresentKHR(m_graphicQueue, &presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        presentResult == VK_SUBOPTIMAL_KHR ||
        Application::GetApp().IsResizing())
    {
        RecreateSwapchain();
    }
    else if (presentResult != VK_SUCCESS)
    {
        LOG_ASSERT(presentResult == VK_TIMEOUT || presentResult == VK_NOT_READY,
                   "Failed to acquire next image to present");
    }
}

void Vulkan_RendererAPI::DrawTest()
{
    DrawFrame();
    m_frameIndex = (m_frameIndex++ % s_kMaxFrameInFlight);

    // vkQueueWaitIdle(m_graphicQueue);
}

void Vulkan_RendererAPI::RecreateSwapchain()
{
    vkQueueWaitIdle(m_graphicQueue);

    for (VkImageView &view : m_imageViews)
    {
        vkDestroyImageView(m_logicalDevice, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    VkSwapchainKHR oldSwapchain = m_swapchain;
    CreateSwapchain(oldSwapchain);

    vkDestroySwapchainKHR(m_logicalDevice, oldSwapchain, nullptr);

    CreateImageViews();
}
} // namespace VoidEngine
