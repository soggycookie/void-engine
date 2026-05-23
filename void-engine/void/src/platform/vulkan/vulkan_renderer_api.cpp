#include "vulkan_renderer_api.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

namespace VoidEngine
{
bool Vulkan_RendererAPI::Init(int width, int height, void *outputWindow)
{
    CreateInstance();
#ifdef VOID_DEBUG
    SetUpDebugMessenger();
#endif // VOID_DEBUG
    SelectPhysicalDevice();
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
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            m_instance, "vkDestroyDebugUtilsMessengerEXT");

    if (vkDestroyDebugUtilsMessengerEXT)
    {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }
    else
    {
        std::cerr << "Vulkan failed to acquire debug messenger destruction "
                     "extended function!"
                  << std::endl;

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

    for (size_t idx = 0; idx < requiredExts.size(); ++idx)
    {
        if (!IsExtAvailable(extProperties, requiredExts[idx]))
        {
            std::cerr << "Vulkan extension " << requiredExts[idx]
                      << " is not available!" << std::endl;
            throw std::runtime_error("[VULKAN] No support for " +
                                     std::string(requiredExts[idx]));
        }
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

    for (size_t idx = 0; idx < requiredLayers.size(); ++idx)
    {
        if (!IsLayerAvailable(layerProperties, requiredLayers[idx]))
        {
            std::cerr << "Vulkan layer " << requiredLayers[idx]
                      << " is not available!" << std::endl;

            throw std::runtime_error("[VULKAN] No support for " +
                                     std::string(requiredLayers[idx]));
        }
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
        std::cerr << "Vulkan instance creation failed!" << std::endl;

        throw std::runtime_error("[VULKAN] Failed to create Vulkan instance!");
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{

    std::cerr << "Vulkan: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void Vulkan_RendererAPI::SetUpDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
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
            std::cerr << "Vulkan debug messenger creation failed!" << std::endl;

            throw std::runtime_error(
                "[VULKAN] Failed to create debug messenger!");
        }
    }
    else
    {
        std::cerr << "Vulkan failed to acquire debug messenger creation "
                     "extended function!"
                  << std::endl;

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

    if (deviceCount)
    {
        std::cerr << "Failed to find GPU with vulkan support!" << std::endl;

        throw std::runtime_error("[VULKAN] No physical devices found!");
    }

    const std::vector<const char *> requiredDeviceExtensions = {
        "VK_KHR_swapchain"};

    for (size_t idx = 0; idx < devices.size(); ++idx)
    {
        const VkPhysicalDevice &device = devices[idx];

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
        bool supportVulkan1_3 =
            property.properties.apiVersion >= VK_API_VERSION_1_3;

        bool supportGraphics = false;

        bool supportsGraphics =
            std::ranges::any_of(queueFamilyProps,
                                [](const VkQueueFamilyProperties2 &p)
                                {
                                    return (p.queueFamilyProperties.queueFlags &
                                            VK_QUEUE_GRAPHICS_BIT) > 0;
                                });

        uint32_t deviceExtCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &deviceExtCount,
                                             nullptr);
        std::vector<VkExtensionProperties> deviceExts(deviceExtCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &deviceExtCount,
                                             deviceExts.data());

        bool supportsExts = std::ranges::all_of(
            requiredDeviceExtensions,
            [&](const char *ext)
            {
                return std::ranges::any_of(
                    deviceExts, [&](const VkExtensionProperties &p)
                    { return std::strcmp(ext, p.extensionName) == 0; });
            });

        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT
            extendedDynamicStateFeatures{
                .sType =
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
                .pNext = nullptr};

        VkPhysicalDeviceVulkan13Features vulkan13Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .pNext = &extendedDynamicStateFeatures};

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &vulkan13Features};

        vkGetPhysicalDeviceFeatures2(device, &features2);

        bool supportsRequiredFeatures =
            vulkan13Features.dynamicRendering &&
            extendedDynamicStateFeatures.extendedDynamicState;

        if (isDiscreteDevice && supportVulkan1_3 && supportGraphics &&
            supportsExts && supportsRequiredFeatures)
        {
            m_physicalDevice = device;
            break;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("[VULKAN] No suitable physical device found!");
    }
}

} // namespace VoidEngine
