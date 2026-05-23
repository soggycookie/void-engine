#pragma once
#include "pch.h"
#include "renderer_api.h"

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS

#ifdef VOID_WIN32
// #define
#else
#endif // VOID_WIN32

#include <vulkan/vulkan.h>

namespace VoidEngine
{

class Vulkan_RendererAPI : public RendererAPI
{
public:
    Vulkan_RendererAPI() = default;
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

private:
    void CreateInstance();
    void SetUpDebugMessenger();
    void SelectPhysicalDevice();

private:
    VkInstance m_instance;
    VkPhysicalDevice m_physicalDevice;
#ifdef VOID_DEBUG
    VkDebugUtilsMessengerEXT m_debugMessenger;
#endif // VOID_DEBUG
};
} // namespace VoidEngine
