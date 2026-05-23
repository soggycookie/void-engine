#pragma once
#include "graphic_buffer.h"
#include "graphic_shader.h"
#include "pch.h"
#include "renderer_api.h"

namespace VoidEngine
{
class Window;

class Renderer
{
public:
    Renderer() = delete;

    virtual ~Renderer() = default;

    static void *GetRendererAPIContext()
    {
        return s_pRendererAPI->GetContext();
    }

    static void NewFrame();
    static void EndFrame();

    static void *CreateAndSubmitBuffer(void *const data, size_t byteSize,
                                       BufferType type);
    // static void SubmitBufferData(const GraphicBuffer& buffer);
    static void DestroyBuffer(GraphicBuffer &buffer);

    static void *CompileShader(const wchar_t *file, const char *entry,
                               const char *target);
    static void *CreateShader(void **compiledSrc, ShaderType type);
    static void DestroyShader(GraphicShader &shader);

    static void Draw(MeshResource *mesh, MaterialResource *material);

private:
    friend class Application;

    static bool SetGraphicAPI(GraphicAPI api);

    static void ShutDown() { s_pRendererAPI->Shutdown(); }

    static void StartUp(Window *window)
    {
        if (!window)
        {
            assert(0 && "Window is null! [Renderer]");
        }

        s_pWindow = window;
    }

    // High Level API

    // static void RenderStaticMesh();

protected:
    static RendererAPI *s_pRendererAPI;
    static Window *s_pWindow;
    static GraphicAPI s_graphicAPI;
};

} // namespace VoidEngine
