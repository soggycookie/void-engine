#include "resource.h"
#include "void/renderer.h"

namespace VoidEngine
{

    void MeshResource::Destroy()
    {
        Renderer::ReleaseBuffer(m_vertexBuffer);
        Renderer::ReleaseBuffer(m_indexBuffer);
    }

    void MeshResource::SubmitMeshData()
    {
        if(m_isSubmitted)
        {
            SIMPLE_LOG("Graphic Buffer already submitted! [MeshResource]");
            return;
        }

        m_isSubmitted = true;
        m_vertexBuffer.Submit(m_vertexData, sizeof(Vertex) * m_vertexCount);
        m_indexBuffer.Submit(m_indexData, sizeof(uint32_t) * m_indexCount);
    }

    void MeshResource::SetVertexDescriptor(VertexDescriptor* descriptors, size_t count)
    {
            
    }


}