#pragma once
#include "file_system.h"
#include "logger/log.h"
#include "pch.h"
#include "str.h"
namespace VoidEngine
{

using AssetId = uint64_t;
constexpr AssetId InvalidAssetId = 0;

class AssetIdGenerator
{
private:
    AssetId m_state;

public:
    explicit AssetIdGenerator(uint64_t seed) : m_state(seed)
    {
    }

    AssetId Generate()
    {
        AssetId z = (m_state += 0x9E3779B97F4A7C15ULL);

        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
};

enum ResourceType : uint8_t
{
    SHADER,
    TEXTURE,
    CUBEMAP,

    MATERIAL,
    MESH,
};

enum class IOState : uint8_t
{
    UNLOADED,
    PENDING,
    PROCESSING,
    LOADED,
    PENDING_CANCEL,
};

struct Asset
{
    AssetId id;
    String name;
    FS::File handle;
    uint32_t ref_count;
    bool open;
    IOState state;

    Asset(Asset &&other)
        : id(other.id), name(std::move(other.name)), handle(other.handle), ref_count(other.ref_count),
          open(other.open), state(other.state)
    {
    }
};

struct ResourceData
{
    uint64_t offset;
    uint32_t size;
    uint16_t asset_idx;
    ResourceType type;
    void *data;
    uint32_t ref_count;
};

} // namespace VoidEngine
