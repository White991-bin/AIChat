#include "../include/util/mylog.h"
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <string>
namespace Bite
{
    std::shared_ptr<spdlog::logger> Logger::_logger=nullptr;
    std::mutex Logger::_mutex;
    Logger::Logger()
    {}
    
    void Logger::InitLogger(const std::string& loggername, const std::string& loggerfile,
                        spdlog::level::level_enum loglevel)
    {
        if(nullptr==_logger)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if(nullptr==_logger)
            {
                //设置全局自动刷新级别，当日志级别 >=loglevel时,刷新到文件
                spdlog::flush_on(loglevel);
                //启动异步日志，即将日志信息放入队列，由后台线程处理
                //参数1：队列大小，参数2，后台线程数量
                spdlog::init_thread_pool(32768,1);
                if("stdout"==loggerfile)
                {
                    //创建带颜色的输出到控制台的日志器
                    _logger=spdlog::stdout_logger_mt(loggername);
                }
                else
                {
                    //创建带颜色的输出到文件的日志器
                    _logger=spdlog::basic_logger_mt(loggername,loggerfile);
                    
                }
            }
            //格式设置
            //[%H:%H:%s] 时分秒
            //[%n] 日志器名称
            //%-7l 日志级别
            //%v 日志消息
            _logger->set_pattern("[%H:%M:%s][%n][%-7l]%v");
            _logger->set_level(loglevel);
        }
    }
    std::shared_ptr<spdlog::logger> Logger::get_logger()
    {
        return _logger;
    }
}//end Bite
