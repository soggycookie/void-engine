#pragma once
#include "pch.h"
#include "common_type.h"
#include "graphic_buffer.h"

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <directxmath.h>

namespace VoidEngine
{
    enum class ResourceType
    {
        //GPU        
        SHADER,

        TEXTURE_2D,
        TEXTURE_3D,
        CUBEMAP,
        FONT,

        //CPU
        AUDIO,

        //COMPOSITE
        MESH,
        MATERIAL,
        UNKNOWN
    };

    struct Vertex
    {
        DirectX::XMVECTOR position;
        //DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;
    };

    enum class TypeFormat
    {
        FORMAT_R32G32B32A32_FLOAT,
        FORMAT_R32G32B32_FLOAT,
        FORMAT_R32G32_FLOAT,
        FORMAT_R32_FLOAT,

        FORMAT_R32G32B32A32_INT,
        FORMAT_R32G32B32_INT,
        FORMAT_R32G32_INT,
        FORMAT_R32_INT,

        FORMAT_R32G32B32A32_UINT,
        FORMAT_R32G32B32_UINT,
        FORMAT_R32G32_UINT,
        FORMAT_R32_UINT,
    };

    struct VertexDescriptor
    {
        std::string semantic;
        uint32_t semanticIndex;
        size_t offset;
        TypeFormat format;
    };

    const VertexDescriptor defaultVertexDesc[] = 
    {
        {"POSITION", 0, 0, TypeFormat::FORMAT_R32G32B32A32_FLOAT},
        {"TEXCOORD", 0, 16, TypeFormat::FORMAT_R32G32_FLOAT}
    };

    class ShaderResource
    {
    public:
        static ResourceType ResourceType()
        {
            return ResourceType::SHADER;
        }

    private:
        ResourceGUID m_guid;
    };

    class MaterialResource
    {
        ResourceGUID guid;
        ShaderResource* shader;

        static ResourceType ResourceType()
        {
            return ResourceType::MATERIAL;
        }
    };

    class MeshResource
    {
    public:
        MeshResource(ResourceGUID guid, bool canCpuRead = false)
            : m_guid(guid), m_vertexData(nullptr), m_vertexCount(0),
            m_indexData(nullptr), m_indexCount(0), 
            m_descriptor(nullptr), m_descriptorCount(0),
            m_vertexBuffer(nullptr, BufferType::VERTEX_BUFFER),
            m_indexBuffer(nullptr, BufferType::INDEX_BUFFER),
            m_canCpuRead(canCpuRead), m_isSetVertexDesc(false),
            m_isSubmitted(false)
        {
        }

        MeshResource(ResourceGUID guid, 
                     Vertex* vertexData, size_t vertexCount, 
                     uint32_t* indexData, size_t indexCount, 
                     bool canCpuRead = false)
            : m_guid(guid), m_vertexData(vertexData), m_vertexCount(vertexCount),
            m_indexData(indexData), m_indexCount(indexCount), 
            m_descriptor(nullptr), m_descriptorCount(0),
            m_vertexBuffer(nullptr, BufferType::VERTEX_BUFFER),
            m_indexBuffer(nullptr, BufferType::INDEX_BUFFER),
            m_canCpuRead(canCpuRead), m_isSetVertexDesc(false),
            m_isSubmitted(false)
        {
        }

        ~MeshResource()
        {
            Destroy();
        }

        static ResourceType ResourceType()
        {
            return ResourceType::MESH;
        }

        const ResourceGUID& GUID()
        {
            return m_guid;
        }

        void SetVertexData(Vertex* vertexData, size_t vertexCount)
        {
            m_vertexData = vertexData;
            m_vertexCount = vertexCount;
        }

        void SetIndexData(uint32_t* indexData, size_t indexCount)
        {
            m_indexData = indexData;
            m_indexCount = indexCount;
        }

        void SubmitMeshData();

        void SetVertexDescriptor(VertexDescriptor* descriptors, size_t count);

    private:
        friend class ResourceCache;

        void Destroy();

    private:
        ResourceGUID m_guid;

        Vertex* m_vertexData;
        size_t m_vertexCount;
        uint32_t* m_indexData;
        size_t m_indexCount;
        
        VertexDescriptor* m_descriptor;
        size_t m_descriptorCount;

        GraphicBuffer m_vertexBuffer;
        GraphicBuffer m_indexBuffer;

        bool m_canCpuRead;
        bool m_isSetVertexDesc;
        bool m_isSubmitted;
    };

}