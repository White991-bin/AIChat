#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <unordered_map>
#include "util/mylog.h"
#include <atomic>
#include <mutex>
#include "DataManager.h"
using timestamp_t = std::time_t;
namespace ai_chat_sdk{
    class SessionManager{
    public:
        SessionManager(const std::string& dbName="chatDB.db");
        ~SessionManager()=default;
        //创建新对话，返回对话ID
        std::string createSession(const std::string& modelname);

        //获得对话对象，返回对话指针
        std::shared_ptr<Session> getSession(const std::string& sessionid);

        //添加消息到对话
        bool addMessage(const std::string& sessionid,const ChatMessage& message);

        //获取对话消息列表
        std::vector<ChatMessage> getMessagesHistory(const std::string& sessionid);

        //获取所有会话ID
        std::vector<std::string> getSessionsList() const;
       
        //删除对话
        bool deleteSession(const std::string& sessionid);
        
        //更新会话事件戳
        bool updateSessionTimestamp(const std::string& sessionid,const timestamp_t& timestamp);

        //清除所有会话
        void clearAllSessions();
        
        //获取会话总数
        size_t getSessionCount() const;
    private:
        //生成唯一会话ID
        std::string generateSessionID();
        
        //生成唯一消息ID
        std::string generateMessageID();
    private:
        //key:会话ID，value:会话对象指针
        std::unordered_map<std::string,std::shared_ptr<Session>> _sessions;
        mutable std::mutex _mutex;  //在const成员函数中，可能修改锁的状态。用mutable关键字声明
        //会话ID计数器
        static std::atomic<int64_t> _sessionIDCounter;  //会话总数
        //消息ID计数器
        static std::atomic<int64_t> _messageIDCounter;  //消息总数

        //通过会话数据到数据库
        DataManager _datamanager;
    };
   
}