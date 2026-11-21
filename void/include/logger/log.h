#pragma once
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#define NOMINMAX
#ifdef VOID_WIN32
#include <windows.h>
#include <winuser.h>
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#define DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define DEBUG_BREAK() __builtin_trap()
#else
#define DEBUG_BREAK() ((void)0)
#endif

namespace Logger
{

enum class LogSeverity : uint32_t
{
    TRACE_LEVEL,
    INFO_LEVEL,
    DEBUG_LEVEL,
    WARN_LEVEL,
    ERROR_LEVEL,
    FATAL_LEVEL,
};

constexpr uint32_t MAX_LOG_MSG_LENGTH = 1024;

struct LogEntry
{
    const char *channel_;
    const char *file_;
    uint32_t line_;
    LogSeverity severity_;
    char msg_[MAX_LOG_MSG_LENGTH];
};

constexpr uint32_t LOG_BUF_CAPACITY = 32;

struct LogBuffer
{
    LogEntry entries_[LOG_BUF_CAPACITY];
    uint32_t head_;
    uint32_t tail_;
};

extern LogBuffer g_log_buffer;

void set_window_handle(const void *window_handle);

template <typename... Args>
void write_log(LogSeverity severity, const char *channel, const char *file, uint32_t line,
               const char *fmt, Args &&...args)
{
    LogEntry &e = g_log_buffer.entries_[g_log_buffer.head_ & (LOG_BUF_CAPACITY - 1)];

    e = {
        .channel_ = channel,
        .file_ = file,
        .line_ = line,
        .severity_ = severity,
    };

    std::snprintf(e.msg_, MAX_LOG_MSG_LENGTH, fmt, std::forward<Args>(args)...);

    ++g_log_buffer.head_;
    if ((g_log_buffer.head_ - g_log_buffer.tail_) > LOG_BUF_CAPACITY)
    {
        ++g_log_buffer.tail_;
    }
}

void print_log();

void flush_log();

inline const char *get_log_severity_string(LogSeverity severity)
{
    switch (severity)
    {
    case LogSeverity::TRACE_LEVEL:
        return "TRACE";
    case LogSeverity::INFO_LEVEL:
        return "INFO";
    case LogSeverity::DEBUG_LEVEL:
        return "DEBUG";
    case LogSeverity::WARN_LEVEL:
        return "WARN";
    case LogSeverity::ERROR_LEVEL:
        return "ERROR";
    case LogSeverity::FATAL_LEVEL:
        return "FATAL";
    }
    return nullptr;
}

struct ContextFieldPrinter
{
    char msg[MAX_LOG_MSG_LENGTH] = "";
    uint32_t count = 0;
    bool first = true;

    template <typename Arg, typename... Fields>
    void AddField(const char *fieldLabel, const Arg &val, Fields &&...fields)
    {
        AddField(fieldLabel, val);
        AddField(std::forward<Fields>(fields)...);
    }

    template <typename Arg>
    void AddField(const char *fieldLabel, const Arg &val)
    {
        uint32_t writeSize = 0;
        if (first)
        {
            writeSize = std::snprintf(&msg[count], MAX_LOG_MSG_LENGTH - count, "%s = ", fieldLabel);
        }
        else
        {
            writeSize = std::snprintf(&msg[count], MAX_LOG_MSG_LENGTH - count, ", %s = ", fieldLabel);
        }

        count += writeSize;

        if (count >= MAX_LOG_MSG_LENGTH)
        {
            count = MAX_LOG_MSG_LENGTH - 1;
            return;
        }

        if constexpr (std::is_same_v<Arg, std::string> || std::is_same_v<Arg, std::string_view> ||
                      std::is_convertible_v<Arg, const char *>)
        {
            writeSize = std::snprintf(&msg[count], MAX_LOG_MSG_LENGTH - count, "\"%s\"",
                                      std::string(val).c_str());
        }
        else if constexpr (std::is_floating_point_v<Arg>)
        {
            writeSize =
                std::snprintf(&msg[count], MAX_LOG_MSG_LENGTH - count, "%g", static_cast<double>(val));
        }
        else if constexpr (std::is_integral_v<Arg>)
        {
            writeSize = std::snprintf(&msg[count], MAX_LOG_MSG_LENGTH - count, "%lld",
                                      static_cast<int64_t>(val));
        }
        else
        {
            // LOG_ASSERT_LOG_ERROR("<opaque>");
        }

        first = false;

        count += writeSize;

        if (count >= MAX_LOG_MSG_LENGTH)
        {

            count = MAX_LOG_MSG_LENGTH - 1;
            return;
        }
    }

