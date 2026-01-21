#pragma once
#include "pch.h"

namespace VoidEngine
{
    enum class BufferType
    {
        VERTEX_BUFFER,
        INDEX_BUFFER,
        CONSTANT_BUFFER,
        UNKNOWN
    };

    class GraphicBuffer
    {
    public:
        GraphicBuffer(void* handle, BufferType type)
            : m_nativeHandle(handle), m_bufferType(type)
        {
        }

        GraphicBuffer(const GraphicBuffer& other) = delete;
        
        virtual ~GraphicBuffer() = default;

        GraphicBuffer& operator=(GraphicBuffer& other) = delete;
        GraphicBuffer& operator=(GraphicBuffer&& other) = delete;
        
        BufferType GetType() const
        {
            return m_bufferType;
        }


        void SubmitToGpu(void* const data, size_t byteSize);
        void Destroy();
    
    private:
#if defined (_WIN32) || defined(_MSC_VER)
        friend class D3D11_RendererAPI;
#endif
    
        template<typename T>
        T As() const
        {
            return static_cast<T>(m_nativeHandle);
        }

    private:
        void* m_nativeHandle;
        BufferType m_bufferType;
    };
}