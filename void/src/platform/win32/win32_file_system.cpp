#ifdef VOID_WIN32
#include "file_system.h"

#define NOMINMAX
#include <windows.h>

namespace VoidEngine::FS
{

struct File_T
{
    HANDLE handle;
};

struct FileNode
{
    union
    {
        FileNode *next;
        File_T file;
    };
};

struct IOContext
{
    HANDLE iocp;
    FileNode files[MAX_OPENED_FILE];
    FileNode *free_node;
    uint32_t file_count;
    std::atomic<bool> can_write;
    std::atomic<bool> running;
};

static ULONG_PTR SHUTDOWN_KEY = 0xDEAD;

static IOContext g_ctx;

wchar_t *utf8_to_utf16(Arena *arena, const char8_t *utf8, uint32_t path_len)
{
    ENGINE_ASSERT(utf8, "Utf8 string is null!");
    int32_t wide_char_count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, RCAST(const char *, utf8), path_len, NULL, 0);
    ENGINE_ASSERT(wide_char_count > 0, "");
    wchar_t *buf = SCAST(
        wchar_t *, arena->push(sizeof(wchar_t) * (wide_char_count + 1), alignof(wchar_t), nullptr));
    ENGINE_ASSERT(buf, "Failed to allocate!");
    int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, RCAST(const char *, utf8), path_len,
                                      buf, wide_char_count);
    ENGINE_ASSERT(written > 0, "Failed to write!");
    buf[written] = L'\0';
    return buf;
}

char8_t *utf16_to_utf8(Arena &arena, const wchar_t *utf16, int32_t *out_len)
{
    ENGINE_ASSERT(utf16, "Wide string is null!");
    int32_t char_bytes = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, NULL, 0, NULL, NULL);
    ENGINE_ASSERT(char_bytes > 0, "");
    char *buf = arena_push_array<char>(arena, char_bytes);
    ENGINE_ASSERT(buf, "Failed to allocate!");
    int32_t written = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, buf, char_bytes, NULL, NULL);
    ENGINE_ASSERT(written == char_bytes, "Failed to write!");
    if (out_len)
    {
        *out_len = char_bytes - 1;
    }
    return RCAST(char8_t *, buf);
}

void startup(uint32_t max_concurrency)
{
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, max_concurrency);
    ENGINE_ASSERT_WIN32(iocp != INVALID_HANDLE_VALUE);
    g_ctx.iocp = iocp;
    g_ctx.running.store(true);
    g_ctx.free_node = nullptr;
    for (uint32_t idx = 0; idx < MAX_OPENED_FILE; ++idx)
    {
        g_ctx.files[idx].next = g_ctx.free_node;
        g_ctx.free_node = &g_ctx.files[idx];
    }
    g_ctx.can_write.store(true, std::memory_order_release);
}

void shutdown()
{
    g_ctx.running.store(false);
    PostQueuedCompletionStatus(g_ctx.iocp, 0, SHUTDOWN_KEY, nullptr);
}

DirectoryType get_directory_type(StringView path)
{
    ENGINE_ASSERT(!path.is_empty(), "Path cannot be null!");
    Arena *scratch = t_scratches.get_scratch(nullptr);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    wchar_t *utf16 = utf8_to_utf16(scratch, path.data(), path.get_length());
    DWORD attr = GetFileAttributesW(utf16);
    temp.end_scratch();

    if (attr == INVALID_FILE_ATTRIBUTES)
    {
        return UNKNOWN_TYPE;
    }

    DirectoryType type = (attr & FILE_ATTRIBUTE_DIRECTORY) > 0 ? DIRECTORY_TYPE : FILE_TYPE;

    return type;
}

