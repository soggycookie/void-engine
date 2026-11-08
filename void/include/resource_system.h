#pragma once
#include "dynamic_array.h"
#include "flat_hash_map.h"
#include "deque.h"
#include "resource.h"
#include "str.h"

namespace VoidEngine
{

constexpr uint32_t MAX_FILE_SOURCE = 16;

constexpr const char8_t *SUPPORTED_EXTS[] = {
    u8"jpg",
    u8"png",
    u8"txt",
};

constexpr const char8_t *META_FILE_EXT = u8"meta";
constexpr uint64_t META_FILE_EXT_LENGTH = 4;

constexpr uint64_t SUPPORTED_EXT_COUNT = sizeof(SUPPORTED_EXTS) / sizeof(SUPPORTED_EXTS[0]);

template<typename T>
void array_to_deque(Arena &arena, DynamicArray<T>&& array, Deque<T> &deque)
{
    deque.init(arena, 256);
    for(uint64_t idx = 0; idx < array.get_count(); ++idx)
    {
        deque.push_back(arena, std::move(array[idx]));
    }
    array.clear();
}

class IFileSource
{
protected:
    String sub_resource_path_;

public:
    IFileSource() : sub_resource_path_()
    {
    }

    NO_COPY_ASSIGNMENT(IFileSource);
    NO_COPY_CTOR(IFileSource);

    void init(Vallocator &vallocator, const String &path);
    void destroy(Vallocator &vallocator);

    virtual void query_asset() = 0;
};

class LooseFileSource : public IFileSource
{
public:
    LooseFileSource() : IFileSource()
    {
    }

    void query_asset() override;

private:
    bool is_supported_ext(StringView ext) const;
    bool has_associated_metadata_file(StringView path) const;
    void create_metadata_file(const char8_t *path);
};

class ResourceSystem
{
private:
    DynamicArray<IFileSource *> file_sources_;
    FlatHashMap<AssetId, ResourceData> resource_registry_;
    DynamicArray<Asset> assets_;
    String resource_path_;

private:
    friend class Application;

    ResourceSystem() : file_sources_(), resource_path_(), assets_(), resource_registry_()
    {
    }

    NO_MOVE_ASSIGNMENT(ResourceSystem);
    NO_MOVE_CTOR(ResourceSystem);
    NO_COPY_ASSIGNMENT(ResourceSystem);
    NO_COPY_CTOR(ResourceSystem);

    void startup(Vallocator &vallocator, StringView resource_path);
    void shutdown(Vallocator &vallocator);

    IFileSource *create_file_source(Vallocator &vallocator, const String &path, bool loose);

public:
    bool has(AssetId id);
    ResourceData *acquire(AssetId id);
    void release(AssetId id);
};
} // namespace VoidEngine
