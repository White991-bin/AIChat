#pragma once 
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <mutex>

namespace Bite{
    class Logger{
        public:
        static void InitLogger(const std::string& logger_name,const std::string& logger_file,
                spdlog::level::level_enum loglevel = spdlog::level::info);
        static std::shared_ptr<spdlog::logger> get_logger();
        private:
        Logger();
        Logger(const Logger&)=delete;
        Logger& operator=(const Logger&)=delete;
        
        private:
        static std::shared_ptr<spdlog::logger> _logger;
        static std::mutex _mutex;
    };
}

//#include <format>
//std::string s=std::format("hello {}","world");

#define TRACE(format,...) Bite::Logger::get_logger()->trace(std::string("[{:>10s}:{:<4d}]")+format,__FILE__,__LINE__,##__VA_ARGS__)
#define DEBUG(format,...) Bite::Logger::get_logger()->debug(std::string("[{:>10s}:{:<4d}]")+format,__FILE__,__LINE__,##__VA_ARGS__)
#define INFO(format,...) Bite::Logger::get_logger()->info(std::string("[{:>10s}:{:<4d}]")+format,__FILE__,__LINE__,##__VA_ARGS__)
#define WARN(format,...) Bite::Logger::get_logger()->warn(std::string("[{:>10s},{:<4d}]")+format,__FILE__,__LINE__,##__VA_ARGS__)
#define ERROR(format,...) Bite::Logger::get_logger()->error(std::string("[{:>10s},{:<4d}]")+format,__FILE__,__LINE__,##__VA_ARGS__)
