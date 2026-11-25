#include "logger/log.h"
#include "utils.h"

namespace Logger
{

static void *g_window_handle;

LogBuffer g_log_buffer{.head_ = 0, .tail_ = 0};

void set_window_handle(const void *window_handle)
{
    // matches the declaration in log.h; stored non-const for MessageBoxEx's HWND
    g_window_handle = const_cast<void *>(window_handle);
}

void show_fatal_mbox(const char *caption, const char *content)
{
    Logger::flush_log();
#ifdef VOID_WIN32
    MessageBoxEx(SCAST(HWND, g_window_handle), content, caption, MB_OK | MB_ICONERROR, 0);
#else

#endif
    std::abort();
}

void flush_log()
{
    print_log();
    g_log_buffer.tail_ = g_log_buffer.head_;
}

void print_log()
{
    uint32_t idx = g_log_buffer.tail_;
    while (idx != g_log_buffer.head_)
    {
        const LogEntry &e = g_log_buffer.entries_[idx & (LOG_BUF_CAPACITY - 1)];
        std::printf("[%-5s] [%s] %s:%u -- %s\n",
                    get_log_severity_string(e.severity_), e.channel_, e.file_, e.line_,
                    e.msg_);
        ++idx;
    }
}

} // namespace Logger
