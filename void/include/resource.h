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
    UNKNOWN_RESOURCE_TYPE = 0x00,
    SHADER_RESOURCE_TYPE = 0x01,
    TEXTURE_RESOURCE_TYPE = 0x02,
    CUBEMAP_TYPE = 0x03,
    MATERIAL_TYPE = 0x04,
    MESH_TYPE = 0x05,
};

enum IOState : uint8_t
{
    STATE_UNLOADED,
    STATE_PENDING,
    STATE_PROCESSING,
    STATE_LOADED,
    STATE_PENDING_CANCEL,
};

struct Asset
{
    String file_name_;
    FS::File file_;
    uint32_t resource_ref_count_;
    bool open_;

    Asset() : file_name_(), file_(FS::INVALID_FILE), open_(false), resource_ref_count_(0)
    {
    }

    Asset(Asset &&other)
        : file_name_(std::move(other.file_name_)), file_(other.file_),
          resource_ref_count_(other.resource_ref_count_), open_(other.open_)
    {
    }
};

struct ResourceData
{
    uint64_t offset_;
    uint32_t size_;
    uint16_t asset_idx_;
    ResourceType type_;
    IOState state_;
    uint32_t ref_count_;
    void *data_;
};

} // namespace VoidEngine
