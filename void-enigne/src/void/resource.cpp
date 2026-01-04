#include "resource.h"
#include "resource_system.h"

namespace VoidEngine
{
    MeshResource::MeshResource(ResourceGUID guid, bool canCpuRead)
        : m_guid(guid), m_vertexData(nullptr), m_vertexCount(0),
        m_indexData(nullptr), m_indexCount(0),
        m_descriptor(nullptr), m_descriptorCount(0),
        m_vertexBuffer(nullptr, BufferType::VERTEX_BUFFER),
        m_indexBuffer(nullptr, BufferType::INDEX_BUFFER),
        m_canCpuRead(canCpuRead), m_isSetVertexDesc(false),
        m_isSubmitted(false)
    {
    }

    MeshResource::MeshResource(ResourceGUID guid,
                               Vertex* vertexData, size_t vertexCount,
                               uint32_t* indexData, size_t indexCount,
                               bool canCpuRead)
        : m_guid(guid), m_vertexData(vertexData), m_vertexCount(vertexCount),
        m_indexData(indexData), m_indexCount(indexCount),
        m_descriptor(nullptr), m_descriptorCount(0),
        m_vertexBuffer(nullptr, BufferType::VERTEX_BUFFER),
        m_indexBuffer(nullptr, BufferType::INDEX_BUFFER),
        m_canCpuRead(canCpuRead), m_isSetVertexDesc(false),
        m_isSubmitted(false)
    {
    }

    MeshResource::~MeshResource()
    {
        m_vertexBuffer.Destroy();
        m_indexBuffer.Destroy();
    }

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

    ShaderResource::ShaderResource(ResourceGUID guid)
        : m_guid(guid),
        m_vertexShader(ShaderType::VERTEX),
        m_pixelShader(ShaderType::PIXEL)
    {
    }

    ShaderResource::~ShaderResource()
    {
        m_vertexShader.Destroy();
        m_pixelShader.Destroy();
    }

    void ShaderResource::SetVertexShaderCompiledSrc(void* compiledSrc)
    {
        m_vertexShader.SetCompiledSrc(compiledSrc);
    }

    void ShaderResource::SetPixelShaderCompiledSrc(void* compiledSrc)
    {
        m_pixelShader.SetCompiledSrc(compiledSrc);
    }

    void ShaderResource::SubmitShaderToGpu()
    {
        m_vertexShader.SubmitToGpu();
        m_pixelShader.SubmitToGpu();
    }



    MaterialResource::MaterialResource(ResourceGUID guid, ResourceGUID shader)
        : m_guid(guid), m_shader(shader)
    {
        auto rsrc = ResourceSystem::Acquire<ShaderResource>(m_shader);

        if(!rsrc)
        {
            std::cerr << "[Resource.Material] Shader does not exist!" << std::endl;
            //acquire default shader
        }
    }

    MaterialResource::~MaterialResource()
    {
        ResourceSystem::Release<ShaderResource>(m_shader);
    }

}