FileMeta extract_metadata(DWORD file_size_low, DWORD file_size_high, FILETIME file_modified_time,
                          FILETIME file_creation_time)
{
    using namespace std::chrono;
    FileMeta meta;
    meta.size = (SCAST(uint64_t, file_size_high) << 32) | SCAST(uint64_t, file_size_low);

    ULARGE_INTEGER creation_time;
    creation_time.LowPart = file_creation_time.dwLowDateTime;
    creation_time.HighPart = file_creation_time.dwHighDateTime;

    ULARGE_INTEGER last_modified_time;
    last_modified_time.LowPart = file_modified_time.dwLowDateTime;
    last_modified_time.HighPart = file_modified_time.dwHighDateTime;

    // quadPart is 100ns ticks since 1601-01-01
    int64_t ct_ticks_since_1601 = SCAST(int64_t, creation_time.QuadPart);
    int64_t lmt_ticks_since_1601 = SCAST(int64_t, last_modified_time.QuadPart);

    // Shift to Unix epoch (1970-01-01)
    constexpr int64_t FILETIME_EPOCH_DIFF = 116444736000000000LL; // 100ns ticks
    int64_t ct_ticks_since_1970 = ct_ticks_since_1601 - FILETIME_EPOCH_DIFF;
    int64_t lmt_ticks_since_1970 = lmt_ticks_since_1601 - FILETIME_EPOCH_DIFF;

    // 100ns duration type
    using FiletimeDuration = duration<int64_t, std::ratio<1, 10000000>>;

    meta.creation_time = time_point<system_clock>(
        duration_cast<system_clock::duration>(FiletimeDuration(ct_ticks_since_1970)));

    meta.last_modified_time = time_point<system_clock>(
        duration_cast<system_clock::duration>(FiletimeDuration(lmt_ticks_since_1970)));

    return meta;
}

FileMeta get_metadata(StringView path)
{
    ENGINE_ASSERT(!path.is_empty(), "Path cannot be null!");
    Arena *scratch = t_scratches.get_scratch(nullptr);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    wchar_t *utf16 = utf8_to_utf16(scratch, path.data(), path.get_length());
    WIN32_FILE_ATTRIBUTE_DATA fileAttr;
    BOOL result = GetFileAttributesExW(utf16, GetFileExInfoStandard, &fileAttr);
    temp.end_scratch();
    if (!result)
    {
        // TODO: Log this, should not be an assert

        return InvalidFileMeta;
    }

    return extract_metadata(fileAttr.nFileSizeLow, fileAttr.nFileSizeHigh, fileAttr.ftLastWriteTime,
                            fileAttr.ftCreationTime);
}

bool is_existing(StringView path)
{
    ENGINE_ASSERT(!path.is_empty(), "Path cannot be null!");
    Arena *scratch = t_scratches.get_scratch(nullptr);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    wchar_t *utf16 = utf8_to_utf16(scratch, path.data(), path.get_length());
    DWORD attr = GetFileAttributesW(utf16);
    temp.end_scratch();
    return attr != INVALID_FILE_ATTRIBUTES;
}

