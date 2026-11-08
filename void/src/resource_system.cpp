#include "resource_system.h"
#include "file_system.h"
#include "str.h"

namespace VoidEngine
{
void IFileSource::init(Vallocator &vallocator, const String &path)
{
    sub_resource_path_.init(vallocator, path.get_length());
    sub_resource_path_.assign(vallocator, path);
    query_asset();
}

void IFileSource::destroy(Vallocator &vallocator)
{
    sub_resource_path_.destroy(vallocator);
}

struct MetaCreation
{
    String resource_path_;
    StringView ext_;

    MetaCreation() : resource_path_(), ext_()
    {
    }

    MetaCreation(MetaCreation &&other)
        : resource_path_(std::move(other.resource_path_)), ext_(other.ext_)
    {
    }
};

void LooseFileSource::query_asset()
{
    ScopedArenaTemp temp_1;
    Arena *scratch = t_scratches.get_scratch();
    temp_1.start_scratch(scratch);

    DynamicArray<FS::DirectoryInfo> dir_children;
    FS::find_children(*scratch, dir_children, sub_resource_path_.data());

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
                }
                else
                {
                    // push into a queue, create meta later so we can compare guid to avoid collision
                    MetaCreation meta_creation;
                    meta_creation.resource_path_.init(*scratch, info.abs_path.get_length() +
                                                                    META_FILE_EXT_LENGTH + 1);
                    meta_creation.resource_path_.append(*scratch, info.abs_path);
                    meta_creation.resource_path_.append(*scratch, u8".");
                    meta_creation.resource_path_.append(*scratch, META_FILE_EXT);
                    meta_creation.ext_ = ext;
                    meta_creation_deque.push_back(*scratch, std::move(meta_creation));
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
        std::cout << "Create Meta File: ";
        std::cout.write(RCAST(char *, mt.resource_path_.data()), mt.resource_path_.get_length())
            << std::endl;
        meta_creation_deque.pop_front();
    }

    temp_1.end_scratch();
}

bool LooseFileSource::is_supported_ext(StringView ext) const
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

bool LooseFileSource::has_associated_metadata_file(StringView path) const
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

void ResourceSystem::startup(Vallocator &vallocator, StringView path)
{
    resource_path_.init(vallocator, path.get_length());
    resource_path_.assign(vallocator, path);
    file_sources_.init(vallocator, MAX_FILE_SOURCE);

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
            IFileSource *source = create_file_source(vallocator, info.abs_path, true);
            file_sources_.push_back(vallocator, source);
        }
    }
    temp.end_scratch();
}

void ResourceSystem::shutdown(Vallocator &vallocator)
{
    for (uint64_t idx = 0; idx < assets_.get_count(); ++idx)
    {
        Asset &asset = assets_[idx];
        if (asset.open)
        {
            FS::close(asset.handle);
        }
    }
    resource_registry_.destroy(vallocator);
    assets_.destroy(vallocator);
    for (size_t idx = 0; idx < file_sources_.get_count(); ++idx)
    {
        IFileSource *source = file_sources_[idx];
        source->destroy(vallocator);
    }
    file_sources_.destroy(vallocator);
    resource_path_.destroy(vallocator);
}

IFileSource *ResourceSystem::create_file_source(Vallocator &vallocator, const String &path, bool loose)
{
    IFileSource *source = construct<LooseFileSource>(vallocator);
    source->init(vallocator, path);
    return source;
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
    return false;
}
} // namespace VoidEngine
