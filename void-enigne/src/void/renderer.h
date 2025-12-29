#pragma once
#include "pch.h"
#include "renderer_api.h"
#include "graphic_buffer.h"

namespace VoidEngine
{
    class Window;

    class Renderer
    {
    public:

        Renderer() = delete;

        virtual ~Renderer() = default;

        static void Clear();

        static void* GetRendererAPIContext()
        {
            return s_rendererAPI->GetContext();
        }

        static void Update();

        static void NewFrame();
        static void EndFrame();
    
        static void* CreateAndSubmitBuffer(void* const data, size_t byteSize, BufferType type);
        //static void SubmitBufferData(const GraphicBuffer& buffer);
        static void ReleaseBuffer(GraphicBuffer& buffer);
    private:
        friend class Application;
    
        static bool SetGraphicAPI(GraphicAPI api);
        
        static void ShutDown()
        {
        }

        static void StartUp(Window* window)
        {
            if(!window)
            {
                assert(0 && "Window is null! [Renderer]");
            }

            s_window = window;
        }


        //High Level API

        //static void RenderStaticMesh();

    protected:
        static RendererAPI* s_rendererAPI;
        static Window* s_window;
        static GraphicAPI s_graphicAPI;
    };

}
