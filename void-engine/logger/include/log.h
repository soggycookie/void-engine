#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <source_location>
#include <string>
#include <string_view>
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

constexpr uint32_t kMaxLogMsgLength = 1024;

struct LogEntry
{
    const char *channel;
    const char *file;
    uint32_t line;
    LogSeverity severity;
    char msg[kMaxLogMsgLength];
};

constexpr uint32_t kLogCapacity = 32;

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

struct ContextFieldPrinter
{
    char msg[kMaxLogMsgLength] = "";
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
            writeSize = std::snprintf(&msg[count], kMaxLogMsgLength - count,
                                      "%s = ", fieldLabel);
        }
        else
        {
            writeSize = std::snprintf(&msg[count], kMaxLogMsgLength - count,
                                      ", %s = ", fieldLabel);
        }

        count += writeSize;

        if (count >= kMaxLogMsgLength)
        {
            count = kMaxLogMsgLength - 1;
            return;
        }

        if constexpr (std::is_same_v<Arg, std::string> ||
                      std::is_same_v<Arg, std::string_view> ||
                      std::is_convertible_v<Arg, const char *>)
        {
            writeSize = std::snprintf(&msg[count], kMaxLogMsgLength - count,
                                      "\"%s\"", std::string(val).c_str());
        }
        else if constexpr (std::is_floating_point_v<Arg>)
        {
            writeSize = std::snprintf(&msg[count], kMaxLogMsgLength - count,
                                      "%g", static_cast<double>(val));
        }
        else if constexpr (std::is_integral_v<Arg>)
        {
            writeSize = std::snprintf(&msg[count], kMaxLogMsgLength - count,
                                      "%lld", static_cast<int64_t>(val));
        }
        else
        {
            // LOG_ASSERT_LOG_ERROR("<opaque>");
        }

        first = false;

        count += writeSize;

        if (count >= kMaxLogMsgLength)
        {

            count = kMaxLogMsgLength - 1;
            return;
        }
    }

    void Print() const { std::cout << "Context:\n" << msg << std::endl; }
};

inline void ShowErrorDialog(void *windowHandle, const char *caption,
                            const char *content)
{
#ifdef _WIN32
    MessageBoxEx(static_cast<HWND>(windowHandle), content, caption,
                 MB_OK | MB_ICONERROR, 0);
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

#define CONTEXT(...)                                                           \
    [](Logger::ContextFieldPrinter &fp) { fp.AddField(__VA_ARGS__); }

#define LOG_ASSERT(cond, msg, ...)                                             \
    while (true)                                                               \
    {                                                                          \
        if (cond)                                                              \
            break;                                                             \
        Logger::WriteLog(Logger::LogSeverity::ERROR_, "ASSERT", __FILE__,      \
                         __LINE__, msg, ##__VA_ARGS__);                        \
        FLUSH_LOG();                                                           \
        std::terminate();                                                      \
    }

#define LOG_ASSERT_CTX(cond, ctx, msg, ...)                                    \
    while (true)                                                               \
    {                                                                          \
        if (cond)                                                              \
            break;                                                             \
        Logger::WriteLog(Logger::LogSeverity::ERROR_, "ASSERT", __FILE__,      \
                         __LINE__, msg, ##__VA_ARGS__);                        \
        Logger::ContextFieldPrinter fp;                                        \
        ctx(fp);                                                               \
        FLUSH_LOG();                                                           \
        fp.Print();                                                            \
        std::terminate();                                                      \
    }
