#include "log.h"
#include <cstdio>
#include <utility>

namespace Logger
{

LogBuffer g_logBuffer{.head = 0, .tail = 0};

void FlushLog()
{
    PrintLog();
    g_logBuffer.tail = g_logBuffer.head;
}

void PrintLog()
{
    uint32_t idx = g_logBuffer.tail;
    while (idx != g_logBuffer.head)
    {
        const LogEntry &e = g_logBuffer.entries[idx & (kLogCapacity - 1)];
        std::printf("[%-5s] [%s] %s:%u -- %s\n",
                    GetLogSeverityString(e.severity), e.channel, e.file, e.line,
                    e.msg);
        ++idx;
    }
}

} // namespace Logger
