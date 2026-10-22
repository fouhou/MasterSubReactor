/* Log.cpp —— Log 模块实现：全局 log 对象、时间格式化、级别转字符串。 */
#include "Log.h"

namespace LogModule
{
    Log log;

    /* Get_Time —— 取当前时间，格式化为 "yyyy/MM/dd HH:mm:ss"。 */
    std::string Get_Time()
    {
        time_t tm;
        time(&tm);
        struct tm time_message;
        localtime_r(&tm, &time_message);
        std::stringstream ss;
        char buf[128];
        snprintf(buf, sizeof(buf), "%4d/%02d/%02d %02d:%02d:%02d",
                 time_message.tm_year + 1900,
                 time_message.tm_mon + 1,
                 time_message.tm_mday,
                 time_message.tm_hour,
                 time_message.tm_min,
                 time_message.tm_sec);
        return buf;
    }

    /* LevelToString —— 日志级别枚举转字符串。 */
    std::string LevelToString(const LogLevel &level)
    {
        switch (level)
        {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
        default:
            return "UNKNOW";
        }
    }
}
