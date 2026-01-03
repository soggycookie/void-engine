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

        virtual ~GraphicBuffer() = default;

        GraphicBuffer& operator=(GraphicBuffer& buffer) = delete;
        GraphicBuffer& operator=(GraphicBuffer&& buffer) = delete;
        
        BufferType GetType() const
        {
            return m_bufferType;
        }

        template<typename T>
        T As()
        {
            return static_cast<T>(m_nativeHandle);
        }

        void SubmitToGpu(void* const data, size_t byteSize);
        void Destroy();

    private:
        void* m_nativeHandle;
        BufferType m_bufferType;
    };
}