    void Print() const
    {
        std::cout << "Context:\n" << msg << std::endl;
    }
};

void show_fatal_mbox(const char *caption, const char *content);

} // namespace Logger
#define PRINT_LOG() Logger::print_log();
#define FLUSH_LOG() Logger::flush_log();

#define LOG_TRACE(channel, fmt, ...)                                                                    \
    Logger::write_log(Logger::LogSeverity::TRACE_LEVEL, channel, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_INFO(channel, fmt, ...)                                                                     \
    Logger::write_log(Logger::LogSeverity::INFO_LEVEL, channel, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(channel, fmt, ...)                                                                    \
    Logger::write_log(Logger::LogSeverity::DEBUG_LEVEL, channel, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN(channel, fmt, ...)                                                                     \
    Logger::write_log(Logger::LogSeverity::WARN_LEVEL, channel, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(channel, fmt, ...)                                                                    \
    Logger::write_log(Logger::LogSeverity::ERROR_LEVEL, channel, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_FATAL(channel, fmt, ...)                                                                    \
    Logger::write_log(Logger::LogSeverity::FATAL_LEVEL, channel, __FILE__, __LINE__,                    \
                      fmt, ##__VA_ARGS__);                                                              \
    Logger::flush_log();                                                                                \
    std::abort();

#define LOG_FATAL_EX(channel, file, line, fmt, ...)                                                     \
    Logger::write_log(Logger::LogSeverity::FATAL_LEVEL, channel, file, line, fmt, ##__VA_ARGS__);            \
    Logger::flush_log();                                                                                \
    std::abort();

#define CONTEXT(...) [](Logger::ContextFieldPrinter &fp) { fp.AddField(__VA_ARGS__); }

#define LOG_ASSERT(cond, msg, ...)                                                                      \
    while (true)                                                                                        \
    {                                                                                                   \
        if (cond)                                                                                       \
            break;                                                                                      \
        Logger::write_log(Logger::LogSeverity::ERROR_, "ASSERT", __FILE__, __LINE__,                    \
                          msg, ##__VA_ARGS__);                                                          \
        FLUSH_LOG();                                                                                    \
        DEBUG_BREAK();                                                                                  \
    }

#define ENGINE_ASSERT(cond, msg, ...)                                                                   \
    do                                                                                                  \
    {                                                                                                   \
        if (cond)                                                                                       \
            break;                                                                                      \
        FLUSH_LOG();                                                                                    \
        char buffer[512];                                                                               \
        std::snprintf(buffer, 512, msg, ##__VA_ARGS__);                                                 \
        std::cerr << buffer << std::endl;                                                               \
        DEBUG_BREAK();                                                                                  \
        std::abort();                                                                                   \
    } while (0)

#define LOG_ASSERT_CTX(cond, ctx, msg, ...)                                                             \
    while (true)                                                                                        \
    {                                                                                                   \
        if (cond)                                                                                       \
            break;                                                                                      \
        Logger::write_log(Logger::LogSeverity::ERROR_, "ASSERT", __FILE__, __LINE__,                    \
                          msg, ##__VA_ARGS__);                                                          \
        Logger::ContextFieldPrinter fp;                                                                 \
        ctx(fp);                                                                                        \
        FLUSH_LOG();                                                                                    \
        fp.Print();                                                                                     \
        DEBUG_BREAK();                                                                                  \
    }

#ifdef VOID_WIN32
#define ENGINE_ASSERT_WIN32(cond)                                                                       \
    do                                                                                                  \
    {                                                                                                   \
        if (cond)                                                                                       \
            break;                                                                                      \
        DWORD error = GetLastError();                                                                   \
        LPSTR message = nullptr;                                                                        \
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |                    \
                           FORMAT_MESSAGE_IGNORE_INSERTS,                                               \
                       nullptr, error, 0, message, 0, nullptr);                                         \
        std::cout << message << std::endl;                                                              \
        FLUSH_LOG();                                                                                    \
        LocalFree(message);                                                                             \
        DEBUG_BREAK();                                                                                  \
        std::abort();                                                                                   \
    } while (0)

#endif // VOID_WIN32
