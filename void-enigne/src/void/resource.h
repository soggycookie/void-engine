#pragma once
#include "pch.h"
#include "common_type.h"
#include "graphic_buffer.h"
#include "graphic_shader.h"

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

        static ResourceType GetResourceType()
        {
            return ResourceType::SHADER;
        }

        const ResourceGUID& GetGUID()
        {
            return m_guid;
        }

    private:
        friend class ResourceSystem;
        friend class ResourceCache;

        ShaderResource(ResourceGUID guid);
        ~ShaderResource();

        void SetVertexShaderCompiledSrc(void* compiledSrc);
        void SetPixelShaderCompiledSrc(void* compiledSrc);

        void SubmitShaderToGpu();

    private:
        ResourceGUID m_guid;
        GraphicShader m_vertexShader;
        GraphicShader m_pixelShader;
    };

    class MaterialResource
    {
    public:
        MaterialResource(ResourceGUID guid, ResourceGUID shader);
        
        ~MaterialResource();

        static ResourceType GetResourceType()
        {
            return ResourceType::MATERIAL;
        }

        const ResourceGUID& GetGUID()
        {
            return m_guid;
        }

    private:
        ResourceGUID m_guid;
        ResourceGUID m_shader;

    };

    class InputLayoutResource
    {
    private:
        friend class ResourceSystem;

        InputLayoutResource();
        ~InputLayoutResource();


    private:
        void* m_nativeHandle;
    };

    class MeshResource
    {
    public:
        MeshResource(ResourceGUID guid, bool canCpuRead);

        MeshResource(ResourceGUID guid, 
                     Vertex* vertexData, size_t vertexCount, 
                     uint32_t* indexData, size_t indexCount, 
                     bool canCpuRead);

        static ResourceType GetResourceType()
        {
            return ResourceType::MESH;
        }

        const ResourceGUID& GetGUID()
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

        void SubmitMeshToGpu();

        void SetVertexDescriptor(VertexDescriptor* descriptors, size_t count);
    
    private:
        friend class ResourceCache;

        ~MeshResource();

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