#include "resource_system.h"
#include "file_system.h"
#include "str.h"

namespace VoidEngine
{

void ResourceSystem::startup(StringView path)
{
    resource_vallocator_.init(gib(64), mib(16), gib(64), mib(4));
    resource_path_.init(resource_vallocator_, path.get_length());
    resource_path_.assign(resource_vallocator_, path);
    sub_resource_paths.init(resource_vallocator_, MAX_FILE_SOURCE);

    Arena *scratch = t_scratches.get_scratch();
    ScopedArenaTemp temp;
    temp.start_scratch(scratch);
    DynamicArray<FS::DirectoryInfo> children;
    FS::find_children(*scratch, children, path);

    for (uint64_t idx = 0; idx < children.get_count(); ++idx)
    {
        FS::DirectoryInfo &info = children[idx];
        if (info.type == FS::DIRECTORY_TYPE)
        {
            String sub_resource;
            sub_resource.init(resource_vallocator_, info.abs_path.get_length());
            sub_resource.assign(resource_vallocator_, info.abs_path);
            sub_resource_paths.push_back(resource_vallocator_, std::move(sub_resource));
        }
    }

    for (uint64_t idx = 0; idx < sub_resource_paths.get_count(); ++idx)
    {
        query_asset(sub_resource_paths[idx]);
    }

    temp.end_scratch();
}

void ResourceSystem::shutdown()
{
    for (uint64_t idx = 0; idx < assets_.get_count(); ++idx)
    {
        Asset &asset = assets_[idx];
        if (asset.open)
        {
            FS::close(asset.handle);
        }
    }
    resource_registry_.destroy(resource_vallocator_);
    assets_.destroy(resource_vallocator_);
    for (uint64_t idx = 0; idx < sub_resource_paths.get_count(); ++idx)
    {
        sub_resource_paths[idx].destroy(resource_vallocator_);
    }
    sub_resource_paths.destroy(resource_vallocator_);
    resource_path_.destroy(resource_vallocator_);
    resource_vallocator_.destroy();
}

AssetId ResourceSystem::get_free_asset_id()
{
    AssetId asset_id = 0;
    do
    {
        asset_id = asset_id_generator_.generate();
    } while (has(asset_id));
    return asset_id;
}

ResourceData *ResourceSystem::acquire(AssetId id)
{
    return nullptr;
}

void ResourceSystem::release(AssetId id)
{
}

bool ResourceSystem::has(AssetId id)
{
    return resource_registry_.contain_key(id);
}

void ResourceSystem::query_asset(const String &sub_resource_path)
{
    ScopedArenaTemp temp_1;
    Arena *scratch = t_scratches.get_scratch();
    temp_1.start_scratch(scratch);

    DynamicArray<FS::DirectoryInfo> dir_children;
    FS::find_children(*scratch, dir_children, sub_resource_path);

    Deque<MetaCreation> meta_creation_deque;
    meta_creation_deque.init(*scratch, 256);

    Deque<FS::DirectoryInfo> processing_resource_directories;
    array_to_deque(*scratch, std::move(dir_children), processing_resource_directories);

    while (!processing_resource_directories.is_empty())
    {
        FS::DirectoryInfo &info = processing_resource_directories.front();
        if (info.type == FS::FILE_TYPE)
        {
            StringView ext = FS::get_file_ext(info.path_name);
            if (is_supported_ext(ext) && info.meta.size > 0)
            {
                if (has_associated_metadata_file(info.abs_path))
                {
                    // parse the meta file
                    parse_resource(info.abs_path, ext);
                }
                else
                {
                    // push into a queue, create meta later so we can compare guid to avoid collision
                    MetaCreation meta_creation;
                    meta_creation.ext_ = ext;
                    meta_creation.resource_path_ = info.abs_path;
                    meta_creation_deque.push_back(*scratch, meta_creation);
                }
            }
        }
        else
        {
            DynamicArray<FS::DirectoryInfo> sub_folder_children;
            FS::find_children(*scratch, sub_folder_children, info.abs_path);
            for (uint64_t idx = 0; idx < sub_folder_children.get_count(); ++idx)
            {
                processing_resource_directories.push_back(*scratch, std::move(sub_folder_children[idx]));
            }
            sub_folder_children.clear();
        }
        processing_resource_directories.pop_front();
    }

    while (!meta_creation_deque.is_empty())
    {
        MetaCreation &mt = meta_creation_deque.front();
        create_metadata_file(mt);
        parse_resource(mt.resource_path_, mt.ext_);
        meta_creation_deque.pop_front();
    }
    temp_1.end_scratch();
}

void ResourceSystem::create_metadata_file(MetaCreation &mt)
{
    ScopedArenaTemp temp;
    Arena *scratch = t_scratches.get_scratch();
    temp.start_scratch(scratch);

    String meta_file;
    meta_file.init(*scratch, mt.resource_path_.get_length() + META_FILE_EXT_LENGTH + 1);
    meta_file.append(*scratch, mt.resource_path_);
    meta_file.append(*scratch, u8".");
    meta_file.append(*scratch, META_FILE_EXT);

    FS::create(meta_file);
    FS::File meta_handle = FS::open(meta_file, FS::WRITE_ACCESS_FLAG | FS::TRUNCATE_FLAG);
    AssetId guid = get_free_asset_id();
    std::cout << "Write id: " << guid << std::endl;
    FS::write(meta_handle, &guid, sizeof(guid));
    FS::close(meta_handle);
    temp.end_scratch();
}

void ResourceSystem::parse_resource(StringView path, StringView ext)
{
    if (ext == u8"jpg")
    {
        std::cout << "Parse JPEG: " << path << std::endl;
    }
    else
    {
        std::cout << "Parse not supported type " << ext << ": " << path << std::endl;
    }
}

bool ResourceSystem::is_supported_ext(StringView ext) const
{
    for (uint64_t idx = 0; idx < SUPPORTED_EXT_COUNT; ++idx)
    {
        if (std::strcmp(RCAST(const char *, ext.data()), RCAST(const char *, SUPPORTED_EXTS[idx])) == 0)
        {
            return true;
        }
    }
    return false;
}

bool ResourceSystem::has_associated_metadata_file(StringView path) const
{
    ScopedArenaTemp temp;
    Arena *scratch = t_scratches.get_scratch();
    temp.start_scratch(scratch);

    String meta_path;
    meta_path.init(*scratch, path.get_length() + META_FILE_EXT_LENGTH + 1);
    meta_path.append(*scratch, path);
    meta_path.append(*scratch, u8".");
    meta_path.append(*scratch, META_FILE_EXT);
    bool file_exists = FS::is_existing(meta_path);

    temp.end_scratch();
    return file_exists;
}
} // namespace VoidEngine
