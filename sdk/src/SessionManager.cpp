#include "../include/SessionManager.h"
#include "../include/util/mylog.h"
#include <sstream>
#include <iomanip>
using timestamp_t = int64_t;

namespace ai_chat_sdk{

    std::atomic<int64_t> SessionManager::_messageIDCounter{0};  //消息总数
    std::atomic<int64_t> SessionManager::_sessionIDCounter{0};  //会话总数

    SessionManager::SessionManager(const std::string& dbName)
    :_datamanager(dbName)
    {
        //获取所有会话
        auto sessions=_datamanager.getAllSessions();
        for(auto& session:sessions){
            _sessions[session->id]=session;
        }
        INFO("SessionManager init: session count {}",_sessions.size());
    }

    //生成消息id
    std::string SessionManager::generateMessageID(){
        //消息计数自增
        _messageIDCounter.fetch_add(1);
        std::time_t time=std::time(nullptr);
        
        //消息id格式为 msg_时间戳_消息计数器
        std::ostringstream os;
        os<<"msg_"<<time<<"_"<<std::setfill('0')<<std::setw(8)<<_messageIDCounter;
        return os.str();
    }
    //生成会话id
    std::string SessionManager::generateSessionID(){
        //会话计数自增
        _sessionIDCounter.fetch_add(1);
        std::time_t time=std::time(nullptr);
        
        //会话id格式为 sess_时间戳_会话计数器
        std::ostringstream os;
        os<<"sess_"<<time<<"_"<<std::setfill('0')<<std::setw(8)<<_sessionIDCounter;
        return os.str();
    }

    //创建会话
    std::string SessionManager::createSession(const std::string& modelname){
        std::lock_guard<std::mutex> lock(_mutex);
        //生成会话id
        std::string sessionid=generateSessionID();

        //创建会话，设置Sessionid
        auto session=std::make_shared<Session>(modelname);
        session->created_time=std::time(nullptr);
        session->updated_time=std::time(nullptr);
        session->id=sessionid;

        //将会话添加到会话列表
        _sessions[sessionid]=session;
        INFO("createSession: sessionid {},modelname {}",sessionid,modelname);
        //将会话添加到数据库
        _datamanager.insertSession(*session);
        return sessionid;
    }

    //通过会话id获取会话信息
    std::shared_ptr<Session> SessionManager::getSession(const std::string& sessionid){
       // 先在内存中查找
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sessionid);
    if(it != _sessions.end()){
        // 获取当前会话的历史消息
        it->second->messages = _datamanager.getMessagesBySessionId(sessionid);
        return it->second;
    }

    // 内存中没有找到，从数据库中查找
    auto session = _datamanager.getSession(sessionid);
    if(session){
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _sessions.find(sessionid);
        if(it == _sessions.end()){
            // 内存中没有找到，将会话添加到会话列表
            _sessions[sessionid] = session; 
        }
        // 获取当前会话的历史消息
        session->messages = _datamanager.getMessagesBySessionId(sessionid);
        return session;
    }

    WARN("sessionid = {} not found", sessionid);
    return nullptr;
    }   
    

    //向某个会话添加消息
    bool SessionManager::addMessage(const std::string& sessionid,const ChatMessage& message){
        
        std::lock_guard<std::mutex> lock(_mutex);
        //获取Session的会话
        auto it=_sessions.find(sessionid);
        if(it==_sessions.end()){
            ERROR("addMessage: sessionid {} not found",sessionid);
            return false;
        }
        //创建消息，设置MessageID
        ChatMessage msg(message.role,message.content);
        msg.id=generateMessageID();
        msg.timestamp = std::time(nullptr);  // 设置消息时间戳
        INFO("message Info: content {}  timestamap {}", msg.content, msg.timestamp); 
       
        //添加消息到会话列表

        it->second->messages.push_back(msg);
        it->second->updated_time=std::time(nullptr);
        INFO("addMessage: sessionid {},message {}",sessionid,msg.id);
        //将会消息添加到数据库
        _datamanager.insertMessage(sessionid,msg);
        INFO("addMessage: sessionid {},message {}",sessionid,msg.id);
        return true;
    }
    //获取会话历史
    std::vector<ChatMessage> SessionManager::getMessagesHistory(const std::string& sessionid){
        // 先从内存中获取会话消息，如果内存中获取不到，再到数据库中获取
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _sessions.find(sessionid);
        if(it != _sessions.end()){
            return it->second->messages;
        }
    
    // 从数据库中获取消息列表
    return _datamanager.getMessagesBySessionId(sessionid);
    }

    //获取所有会话列表,包含Sessionid和模型名称
    std::vector<std::string> SessionManager::getSessionsList() const{
       
        auto sessions = _datamanager.getAllSessions();

        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::pair<std::time_t, std::shared_ptr<Session>>> temp;
        temp.reserve(_sessions.size());
        
        //填充临时会话
        for(const auto &session:_sessions){
            temp.emplace_back(session.second->updated_time,session.second);
        }
         // 将数据库中的会话添加到临时列表中
        for(const auto& session : sessions){
            if(_sessions.find(session->id) == _sessions.end()){
            temp.emplace_back(session->updated_time, session);
            }
        }
    
        std::sort(temp.begin(),temp.end(),[](const auto &a,const auto &b){
            return a.first>b.first;
        });

        //构造返回列表
        std::vector<std::string> session_ids;
        session_ids.reserve(_sessions.size());
        for(const auto &pair:temp){
            session_ids.push_back(pair.second->id);
        }
        
        return session_ids;
    }

    //删除会话
    bool SessionManager::deleteSession(const std::string& sessionid){
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _sessions.find(sessionid);
        if(it == _sessions.end()){
            ERROR("deleteSession: sessionid {} not found",sessionid);
            return false;
        }

        // 从内存中删除会话
        _sessions.erase(it);
        
        // 从数据库中删除会话
        _datamanager.deleteSession(sessionid);
        return true;
    }
    //更新会话时间戳
    bool SessionManager::updateSessionTimestamp(const std::string& sessionid,const timestamp_t& timestamp){
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _sessions.find(sessionid);
    if(it != _sessions.end()){
        it->second->updated_time = timestamp;
        _datamanager.updateSessionTimestamp(sessionid, timestamp);
        return true;
    }
    return false;
}
    //清除所有会话
    void SessionManager::clearAllSessions(){
        _mutex.lock();
        INFO("clearAllSessions: all sessions cleared");
        _sessions.clear();
         // 清空数据库中的所有会话
        _datamanager.deleteAllSessions();
        _mutex.unlock();
    }

    //获取会话总数
    size_t SessionManager::getSessionCount() const{
        std::lock_guard<std::mutex> lock(_mutex);
        return _sessions.size();
    }
   }//end ai_chat_sdk
