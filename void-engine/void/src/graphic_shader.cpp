#include "graphic_shader.h"
#include "renderer.h"

namespace VoidEngine
{
        void GraphicShader::SubmitToGpu()
        {
            m_nativeHandle = Renderer::CreateShader(&m_compiledSrc, m_type);

        if(!m_nativeHandle)
        {
#ifdef VOID_DEBUG
            assert(0 && "[GraphicShader] Failed to submit shader!");
#else
            SIMPLE_LOG("[GraphicShader] Failed to submit shader!");
#endif
        }
        }

        void GraphicShader::Destroy()
        {
            Renderer::DestroyShader(*this);
        }

}