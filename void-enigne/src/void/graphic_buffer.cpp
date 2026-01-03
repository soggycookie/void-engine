#include "graphic_buffer.h"
#include "renderer.h"

namespace VoidEngine
{
    void GraphicBuffer::SubmitToGpu(void* const data, size_t byteSize)
    {
        m_nativeHandle = Renderer::CreateAndSubmitBuffer(data, byteSize, m_bufferType);

        if(!m_nativeHandle)
        {
#ifdef VOID_DEBUG
            assert(0 && "[GraphicBuffer] Failed to submit buffer!");
#else
            SIMPLE_LOG("[GraphicBuffer] Failed to submit buffer!");
#endif
        }
    }

     void GraphicBuffer::Destroy()
     {
        Renderer::DestroyBuffer(*this);        
     }
}