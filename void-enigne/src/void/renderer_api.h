#pragma once
#include "graphic_buffer.h"
#include "resource.h"

namespace VoidEngine
{
    class RendererAPI
    {
    public:
        virtual void Clear() = 0;
        virtual bool Init(int width, int height, void* outputWindow) = 0;
        virtual void Update() = 0;

        virtual void NewFrame() = 0;
        virtual void EndFrame() = 0;

        virtual void* GetContext() = 0;

        virtual void* CreateAndSubmitBuffer(void* const data, size_t byteSize, BufferType type) = 0;
        virtual void ReleaseBuffer(GraphicBuffer& buffer) = 0;

    };

}