File open(StringView path, FileOptionFlags flags)
{
    ENGINE_ASSERT(!path.is_empty(), "Path cannot be null!");
    Arena *scratch = t_scratches.get_scratch(nullptr);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    wchar_t *utf16 = utf8_to_utf16(scratch, path.data(), path.get_length());
    DWORD desired_access = 0;
    DWORD creation_disposition = 0;
    DWORD flags_and_attrs = FILE_ATTRIBUTE_NORMAL;

    bool read_access = (flags & SCAST(uint32_t, FileOptionFlag::READ_ACCESS_FLAG)) > 0;
    bool write_access = (flags & SCAST(uint32_t, FileOptionFlag::WRITE_ACCESS_FLAG)) > 0;
    bool truncate = (flags & SCAST(uint32_t, FileOptionFlag::TRUNCATE_FLAG)) > 0;
    bool create_if_not_exist = (flags & SCAST(uint32_t, FileOptionFlag::CREATE_IF_NOT_EXIST_FLAG)) > 0;
    bool async = (flags & SCAST(uint32_t, FileOptionFlag::ASYNC_FLAG)) > 0;
    bool append = (flags & SCAST(uint32_t, FileOptionFlag::WRITE_APPEND)) > 0;

    if (read_access)
    {
        desired_access |= GENERIC_READ;
    }
    if (write_access && !append)
    {
        desired_access |= GENERIC_WRITE;
    }
    else if (write_access && append)
    {
        desired_access |= FILE_APPEND_DATA;
    }

    if (async)
    {
        flags_and_attrs |= FILE_FLAG_OVERLAPPED;
    }

    if (truncate && !create_if_not_exist)
    {
        creation_disposition = TRUNCATE_EXISTING;
    }
    else if (truncate && create_if_not_exist)
    {
        creation_disposition = CREATE_ALWAYS;
    }
    else if (!truncate && create_if_not_exist)
    {
        creation_disposition = OPEN_ALWAYS;
    }
    else
    {
        creation_disposition = OPEN_EXISTING;
    }

    HANDLE handle = CreateFileW(utf16, desired_access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                creation_disposition, flags_and_attrs, NULL);
    temp.end_scratch();
    if (handle == INVALID_HANDLE_VALUE)
    {
        return nullptr;
    }

    bool expected = true;
    while (!g_ctx.can_write.compare_exchange_strong(expected, false, std::memory_order_acquire,
                                                    std::memory_order_relaxed))
    {
        expected = true;
    }

    FileNode *node = g_ctx.free_node;
    g_ctx.free_node = node->next;
    node->file.handle = handle;
    ++g_ctx.file_count;
    ENGINE_ASSERT(g_ctx.file_count <= MAX_OPENED_FILE, "Exceed maximum opened file!");
    g_ctx.can_write.store(true, std::memory_order_release);

    if (async)
    {
        ENGINE_ASSERT(g_ctx.iocp != INVALID_HANDLE_VALUE, "Invalid IOCP handle!");
        HANDLE iocp = CreateIoCompletionPort(node->file.handle, g_ctx.iocp, NULL, 0);
        ENGINE_ASSERT(iocp == g_ctx.iocp, "IOCP mismatch handle!");
    }
    return &node->file;
}

bool close(File file)
{
    BOOL result = CloseHandle(file->handle);
    if (result)
    {
        bool expected = true;
        while (!g_ctx.can_write.compare_exchange_strong(expected, false, std::memory_order_acquire,
                                                        std::memory_order_relaxed))
        {
            expected = true;
        }

        FileNode *new_head = RCAST(FileNode *, file);
        new_head->next = g_ctx.free_node;
        g_ctx.free_node = new_head;
        ENGINE_ASSERT(g_ctx.file_count > 0, "Double close on some file handle!");
        --g_ctx.file_count;
        g_ctx.can_write.store(true, std::memory_order_release);
    }

    return result;
}

uint64_t get_file_size(File file)
{
    LARGE_INTEGER size;
    BOOL result = GetFileSizeEx(file->handle, &size);
    if (!result)
    {
        return 0;
    }
    return size.QuadPart;
}

bool async_read(File file, uint64_t offset, uint64_t size)
{
    ENGINE_ASSERT(file->handle != INVALID_HANDLE_VALUE, "");
    // TODO move this transient allocator
    void *buffer = aligned_alloc(DEFAULT_ALIGNMENT, size);
    ENGINE_ASSERT(buffer, "");
    void *requestAddr = aligned_alloc(alignof(IO_Request), sizeof(IO_Request));
    ENGINE_ASSERT(requestAddr, "");
    IO_Request *request = SCAST(IO_Request *, requestAddr);

    request->buffer = buffer;
    request->buffer_size = size;
    request->data.Offset = low_32bit(size);
    request->data.OffsetHigh = high_32bit(size);
    request->callback = nullptr;
    request->userData = nullptr;

    BOOL result = ReadFile(file->handle, buffer, size, NULL, &request->data);
    if (!result && GetLastError() == ERROR_IO_PENDING)
    {
        return true;
    }
    aligned_free(requestAddr);
    aligned_free(buffer);
    return false;
}

void write(File file, const void *data, uint64_t num_bytes_to_write)
{
    DWORD num_bytes_written = 0;
    BOOL result = WriteFile(file->handle, data, num_bytes_to_write, &num_bytes_written, NULL);

    if (!result)
    {
    }
}

void seek(File file, uint64_t offset, FileAnchor anchor)
{
    DWORD move_method = 0;
    switch (anchor)
    {
    case FileAnchor::BEGIN:
    {
        move_method = FILE_BEGIN;
        break;
    }
    case FileAnchor::END:
    {
        move_method = FILE_END;
        break;
    }
    case FileAnchor::CURRENT:
    {
        move_method = FILE_CURRENT;
        break;
    }
    default:
    {
        ENGINE_ASSERT(0, "Unknown file anchor type");
    }
    }

    LARGE_INTEGER dist;
    dist.QuadPart = offset;
    SetFilePointerEx(file->handle, dist, NULL, move_method);
}

