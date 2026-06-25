#pragma once
#include "graphic_buffer.h"
#include "graphic_shader.h"
#include "log.h"
#include "pch.h"

#include "math_utils.h"

namespace VoidEngine
{
enum class ResourceType : uint16_t
{
    // GPU
    SHADER,

    TEXTURE_2D,
    TEXTURE_3D,
    CUBEMAP,
    FONT,

    // CPU
    AUDIO,

    // COMPOSITE
    MESH,
    MATERIAL,
    UNKNOWN
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

static size_t HashVertexDesc(const VertexDescriptor *vd, uint32_t count)
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

constexpr VertexDescriptor defaultQuadVertexDesc[] = {
    {VertexSemantic::POSITION, 0, 0, TypeFormat::FORMAT_R32G32B32A32_FLOAT, 0},
    {VertexSemantic::TEXCOORD, 0, 0, TypeFormat::FORMAT_R32G32_FLOAT, 16}};

#define DEFAULT_VERTEX_DESC       defaultQuadVertexDesc
#define DEFAULT_VERTEX_DESC_COUNT 2

const size_t defaultVertexDescHash = HashVertexDesc(DEFAULT_VERTEX_DESC, DEFAULT_VERTEX_DESC_COUNT);

#define DEFAULT_VERTEX_DESC_HASH defaultVertexDescHash

using GUID = size_t;
constexpr GUID kInvalidGUID = 0;

static GUID GenerateGUID()
{
    static size_t guid = kInvalidGUID;
    return ++guid;
}

class ShaderResource
{
public:
    static ResourceType GetResourceType() { return ResourceType::SHADER; }

    const GUID &GetGUID() { return m_guid; }

    const GraphicShader &GetVertexShader() const { return m_vertexShader; }

    const GraphicShader &GetPixelShader() const { return m_pixelShader; }

private:
    friend class ResourceSystem;
    friend class ResourceRegistry;

    ShaderResource(GUID guid);
    ~ShaderResource();

    void SetVertexShaderCompiledSrc(void *compiledSrc);
    void SetPixelShaderCompiledSrc(void *compiledSrc);

    void SubmitShaderToGpu();

private:
    GUID m_guid;
    GraphicShader m_vertexShader;
    GraphicShader m_pixelShader;
};

class MaterialResource
{
public:
    MaterialResource(GUID guid, GUID shader);

    ~MaterialResource();

    static ResourceType GetResourceType() { return ResourceType::MATERIAL; }

    const GUID &GetGUID() { return m_guid; }

    const ShaderResource *GetShader() const { return m_shader; }

private:
    GUID m_guid;
    ShaderResource *m_shader;
};

template <typename T>
struct ResourceTypeTraits
{
    static const ResourceType type = ResourceType::UNKNOWN;
};

// template<> struct ResourceTypeTraits<MeshResource>
// {
//     static const ResourceType type = ResourceType::MESH;
// };

template <>
struct ResourceTypeTraits<ShaderResource>
{
    static const ResourceType type = ResourceType::SHADER;
};

template <>
struct ResourceTypeTraits<MaterialResource>
{
    static const ResourceType type = ResourceType::MATERIAL;
};

struct ResourceData
{
    void *rsc;
    const char *file;
    GUID guid;
    int32_t ref;
    ResourceType type;
    bool isLoaded;

    template <typename T>
    T *As()
    {
        static_assert(ResourceTypeTraits<T>::type == type, "Mismatch resource type!");
        LOG_ASSERT(rsc != nullptr, "Resource %d is null!", guid);
        return static_cast<T *>(rsc);
    }
};

template <typename T>
class ResourceHandle
{
public:
    ResourceHandle(ResourceHandle<T> &&handle)
    {
        m_guid = handle.m_guid;
        handle.m_guid = kInvalidGUID;
    }

    ResourceHandle(const ResourceHandle<T> &handle);

    T *Get();

    bool IsValid() const { return m_guid != kInvalidGUID; }

private:
    friend class ResourceRegistry;

    ResourceHandle(GUID guid);

private:
    GUID m_guid;
};

} // namespace VoidEngine
