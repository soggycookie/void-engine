#pragma once
#include "deque.h"
#include "dynamic_array.h"
#include "flat_hash_map.h"
#include "resource.h"
#include "str.h"

namespace VoidEngine
{

struct Reader
{
    const char *p_;
    const char *end_;

    void init(const void *buf, uint64_t len);
    void read_uint64(uint64_t &out);
    void read_byte(char &out);
    uint64_t reader_remaining();
};

struct Writer
{
    char *p_;
    const char *end_;
    uint64_t written_;

    void init(void *buf, uint64_t len);
    void write_uint64(uint64_t data);
    void write_byte(char data);
    uint64_t writer_remaining();
};

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
    Vallocator vallocator_;
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
          vallocator_()
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
    bool has_associated_meta_file(StringView abs_rpath) const;
    void create_meta_for_sole_file(StringView abs_rpath);
    void parse_resource(StringView abs_rpath);
    ResourceType ext_to_resource_type(StringView ext) const;
    bool is_bundle_file(StringView ext) const;
    String get_sole_meta_file_path(Arena &arena, StringView abs_rpath);

    //meta file reading funcs
    // a = parse_type_header()
    // open meta file 
    // Asset asset = ...
    // push into array
    //
    // if a == 1
    // read guid, resource type + extra info -> resource data
    // close meta file
    // else
    //  while (parse_bundle)
    //  out offset
    //  jump to offset -> read guid resource type +extra info -> resource data
    //  asset data_ref_count ++
    //   not close file because bundle file has no meta

public:
    bool has(AssetId id);
    ResourceData *acquire(AssetId id);
    void release(AssetId id);

    AssetId get_free_asset_id();
};
} // namespace VoidEngine
