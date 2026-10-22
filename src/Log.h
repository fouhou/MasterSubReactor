#ifndef __LOG__H
#define __LOG__H

/* Log.h —— 流式日志库（策略模式 + RAII 落盘）。
 * 用法：LOG(LogLevel::ERROR) << "..."; 临时 LogMessage 析构时触发落盘。
 * 性能短板：File_Fflush 每条日志 open/write/close 一次文件，高并发下 IO 开销大。 */
#include <iostream>
#include <string>
#include <mutex>
#include <ios>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <sstream>
#include <time.h>
#include <memory>

namespace LogModule
{
    namespace fs = std::filesystem;

    /* 落盘策略抽象基类。 */
    class Log_Fflush_Strategy
    {
    public:
        virtual void Fflush(const std::string &message) = 0;
    };

    /* 控制台落盘实现：加锁后 std::cout 输出。 */
    class Console_Fflush : public Log_Fflush_Strategy
    {
    public:
        void Fflush(const std::string &message) override
        {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                std::cout << message << "\t\n";
            }
        }

    private:
        std::mutex _mutex;
    };

    const std::string default_file_path = "./log";
    const std::string default_file_name = "log.txt";

    /* 文件落盘实现：追加写日志到磁盘文件。
     * 每条日志 open/write/close 一次文件，高并发下 IO 开销大。 */
    class File_Fflush : public Log_Fflush_Strategy
    {
    public:
        File_Fflush(const std::string &file_path = default_file_path, const std::string &file_name = default_file_name)
            : _file_path(file_path),
              _file_name(file_name)
        {
            fs::path dir(file_path);
            try
            {
                if (fs::exists(dir))
                {
                    if (!fs::is_directory(dir))
                    {
                        std::cerr << "错误: 路径 [" << file_path << "] " << "是普通文件,无法作为目录!" << std::endl;
                        return;
                    }
                    else
                    {
                        return;
                    }
                }
                else
                {
                    fs::create_directories(dir);
                }
            }
            catch (const fs::filesystem_error &err)
            {
                std::cerr << "文件系统异常: " << err.what() << std::endl;
            }
        }

        void Fflush(const std::string &message) override
        {
            std::string file = _file_path + (_file_path.back() == '/' ? "" : "/") + _file_name;

            {
                std::lock_guard<std::mutex> lock(_mutex);
                std::ofstream out_file(file, std::ios::app);

                if (!out_file.is_open())
                {
                    std::cerr << "日志文件打开失败: " << file << std::endl;
                }
                out_file << message << "\t\n";

                out_file.close();
            }
        }

    private:
        std::mutex _mutex;
        std::string _file_path;
        std::string _file_name;
    };

    /* 日志级别枚举。 */
    enum class LogLevel
    {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    std::string Get_Time();
    std::string LevelToString(const LogLevel &level);

    /* 日志门面类：持有当前落盘策略，operator() 返回 LogMessage 临时对象。 */
    class Log
    {
    public:
        Log()
        {
            Use_Console_Fflush_Strategy();
        }

        void Use_Console_Fflush_Strategy()
        {
            _strategy = std::make_unique<Console_Fflush>();
        }

        void Use_File_Fflush_Strategy(const std::string &file_path = default_file_path, const std::string &file_name = default_file_name)
        {
            _strategy = std::make_unique<File_Fflush>(file_path, file_name);
        }

        /* 单条日志的临时载体：构造拼头部，<< 追加正文，析构落盘。 */
        class LogMessage
        {
        public:
            LogMessage(const LogLevel level, const std::string &file_name, int line, Log &log)
                : _time(Get_Time()),
                  _file_name(file_name),
                  _line(line),
                  _pid(getpid()),
                  _level(level),
                  _log(log)
            {
                std::stringstream ss;
                ss << "[" << _time << "] "
                   << "[" << LevelToString(level) << "] "
                   << "[" << _pid << "] "
                   << "[" << _file_name << "] "
                   << "[" << _line << "] "
                   << "- ";
                _message = ss.str();
            }
            template <typename T>
            LogMessage &operator<<(const T &message)
            {
                std::stringstream ss;
                ss << message;
                _message += ss.str();

                return *this;
            }
            ~LogMessage()
            {
                if (_log._strategy)
                {
                    _log._strategy->Fflush(_message);
                }
            }

        private:
            std::string _time;
            LogLevel _level;
            pid_t _pid;
            std::string _file_name;
            int _line;
            std::string _message;
            Log &_log;
        };

        LogMessage operator()(const LogLevel level, const std::string &file_name, int line)
        {
            return LogMessage(level, file_name, line, *this);
        }
        ~Log()
        {
        }

    private:
        std::unique_ptr<Log_Fflush_Strategy> _strategy;
    };
    extern Log log;
#define LOG(x) log(x, __FILE__, __LINE__)
#define USECONSOLESTRATEGY() log.Use_Console_Fflush_Strategy()
#define USEFILESTRATEGY() log.Use_File_Fflush_Strategy()
}

#endif
