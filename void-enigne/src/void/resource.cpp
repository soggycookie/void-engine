#include "resource.h"

namespace VoidEngine
{
    void MeshResource::SubmitMeshToGpu()
    {
        if(m_isSubmitted)
        {
            SIMPLE_LOG("[Resource.Mesh] Graphic Buffer already submitted!");
            return;
        }

        m_isSubmitted = true;
        m_vertexBuffer.SubmitToGpu(m_vertexData, sizeof(Vertex) * m_vertexCount);
        m_indexBuffer.SubmitToGpu(m_indexData, sizeof(uint32_t) * m_indexCount);
    }

    void MeshResource::SetVertexDescriptor(VertexDescriptor* descriptors, size_t count)
    {
            
    }

    void ShaderResource::SubmitShaderToGpu()
    {
        m_vertexShader.SubmitToGpu();
        m_pixelShader.SubmitToGpu();
    }

}