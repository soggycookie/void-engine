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

    enum class TypeFormat : uint16_t
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

    enum class VertexSemantic : uint16_t
    {
        POSITION,
        TEXCOORD,
    };

    struct VertexDescriptor
    {
        VertexSemantic semanticName;
        uint16_t semanticIndex;
        uint16_t inputSlot;
        TypeFormat format;
        uint32_t offset;
    };

    static size_t HashVertexDesc(const VertexDescriptor* vd, uint32_t count)
    {
        // 64-bit FNV-1a
        size_t hash = 1469598103934665603ULL;

        auto mix = [&](size_t v)
        {
            hash ^= v;
            hash *= 1099511628211ULL;
        };

        for (uint32_t i = 0; i < count; ++i)
        {
            mix(static_cast<size_t>(vd[i].semanticName));
            mix(static_cast<size_t>(vd[i].semanticIndex));
            mix(static_cast<size_t>(vd[i].inputSlot));
            mix(static_cast<size_t>(vd[i].offset));
            mix(static_cast<size_t>(vd[i].format));
        }

        return hash;
    }

    constexpr VertexDescriptor defaultQuadVertexDesc[] = 
    {
        {VertexSemantic::POSITION, 0, 0, TypeFormat::FORMAT_R32G32B32A32_FLOAT, 0},
        {VertexSemantic::TEXCOORD, 0, 0, TypeFormat::FORMAT_R32G32_FLOAT, 16}
    };


#define DEFAULT_VERTEX_DESC defaultQuadVertexDesc
#define DEFAULT_VERTEX_DESC_COUNT 2
    
    const size_t defaultVertexDescHash = HashVertexDesc(DEFAULT_VERTEX_DESC, DEFAULT_VERTEX_DESC_COUNT);

#define DEFAULT_VERTEX_DESC_HASH defaultVertexDescHash

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

        const GraphicShader& GetVertexShader() const
        {
            return m_vertexShader;
        }
        
        const GraphicShader& GetPixelShader() const
        {
            return m_pixelShader;
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

        const ShaderResource* GetShader() const
        {
            return m_shader;
        }

    private:
        ResourceGUID m_guid;
        ShaderResource* m_shader;
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

        uint32_t GetIndexCount() const
        {
            return m_indexCount;
        }
        
        uint32_t GetVertexCount() const
        {
            return m_vertexCount;
        }

        void SubmitMeshToGpu();

        void SetVertexDesc(VertexDescriptor* descriptors, size_t count);
        
        const GraphicBuffer& GetVertexGraphicBuffer() const
        {
            return m_vertexBuffer;
        } 
        
        const GraphicBuffer& GetIndexGraphicBuffer() const
        {
            return m_indexBuffer;
        } 

        const VertexDescriptor* GetVertexDesc() const
        {
            return m_descriptor;
        }

        size_t GetVertexDescCount() const
        {
            return m_descriptorCount;
        }

        size_t GetVertexDescHash() const
        {
            return m_descriptorHash;
        }

    private:
        friend class ResourceCache;

        ~MeshResource();



    private:
        ResourceGUID m_guid;

        Vertex* m_vertexData;
        uint32_t m_vertexCount;
        uint32_t* m_indexData;
        uint32_t m_indexCount;
        
        const VertexDescriptor* m_descriptor;
        size_t m_descriptorCount;
        size_t m_descriptorHash;

        GraphicBuffer m_vertexBuffer;
        GraphicBuffer m_indexBuffer;

        bool m_canCpuRead;
        bool m_isSetVertexDesc;
        bool m_isSubmitted;
    };

}