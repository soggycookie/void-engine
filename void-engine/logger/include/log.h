#pragma once
#include <cstdint>
#include <cstdio>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <winuser.h>
#endif

namespace Logger
{

enum class LogSeverity : uint32_t
{
    TRACE_,
    INFO_,
    DEBUG_,
    WARN_,
    ERROR_,
};

constexpr uint32_t kMaxLogMsgLength = 256;

struct LogEntry
{
    const char *channel;
    const char *file;
    uint32_t line;
    LogSeverity severity;
    char msg[kMaxLogMsgLength];
};

constexpr uint32_t kLogCapacity = 4;

struct LogBuffer
{
    LogEntry entries[kLogCapacity];
    uint32_t head;
    uint32_t tail;
};

extern LogBuffer g_logBuffer;

template <typename... Args>
void WriteLog(LogSeverity severity_, const char *channel_, const char *file_,
              uint32_t line_, const char *fmt, Args &&...args)
{
    LogEntry &e = g_logBuffer.entries[g_logBuffer.head & (kLogCapacity - 1)];

    e = {
        .channel = channel_,
        .file = file_,
        .line = line_,
        .severity = severity_,
    };

    std::snprintf(e.msg, kMaxLogMsgLength, fmt, std::forward<Args>(args)...);

    ++g_logBuffer.head;

    if ((g_logBuffer.head - g_logBuffer.tail) > kLogCapacity)
    {
        ++g_logBuffer.tail;
    }
}

void PrintLog();

void FlushLog();

inline const char *GetLogSeverityString(LogSeverity severity)
{
    switch (severity)
    {
    case LogSeverity::TRACE_:
        return "TRACE";
    case LogSeverity::INFO_:
        return "INFO";
    case LogSeverity::DEBUG_:
        return "DEBUG";
    case LogSeverity::WARN_:
        return "WARN";
    case LogSeverity::ERROR_:
        return "ERROR";
    }

    return nullptr;
}

inline void ShowErrorDialog(void* windowHandle, const char* caption, const char* content)
{
#ifdef _WIN32
    MessageBoxEx(static_cast<HWND>(windowHandle), content, caption, MB_OK | MB_ICONERROR, 0);
#else

#endif
}

} // namespace Logger
#define PRINT_LOG() Logger::PrintLog();
#define FLUSH_LOG() Logger::FlushLog();

#define LOG_TRACE(channel, fmt, ...)                                           \
    Logger::WriteLog(Logger::LogSeverity::TRACE_, channel, __FILE__, __LINE__, \
                     fmt, ##__VA_ARGS__)

#define LOG_INFO(channel, fmt, ...)                                            \
    Logger::WriteLog(Logger::LogSeverity::INFO_, channel, __FILE__, __LINE__,  \
                     fmt, ##__VA_ARGS__)

#define LOG_DEBUG(channel, fmt, ...)                                           \
    Logger::WriteLog(Logger::LogSeverity::DEBUG_, channel, __FILE__, __LINE__, \
                     fmt, ##__VA_ARGS__)

#define LOG_WARN(channel, fmt, ...)                                            \
    Logger::WriteLog(Logger::LogSeverity::WARN_, channel, __FILE__, __LINE__,  \
                     fmt, ##__VA_ARGS__)

#define LOG_ERROR(channel, fmt, ...)                                           \
    Logger::WriteLog(Logger::LogSeverity::ERROR_, channel, __FILE__, __LINE__, \
                     fmt, ##__VA_ARGS__)
