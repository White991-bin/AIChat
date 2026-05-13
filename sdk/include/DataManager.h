#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <unordered_map>
#include "util/mylog.h"
#include <atomic>
#include "commen.h"
#include <mutex>

namespace ai_chat_sdk{
    class DataManager{
    public:
        DataManager(const std::string& dbName);
        ~DataManager();
        //session相关操作
        //插入新对话
        bool insertSession(const Session& session);
        //获取指定对话
        std::shared_ptr<Session> getSession(const std::string& sessionId) const;
        //更新对话的时间戳
        bool updateSessionTimestamp(const std::string& sessionId, std::time_t timestamp);
        //删除指定对话，对应的历史消息也要被删除
        bool deleteSession(const std::string& sessionId);
        //获取所有对话id
        std::vector<std::string> getAllSessionIds() const;
        //获取所有对话消息
        std::vector<std::shared_ptr<Session>> getAllSessions() const;
        //删除所有对话
        bool deleteAllSessions();
        //获取对话个数
        size_t getSessionCount() const;

        //message相关操作
        //插入新消息
        bool insertMessage(const std::string& sessionId, const ChatMessage& message);
        //获取指定对话的所有消息
        std::vector<ChatMessage> getMessagesBySessionId(const std::string& sessionId) const;
        //删除指定对话的所有消息
        bool deleteMessagesBySessionId(const std::string& sessionId);

    private:
        //初始化数据库(创建表)
        bool initDatabase();
        //执行SQL语句
        bool executeSQL(const std::string& sql);
        sqlite3* _db;
        std::string _dbName;
        mutable std::mutex _mutex;  //mutable关键字，允许const成员函数修改这个互斥锁，用于线程安全
    };


}