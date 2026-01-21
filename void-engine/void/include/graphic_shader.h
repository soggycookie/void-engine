#pragma once
#include "pch.h"

namespace VoidEngine
{
    enum class ShaderType
    {
        VERTEX,
        PIXEL
    };

    class GraphicShader
    {
    public:

        GraphicShader(ShaderType type, void* compiledSrc = nullptr)
            : m_type(type), m_compiledSrc(compiledSrc), m_nativeHandle(nullptr)
        {
        }
        
        void SetCompiledSrc(void* compiledSrc)
        {
            m_compiledSrc = compiledSrc;
        }

        void SubmitToGpu();
        void Destroy();

        GraphicShader& operator=(GraphicShader&& shader) = delete;
        GraphicShader& operator=(const GraphicShader& shader) = delete;

        ShaderType GetShaderType()
        {
            return m_type;
        }

    private:
#if defined (_WIN32) || defined(_MSC_VER)
        friend class D3D11_RendererAPI;
#endif
        template<typename T>
        T As() const
        {
            return static_cast<T>(m_nativeHandle);
        }
        
        template<typename T>
        T CompiledSrcAs() const
        {
            return static_cast<T>(m_compiledSrc);
        }


    private:
        void* m_compiledSrc;
        void* m_nativeHandle;
        ShaderType m_type;
    };
}