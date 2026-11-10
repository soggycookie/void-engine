#pragma once
#include "file_system.h"
#include "pch.h"
#include "str.h"
namespace VoidEngine
{

using AssetId = uint64_t;
constexpr AssetId INVALID_ASSET_ID = 0;

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
