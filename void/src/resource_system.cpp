#include "resource_system.h"
#include "file_system.h"
#include "str.h"

namespace VoidEngine
{

constexpr uint8_t SINGLE_RESOURCE_FILE_HEADER = 1;
constexpr uint8_t BUNDLE_RESOURCE_FILES_HEADER = 2;

constexpr const char8_t *JPG_EXT = u8"jpg";
constexpr const char8_t *PNG_EXT = u8"png";
constexpr const char8_t *SPV_EXT = u8"spv";
constexpr const char8_t *BUNDLE_EXT = u8"bundle";

constexpr const char8_t *META_EXT = u8"meta";

constexpr const char8_t *SUPPORTED_EXTS[] = {
    JPG_EXT,
    PNG_EXT,
    SPV_EXT,
    BUNDLE_EXT,
};

constexpr uint64_t META_EXT_LENGTH = 4;
constexpr uint64_t SUPPORTED_EXT_COUNT = sizeof(SUPPORTED_EXTS) / sizeof(SUPPORTED_EXTS[0]);

constexpr uint32_t MAX_FILE_SOURCE = 16;

void Reader::init(const void *buf, uint64_t len)
{
    ENGINE_ASSERT(buf, "Buffer must not be null!");
    ENGINE_ASSERT(len > 0, "Buffer length must be greater than 0!");
    p_ = SCAST(const char *, buf);
    end_ = p_ + len;
}

void Reader::read_uint64(uint64_t &out)
{
    ENGINE_ASSERT(reader_remaining() >= sizeof(out), "Exceed buffer length!");
    std::memcpy(&out, p_, sizeof(uint64_t));
    p_ += sizeof(uint64_t);
}

void Reader::read_byte(char &out)
{
    ENGINE_ASSERT(reader_remaining() >= sizeof(out), "Exceed buffer length!");
    std::memcpy(&out, p_, sizeof(char));
    p_ += sizeof(char);
}

uint64_t Reader::reader_remaining()
{
    ENGINE_ASSERT(p_, "Reader is not initialized!");
    return SCAST(uint64_t, end_ - p_);
}

void Writer::init(void *buf, uint64_t len)
{
    ENGINE_ASSERT(buf, "Buffer must not be null!");
    ENGINE_ASSERT(len > 0, "Buffer length must be greater than 0!");
    p_ = SCAST(char *, buf);
    end_ = p_ + len;
    written_ = 0;
}

void Writer::write_uint64(uint64_t data)
{
    ENGINE_ASSERT(writer_remaining() >= sizeof(data), "Exceed buffer length!");
    std::memcpy(p_, &data, sizeof(uint64_t));
    p_ += sizeof(uint64_t);
    written_ += sizeof(data);
}

void Writer::write_byte(char data)
{
    ENGINE_ASSERT(writer_remaining() >= sizeof(data), "Exceed buffer length!");
    std::memcpy(p_, &data, sizeof(char));
    p_ += sizeof(char);
    written_ += sizeof(data);
}

uint64_t Writer::writer_remaining()
{
    ENGINE_ASSERT(p_, "Reader is not initialized!");
    return SCAST(uint64_t, end_ - p_);
}

void ResourceSystem::startup(StringView path)
{
    vallocator_.init(gib(64), mib(16), gib(64), mib(4));
    resource_path_.init(vallocator_, path.get_length());
    resource_path_.assign(vallocator_, path);
    sub_resource_paths.init(vallocator_, MAX_FILE_SOURCE);
    assets_.init(vallocator_, 64);
    resource_registry_.init(vallocator_, 64);

    Arena *scratch = t_scratches.get_scratch();
    ScopedArenaTemp temp;
    temp.start_scratch(scratch);
    DynamicArray<FS::DirectoryInfo> children = FS::find_children(*scratch, path);

    for (uint64_t idx = 0; idx < children.get_count(); ++idx)
    {
        FS::DirectoryInfo &info = children[idx];
        if (info.type == FS::DIRECTORY_TYPE)
        {
            String sub_resource;
            sub_resource.init(vallocator_, info.abs_path.get_length());
            sub_resource.assign(vallocator_, info.abs_path);
            sub_resource_paths.push_back(vallocator_, std::move(sub_resource));
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
        if (asset.open_)
        {
            FS::close(asset.file_);
        }
    }
    resource_registry_.destroy(vallocator_);
    assets_.destroy(vallocator_);
    for (uint64_t idx = 0; idx < sub_resource_paths.get_count(); ++idx)
    {
        sub_resource_paths[idx].destroy(vallocator_);
    }
    sub_resource_paths.destroy(vallocator_);
    resource_path_.destroy(vallocator_);
    vallocator_.destroy();
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

    DynamicArray<FS::DirectoryInfo> dir_children = FS::find_children(*scratch, sub_resource_path);

    Deque<StringView> sole_meta_creation_deque;
    sole_meta_creation_deque.init(*scratch, 256);

    Deque<FS::DirectoryInfo> processing_resource_directories;
    array_to_deque(*scratch, std::move(dir_children), processing_resource_directories);

    while (!processing_resource_directories.is_empty())
    {
        FS::DirectoryInfo &info = processing_resource_directories.front();
        if (info.type == FS::FILE_TYPE)
        {
            StringView ext = FS::get_file_ext(info.dir_name);
            if (is_supported_ext(ext) && info.meta.size > 0)
            {
                if (has_associated_meta_file(info.abs_path))
                {
                    // parse the meta file
                    parse_resource(info.abs_path);
                }
                else
                {
                    // push into a queue, create meta later so we can compare guid to avoid collision
                    // only sole file can only go to this path
                    sole_meta_creation_deque.push_back(*scratch, info.abs_path);
                }
            }
        }
        else
        {
            DynamicArray<FS::DirectoryInfo> sub_folder_children = FS::find_children(*scratch, info.abs_path);
            for (uint64_t idx = 0; idx < sub_folder_children.get_count(); ++idx)
            {
                processing_resource_directories.push_back(*scratch, std::move(sub_folder_children[idx]));
            }
            sub_folder_children.clear();
        }
        processing_resource_directories.pop_front();
    }

    while (!sole_meta_creation_deque.is_empty())
    {
        StringView &abs_rpath = sole_meta_creation_deque.front();
        create_meta_for_sole_file(abs_rpath);
        // TODO: remove the read file and populate directly
        parse_resource(abs_rpath);
        sole_meta_creation_deque.pop_front();
    }
    temp_1.end_scratch();
}

StringView resource_type_to_string(ResourceType rtype)
{
    switch (rtype)
    {
    case SHADER_RESOURCE_TYPE:
        return u8"shader";
    case TEXTURE_RESOURCE_TYPE:
        return u8"texture";
    default:
        return u8"unknown type";
    }
}

void ResourceSystem::create_meta_for_sole_file(StringView abs_rpath)
{
    ScopedArenaTemp temp;
    Arena *scratch = t_scratches.get_scratch();
    temp.start_scratch(scratch);

    FS::File rfile = FS::open(abs_rpath);
    if (rfile == FS::INVALID_FILE)
    {
        // TODO: handle
        return;
    }
    uint64_t rsize = FS::get_file_size(rfile);
    FS::close(rfile);

    String mpath = get_sole_meta_file_path(*scratch, abs_rpath);
    ResourceType rtype = ext_to_resource_type(FS::get_file_ext(abs_rpath));

    // Create temp file and rename
    FS::create(mpath);
    FS::File mfile = FS::open(mpath, FS::WRITE_ACCESS_FLAG | FS::TRUNCATE_FLAG);
    AssetId guid = get_free_asset_id();
    char buf[64];
    Writer writer;
    writer.init(buf, 64);
    writer.write_uint64(guid);
    writer.write_byte(SCAST(char, rtype));
    writer.write_uint64(rsize);
    FS::write(mfile, buf, writer.written_);
    FS::flush(mfile);
    FS::close(mfile);
    temp.end_scratch();
    std::cout << "New " << resource_type_to_string(rtype) << " ID: " << guid << std::endl;
}

String ResourceSystem::get_sole_meta_file_path(Arena &arena, StringView abs_rpath)
{
    String path;
    path.init(arena, abs_rpath.get_length() + META_EXT_LENGTH + 1);
    path.append(arena, abs_rpath);
    path.append(arena, u8".");
    path.append(arena, META_EXT);
    return path;
}

void ResourceSystem::parse_resource(StringView abs_rpath)
{
    ScopedArenaTemp temp;
    Arena *scratch = t_scratches.get_scratch();
    temp.start_scratch(scratch);

    Asset asset;
    asset.file_name_.init(vallocator_, abs_rpath.get_length());
    asset.file_name_.assign(vallocator_, abs_rpath);

    StringView ext = FS::get_file_ext(abs_rpath);
    bool is_bundle = is_bundle_file(ext);

    if (!is_bundle)
    {
        uint64_t size = 0;

        String meta_file_name = get_sole_meta_file_path(*scratch, abs_rpath);
        FS::File meta_file = FS::open(meta_file_name, FS::READ_ACCESS_FLAG);
        void *buf = FS::read(vallocator_, meta_file, &size);
        if (buf)
        {
            AssetId guid;
            Reader reader;
            char type;
            uint64_t rsize = 0;
            reader.init(buf, size);
            reader.read_uint64(guid);
            reader.read_byte(type);
            reader.read_uint64(rsize);
            ResourceType rtype = SCAST(ResourceType, type);
            std::cout << "Read " << resource_type_to_string(rtype) << " ID: " << guid << std::endl;
            ENGINE_ASSERT(rtype == ext_to_resource_type(ext), "Resource type mismatched!");
            std::cout << "ID: " << guid << " with " << rsize << " bytes" << std::endl;
            asset.resource_ref_count_ = 1;

            ResourceData rdata;
            rdata.offset_ = 0;
            rdata.size_ = rsize;
            rdata.asset_idx_ = assets_.get_count();
            rdata.type_ = rtype;
            rdata.state_ = STATE_UNLOADED;
            rdata.ref_count_ = 0;
            rdata.data_ = nullptr;

            resource_registry_.insert(vallocator_, guid, rdata);
        }
        else
        {
            // TODO: handle recovery
        }
    }
    else
    {
        // TODO: bundle type
    }
    assets_.push_back(vallocator_, std::move(asset));

    temp.end_scratch();
    // switch (type)
    // {
    // case TEXTURE_RESOURCE_TYPE:
    // {
    //     std::cout << "texture" << std::endl;
    //     break;
    // }
    // case SHADER_RESOURCE_TYPE:
    // {
    //     std::cout << "shader" << std::endl;
    //     break;
    // }
    // default:
    // {
    //     break;
    // }
    // }
}

bool ResourceSystem::is_bundle_file(StringView ext) const
{
    if (ext == BUNDLE_EXT)
    {
        return true;
    }
    else
    {
        return false;
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

ResourceType ResourceSystem::ext_to_resource_type(StringView ext) const
{
    if (ext == JPG_EXT || ext == PNG_EXT)
    {
        return TEXTURE_RESOURCE_TYPE;
    }
    else if (ext == SPV_EXT)
    {
        return SHADER_RESOURCE_TYPE;
    }
    else
    {
        return UNKNOWN_RESOURCE_TYPE;
    }
}

bool ResourceSystem::has_associated_meta_file(StringView path) const
{
    ScopedArenaTemp temp;
    Arena *scratch = t_scratches.get_scratch();
    temp.start_scratch(scratch);

    String meta_path;
    meta_path.init(*scratch, path.get_length() + META_EXT_LENGTH + 1);
    meta_path.append(*scratch, path);
    meta_path.append(*scratch, u8".");
    meta_path.append(*scratch, META_EXT);
    bool file_exists = FS::is_existing(meta_path);

    temp.end_scratch();
    return file_exists;
}
} // namespace VoidEngine