uint64_t get_file_offset(File file)
{
    LONG offset_high = 0;
    DWORD offset_low = SetFilePointer(file->handle, 0, &offset_high, FILE_CURRENT);

    if (offset_low == INVALID_SET_FILE_POINTER)
    {
    }

    LARGE_INTEGER result;
    result.LowPart = offset_low;
    result.HighPart = offset_high;

    return result.QuadPart;
}

bool create(StringView path)
{
    ENGINE_ASSERT(!path.is_empty(), "Path cannot be null!");
    Arena *scratch = t_scratches.get_scratch(nullptr);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    wchar_t *utf16 = utf8_to_utf16(scratch, path.data(), path.get_length());
    HANDLE handle = CreateFileW(utf16, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    temp.end_scratch();
    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    CloseHandle(handle);
    return true;
}

bool remove(StringView path)
{
    ENGINE_ASSERT(!path.is_empty(), "Path cannot be null!");
    Arena *scratch = t_scratches.get_scratch(nullptr);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    wchar_t *utf16 = utf8_to_utf16(scratch, path.data(), path.get_length());
    BOOL result = DeleteFileW(utf16);
    temp.end_scratch();

    return result;
}

bool rename(StringView old_path, StringView new_path)
{
    ENGINE_ASSERT(!old_path.is_empty(), "Old path cannot be null!");
    ENGINE_ASSERT(!new_path.is_empty(), "New path cannot be null!");
    Arena *scratch = t_scratches.get_scratch(nullptr);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    wchar_t *old_utf16 = utf8_to_utf16(scratch, old_path.data(), old_path.get_length());
    wchar_t *new_utf16 = utf8_to_utf16(scratch, new_path.data(), new_path.get_length());
    BOOL result = MoveFileW(old_utf16, new_utf16);
    temp.end_scratch();
    return result;
}

StringView get_file_ext(StringView path)
{
    ENGINE_ASSERT(!path.is_empty(), "Path cannot be null!");
    const char8_t *start = path.data();
    const char8_t *end = start + path.get_length();

    while (end != start)
    {
        --end;
        if (*end == u8'.')
            return StringView(end + 1, BYTE_OFFSET(start, end + 1));

        if (*end == u8'\\' || *end == u8'/')
            break;
    }
    return nullptr;
}

void find_children(Arena &arena, DynamicArray<DirectoryInfo> &info, StringView path)
{
    ENGINE_ASSERT(!path.is_empty(), "Path cannot be null!");
    Arena *scratch = t_scratches.get_scratch(&arena);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    uint32_t filtered_len = path.get_length() + 2;
    char8_t *filtered_all_path =
        SCAST(char8_t *, scratch->push(sizeof(char8_t) * filtered_len, alignof(char8_t), nullptr));
    std::memcpy(filtered_all_path, path.data(), sizeof(char8_t) * path.get_length());
    filtered_all_path[filtered_len - 2] = u8'\\';
    filtered_all_path[filtered_len - 1] = u8'*';
    wchar_t *utf16 = utf8_to_utf16(scratch, filtered_all_path, filtered_len);

    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(utf16, &data);

    if (handle == INVALID_HANDLE_VALUE)
    {
        temp.end_scratch();
        return;
    }

    info.init(arena, 32);
    do
    {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
        {
            continue;
        }

        DirectoryType type =
            (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) > 0 ? DIRECTORY_TYPE : FILE_TYPE;
        uint64_t size = SCAST(uint64_t, data.nFileSizeLow) | (SCAST(uint64_t, data.nFileSizeHigh) << 32);
        FileMeta meta = extract_metadata(data.nFileSizeLow, data.nFileSizeHigh, data.ftLastWriteTime,
                                         data.ftCreationTime);
        DirectoryInfo node;
        node.meta = meta;
        node.type = type;
        int32_t dir_len;
        char8_t *utf8_path_name = utf16_to_utf8(*scratch, data.cFileName, &dir_len);
        node.abs_path.init(arena, dir_len + path.get_length() + 1);
        node.abs_path.assign(arena, path);
        node.abs_path.append(arena, u8"\\");
        node.abs_path.append(arena, utf8_path_name);
        node.path_name = StringView(node.abs_path.data() + (path.get_length() + 1), dir_len);
        info.push_back(arena, std::move(node));

    } while (FindNextFileW(handle, &data));
    FindClose(handle);
    temp.end_scratch();
}

void poll_io_completions(void *)
{
    while (g_ctx.running)
    {
        std::cout << "FS Running 1" << std::endl;
        DWORD byte_transferred;
        OVERLAPPED *overlapped;
        ULONG_PTR completion_key;
        BOOL result = GetQueuedCompletionStatus(g_ctx.iocp, &byte_transferred, &completion_key,
                                                &overlapped, INFINITE);

        std::cout << "FS Running" << std::endl;
        if (result && overlapped)
        {
            std::cout << "========== Loaded " << byte_transferred << "===========" << std::endl;

            IO_Request *request = RCAST(IO_Request *, overlapped);
            ENGINE_ASSERT(request, "IO request is null!");
            // BYTE *data = SCAST(BYTE *, request->buffer);
            // TextEncoding encoding = DetectEncoding(data, byteTransferred);
            // std::string str(RCAST(char *, data), byteTransferred);
            // std::cout << str << std::endl;
            continue;
        }
        if (completion_key == SHUTDOWN_KEY)
        {
            CloseHandle(g_ctx.iocp);
            std::cout << "FileSystem Shutdown" << std::endl;
        }
    }
}

void get_absolute_path(Arena &arena, StringView path, String &out_abs_path)
{
    ENGINE_ASSERT(path.is_empty(), "Path must not be null!");
    Arena *scratch = t_scratches.get_scratch(&arena);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    wchar_t *utf16 = utf8_to_utf16(scratch, path.data(), path.get_length());
    DWORD size = GetFullPathNameW(utf16, 0, nullptr, nullptr);
    ENGINE_ASSERT(size, "Failed to get absolute path!");
    //TODO: remove the null terminator
    wchar_t *abs_path = arena_push_array<wchar_t>(*scratch, size);
    DWORD len = GetFullPathNameW(utf16, size, abs_path, nullptr);
    char8_t *utf8_abs_path = utf16_to_utf8(*scratch, abs_path, nullptr);
    temp.end_scratch();
    out_abs_path.assign(arena, utf8_abs_path);
}

void get_current_directory(Arena &arena, String &out_dir)
{
    DWORD char_count = GetCurrentDirectoryW(0, nullptr);
    if (char_count == 0)
    {
        out_dir.clear();
        return;
    }
    Arena *scratch = t_scratches.get_scratch(&arena);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    //TODO: remove the null terminator
    wchar_t *dir = arena_push_array<wchar_t>(*scratch, char_count);
    DWORD written = GetCurrentDirectoryW(char_count, dir);
    ENGINE_ASSERT(written, "Failed to get current directory!");
    char8_t *utf8_dir = utf16_to_utf8(*scratch, dir, nullptr);
    temp.end_scratch();
    out_dir.assign(arena, utf8_dir);
}

void get_current_directory(Vallocator &vallocator, String &out_dir)
{
    DWORD char_count = GetCurrentDirectoryW(0, nullptr);
    if (char_count == 0)
    {
        out_dir.clear();
        return;
    }
    Arena *scratch = t_scratches.get_scratch(nullptr);
    ScopedScratchTemp temp;
    temp.start_scratch(scratch);
    //TODO: remove the null terminator
    wchar_t *dir = arena_push_array<wchar_t>(*scratch, char_count);
    DWORD written = GetCurrentDirectoryW(char_count, dir);
    ENGINE_ASSERT(written, "Failed to get current directory!");
    char8_t *utf8_dir = utf16_to_utf8(*scratch, dir, nullptr);
    temp.end_scratch();
    out_dir.assign(vallocator, utf8_dir);
}
} // namespace VoidEngine::FS
#endif // VOID_WIN32
