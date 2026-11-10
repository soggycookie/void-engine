#pragma once
#include "deque.h"
#include "dynamic_array.h"
#include "flat_hash_map.h"
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

template <typename T>
void array_to_deque(Arena &arena, DynamicArray<T> &&array, Deque<T> &deque)
{
    deque.init(arena, 256);
    for (uint64_t idx = 0; idx < array.get_count(); ++idx)
    {
        deque.push_back(arena, std::move(array[idx]));
    }
    array.clear();
}

class ResourceSystem;

class Uint64Generator
{
private:
    std::mt19937_64 rng_;

public:
    explicit Uint64Generator() : rng_(std::random_device{}())
    {
    }

    uint64_t generate()
    {
        return rng_();
    }
};

class ResourceSystem
{
private:
    struct MetaCreation
    {
        StringView resource_path_;
        StringView ext_;

        MetaCreation() : resource_path_(), ext_()
        {
        }
    };

    Vallocator resource_vallocator_;
    // DynamicArray<IFileSource *> file_sources_;
    DynamicArray<String> sub_resource_paths;
    FlatHashMap<AssetId, ResourceData> resource_registry_;
    DynamicArray<Asset> assets_;
    String resource_path_;
    Uint64Generator asset_id_generator_;

private:
    friend class Application;

    ResourceSystem()
        : sub_resource_paths(), resource_path_(), assets_(), resource_registry_(), asset_id_generator_(),
          resource_vallocator_()
    {
    }

    NO_MOVE_ASSIGNMENT(ResourceSystem);
    NO_MOVE_CTOR(ResourceSystem);
    NO_COPY_ASSIGNMENT(ResourceSystem);
    NO_COPY_CTOR(ResourceSystem);

    void startup(StringView resource_path);
    void shutdown();

    void query_asset(const String &sub_resource_path);
    bool is_supported_ext(StringView ext) const;
    bool has_associated_metadata_file(StringView path) const;
    void create_metadata_file(MetaCreation &mt);
    void parse_resource(StringView path, StringView ext);

public:
    bool has(AssetId id);
    ResourceData *acquire(AssetId id);
    void release(AssetId id);

    AssetId get_free_asset_id();
};
} // namespace VoidEngine
