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
    BEGIN,
    END,
    CURRENT,
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
    StringView path_name;
    FileMeta meta;
    DirectoryType type;

    DirectoryInfo() : abs_path()
    {
    }

    DirectoryInfo(DirectoryInfo &&other)
        : abs_path(std::move(other.abs_path)), meta(std::move(other.meta)), type(other.type),
          path_name(other.path_name)
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

struct IO_Request
{
    IO_Request() : buffer_size(0), buffer(nullptr), callback(nullptr)
    {
    }

#ifdef VOID_WIN32
    OVERLAPPED data;
#endif // VOID_WIN32

    uint64_t buffer_size;
    void *buffer;
    void (*callback)(void *);
    void *userData;
};

void startup(uint32_t max_concurrency = 0);
void shutdown();

void poll_io_completions(void *);

FileMeta get_metadata(StringView path);
DirectoryType get_directory_type(StringView path);
bool is_existing(StringView path);

File open(StringView path, FileOptionFlags flags = SCAST(uint32_t, FileOptionFlag::READ_ACCESS_FLAG) |
                                                   SCAST(uint32_t, FileOptionFlag::WRITE_ACCESS_FLAG));

bool close(File file);

uint64_t get_file_size(File file);

bool async_read(File file, uint64_t offset, uint64_t size);

bool create(StringView path);

bool remove(StringView path);

bool rename(StringView old_path, StringView new_path);

StringView get_file_ext(StringView path);

void find_children(Arena &arena, DynamicArray<DirectoryInfo> &info, StringView path);

void write(File handle, const void *data, uint64_t num_bytes_to_write);

void seek(File handle, uint64_t offset, FileAnchor anchor);

uint64_t get_file_offset(File handle);

void get_absolute_path(Arena &arena, StringView path, String &out_abs_path);

void get_current_directory(Arena &arena, String &out_dir);
void get_current_directory(Vallocator &vallocator, String &out_dir);

} // namespace VoidEngine::FS
