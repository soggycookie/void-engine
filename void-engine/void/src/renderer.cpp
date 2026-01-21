#include "renderer.h"
#include "window.h"
#include "platform/d3d11/d3d11_renderer_api.h"

namespace VoidEngine
{
    RendererAPI* Renderer::s_rendererAPI = nullptr;
    GraphicAPI Renderer::s_graphicAPI = GraphicAPI::UNKNOWN;
    Window* Renderer::s_window = nullptr;
    
    void Renderer::NewFrame()
    {
        s_rendererAPI->NewFrame();
    }
    
    void Renderer::EndFrame()
    {
        s_rendererAPI->EndFrame();
    }

    bool Renderer::SetGraphicAPI(GraphicAPI api)
    {
        if(!s_window)
        {
            SIMPLE_LOG("Renderer's window is uninitialized!");
            return false;
        }

        switch(api)
        {
            case GraphicAPI::D3D11:
            {
                if(s_graphicAPI != api)
                {
                    //TODO: free old renderer api
                    void* rendererAddr = GlobalPersistantAllocator::Get().Alloc(sizeof(D3D11_RendererAPI), alignof(D3D11_RendererAPI));
                    s_rendererAPI = new (rendererAddr) D3D11_RendererAPI();
                    auto dimension = s_window->GetDimension();
                    s_rendererAPI->Init(dimension.width, dimension.height, s_window->GetDisplayWindow());
                    s_graphicAPI = api;
                }
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

    void* Renderer::CreateAndSubmitBuffer(void* const data, size_t byteSize, BufferType type)
    {
        return s_rendererAPI->CreateAndSubmitBuffer(data, byteSize, type);
    }

    void Renderer::DestroyBuffer(GraphicBuffer& buffer)
    {
        s_rendererAPI->DestroyBuffer(buffer);
    }

    ////////////////////    Shader

    void* Renderer::CompileShader(const wchar_t* file, const char* entry, const char* target)
    {
        return s_rendererAPI->CompileShader(file, entry, target);
    }
    
    void* Renderer::CreateShader(void** compiledSrc, ShaderType type)
    {
        return s_rendererAPI->CreateShader(compiledSrc, type);
    }

    void Renderer::DestroyShader(GraphicShader& shader)
    {
        s_rendererAPI->DestroyShader(shader);
    }

    void Renderer::Draw(MeshResource* mesh, MaterialResource* material)
    {
        s_rendererAPI->Draw(mesh, material);
    }
}