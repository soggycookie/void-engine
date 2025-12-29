#include "graphic_buffer.h"
#include "renderer.h"

namespace VoidEngine
{
    void GraphicBuffer::Submit(void* const data, size_t byteSize)
    {
        m_nativeHandle = Renderer::CreateAndSubmitBuffer(data, byteSize, m_bufferType);

        if(!m_nativeHandle)
        {
            assert(0 && "Failed to create and submit buffer! [GraphicBuffer]");
        }
    }
}