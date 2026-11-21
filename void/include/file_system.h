#pragma once
#include "arena.h"
#include "dynamic_array.h"
#include "str.h"
#include "utils.h"

namespace VoidEngine::FS
{

struct FileMeta
{
    uint64_t size;
    TimePoint creation_time;
    TimePoint last_modified_time;
};

constexpr FileMeta InvalidFileMeta{};

enum FileOptionFlag : uint32_t
{
    READ_ACCESS_FLAG = 0x01U,
    WRITE_ACCESS_FLAG = 0x02U,
    TRUNCATE_FLAG = 0x04U,
    CREATE_IF_NOT_EXIST_FLAG = 0x08U,
    ASYNC_FLAG = 0x10U,
    WRITE_APPEND = 0x20U,
};
using FileOptionFlags = uint32_t;

enum FileAnchor
{
    ANCHOR_BEGIN,
    ANCHOR_END,
    ANCHOR_CURRENT,
};

enum DirectoryType
{
    FILE_TYPE,
    DIRECTORY_TYPE,
    UNKNOWN_TYPE,
};

constexpr uint32_t MAX_LOCAL_PATH = 512;
constexpr uint32_t MAX_OPENED_FILE = 1024;

struct DirectoryInfo
{
    String abs_path;
    StringView dir_name;
    FileMeta meta;
    DirectoryType type;

    DirectoryInfo() : abs_path()
    {
    }

    DirectoryInfo(DirectoryInfo &&other)
        : abs_path(std::move(other.abs_path)), meta(std::move(other.meta)), type(other.type),
          dir_name(other.dir_name)
    {
    }
};

enum FileErrorCode
{
    FILE_NOT_FOUND,
    PATH_NOT_FOUND,
    PERMISSION_DENIED,
    SUCCESS,
};

DEFINE_HANDLE(File);
extern const File INVALID_FILE;

void startup(uint32_t max_concurrency = 0);
void shutdown();

//dummy 2 void * to satisfy job signature
void poll_io_completions(void *, void *);

FileMeta get_metadata(StringView path);
DirectoryType get_directory_type(StringView path);
bool is_existing(StringView path);

File open(StringView path, FileOptionFlags flags = SCAST(uint32_t, FileOptionFlag::READ_ACCESS_FLAG) |
                                                   SCAST(uint32_t, FileOptionFlag::WRITE_ACCESS_FLAG));

bool close(File file);

uint64_t get_file_size(File file);

bool async_read(Vallocator &vallocator, File file, uint64_t offset, uint64_t size,
                void (*callback)(void *, void *), void * user_data = nullptr);

void *read(Vallocator &vallocator, File file, uint64_t *out_size);

bool create(StringView path);

bool remove(StringView path);

bool rename(StringView old_path, StringView new_path);

StringView get_file_ext(StringView path);

DynamicArray<DirectoryInfo> find_children(Arena &arena, StringView path);

void write(File file, const void *data, uint64_t num_bytes_to_write);
void flush(File file);

void seek(File file, uint64_t offset, FileAnchor anchor);

uint64_t get_file_offset(File file);

String get_absolute_path(Arena &arena, StringView path);

String get_current_directory(Arena &arena);
String get_current_directory(Vallocator &vallocator);

} // namespace VoidEngine::FS
