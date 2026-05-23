#include "renderer.h"
#include "common_type.h"
#include "platform/d3d11/d3d11_renderer_api.h"
#include "platform/vulkan/vulkan_renderer_api.h"
#include "window.h"

namespace VoidEngine
{
RendererAPI *Renderer::s_pRendererAPI = nullptr;
GraphicAPI Renderer::s_graphicAPI = GraphicAPI::UNKNOWN;
Window *Renderer::s_pWindow = nullptr;

void Renderer::NewFrame() { s_pRendererAPI->NewFrame(); }

void Renderer::EndFrame() { s_pRendererAPI->EndFrame(); }

bool Renderer::SetGraphicAPI(GraphicAPI api)
{
    if (!s_pWindow)
    {
        SIMPLE_LOG("Renderer's window is uninitialized!");
        return false;
    }

    if (s_graphicAPI != GraphicAPI::UNKNOWN)
    {
        // Create new API's objs and free old one
    }

    switch (api)
    {
    case GraphicAPI::D3D11:
    {
        s_pRendererAPI = new D3D11_RendererAPI();
        auto dimension = s_pWindow->GetDimension();
        s_pRendererAPI->Init(dimension.width, dimension.height,
                             s_pWindow->GetDisplayWindow());
        s_graphicAPI = api;
        return true;
    }
    case GraphicAPI::VULKAN:
    {

        s_pRendererAPI = new Vulkan_RendererAPI();
        auto dimension = s_pWindow->GetDimension();
        s_pRendererAPI->Init(dimension.width, dimension.height,
                             s_pWindow->GetDisplayWindow());
        s_graphicAPI = api;
        return true;
    }
    default:
    {
        return false;
    }
    }

    return false;
}

////////////////////    Buffer

void *Renderer::CreateAndSubmitBuffer(void *const data, size_t byteSize,
                                      BufferType type)
{
    return s_pRendererAPI->CreateAndSubmitBuffer(data, byteSize, type);
}

void Renderer::DestroyBuffer(GraphicBuffer &buffer)
{
    s_pRendererAPI->DestroyBuffer(buffer);
}

////////////////////    Shader

void *Renderer::CompileShader(const wchar_t *file, const char *entry,
                              const char *target)
{
    return s_pRendererAPI->CompileShader(file, entry, target);
}

void *Renderer::CreateShader(void **compiledSrc, ShaderType type)
{
    return s_pRendererAPI->CreateShader(compiledSrc, type);
}

void Renderer::DestroyShader(GraphicShader &shader)
{
    s_pRendererAPI->DestroyShader(shader);
}

void Renderer::Draw(MeshResource *mesh, MaterialResource *material)
{
    s_pRendererAPI->Draw(mesh, material);
}
} // namespace VoidEngine
