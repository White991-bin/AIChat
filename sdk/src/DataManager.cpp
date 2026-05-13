#include "../include/DataManager.h"
#include "../include/commen.h"

namespace ai_chat_sdk{
    DataManager::DataManager(const std::string& dbName):
        _db(nullptr),
        _dbName(dbName){
        //创建或打开数据库连接
        int ret=sqlite3_open(dbName.c_str(), &_db);
        if(ret!=SQLITE_OK){
            sqlite3_close(_db);
            _db=nullptr;
            ERROR("Failed to open database: ",_dbName);
        }
        INFO("Database connection opened");
        //初始化数据库(创建表)
        if(!initDatabase()){
            sqlite3_close(_db);
            _db=nullptr;
            ERROR("Failed to initialize database: ",_dbName);
        }
    }
    DataManager::~DataManager()
    {
        if(_db!=nullptr){
            sqlite3_close(_db);
            _db=nullptr;
            INFO("Database connection closed");
        }
    }
    bool DataManager::initDatabase(){
        std::lock_guard<std::mutex> lock(_mutex);
        //创建sessions表
        const std::string createSessionsTable=
        "create table if not exists sessions("
            "session_id Text  primary key,"
            "model_name Text not null,"
            "created_time Integer not null,"
            "updated_time Integer not null"");";

        if(!executeSQL(createSessionsTable)){
            return false;
        }

        //创建messages表
        const std::string createMessagesTable=
        "create table if not exists messages("
            "message_id Text  primary key,"
            "session_id Text not null,"
            "role Text not null,"
            "content Text not null,"
            "timestamp Integer not null,"
            "foreign key(session_id) references sessions(session_id) on delete cascade"
            ");";

        if(!executeSQL(createMessagesTable)){
            return false;
        }
        
        //创建索引以加速查询
        const std::string createIndex=
        "create index if not exists idx_messages_session_id on messages(session_id);";
        if(!executeSQL(createIndex)){
            return false;
        }
        INFO("Database initialized Successfully");
        return true;
    }

    bool DataManager::executeSQL(const std::string& sql){
        char* errmsg=nullptr;
        int ret=sqlite3_exec(_db,sql.c_str(),nullptr,nullptr,&errmsg);
        if(ret!=SQLITE_OK){
            ERROR("Failed to execute SQL: ",errmsg);
            sqlite3_free(errmsg);
            return false;
        }
        return true;
    }

    bool DataManager::insertSession(const Session& session){
        std::lock_guard<std::mutex> lock(_mutex);
        const std::string sql="insert into sessions(session_id,model_name,created_time,updated_time) values(?,?,?,?)";
        
        //准备SQL语句
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(_db,sql.c_str(),-1,&stmt,nullptr);
        if(rc!=SQLITE_OK){
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return false;
        }

        sqlite3_bind_text(stmt,1,session.id.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt,2,session.model_name.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt,3,static_cast<int64_t>(session.created_time));
        sqlite3_bind_int64(stmt,4,static_cast<int64_t>(session.updated_time));
        
        //执行SQL语句,检查是否插入成功
        rc=sqlite3_step(stmt);
        if(rc!=SQLITE_DONE){
            ERROR("Failed to insert session: {}",sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_finalize(stmt);
        INFO("Session inserted successfully: {}",session.id);
        return true;
    }
    //获取指定会话
    std::shared_ptr<Session> DataManager::getSession(const std::string& sessionId)const{
        std::lock_guard<std::mutex> lock(_mutex);
        const std::string sql="select model_name,created_time,updated_time from sessions where session_id=?";
        
        //准备SQL语句
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(_db,sql.c_str(),-1,&stmt,nullptr);
        if(rc!=SQLITE_OK){
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return nullptr;
        }
        //绑定参数
        sqlite3_bind_text(stmt,1,sessionId.c_str(),-1,SQLITE_TRANSIENT);

        //执行SQL语句,检查是否查询到结果
        rc=sqlite3_step(stmt);
        if(rc!=SQLITE_ROW){
            sqlite3_finalize(stmt);
            return nullptr;
        }

        //查询到结果，创建会话对象
        std::string modelName(reinterpret_cast<const char*>(sqlite3_column_text(stmt,0)));
        auto session=std::make_shared<Session>(modelName);
        session->id=sessionId;
        session->created_time=static_cast<std::time_t>(sqlite3_column_int64(stmt,1));
        session->updated_time=static_cast<std::time_t>(sqlite3_column_int64(stmt,2));
        //释放stmt
        sqlite3_finalize(stmt);

        //获取历史内容
       
        session->messages=getMessagesBySessionId(sessionId);
        return session;
    }
    //更新会话时间戳
    bool DataManager::updateSessionTimestamp(const std::string& sessionId, std::time_t timestamp){
        std::lock_guard<std::mutex> lock(_mutex);
        const std::string sql="update sessions set updated_time=? where session_id=?";
        
        //准备SQL语句
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(_db,sql.c_str(),-1,&stmt,nullptr);
        if(rc!=SQLITE_OK){
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return false;
        }
        //绑定参数
        sqlite3_bind_int64(stmt,1,static_cast<int64_t>(timestamp));
        sqlite3_bind_text(stmt,2,sessionId.c_str(),-1,SQLITE_TRANSIENT);
        
        //执行SQL语句,检查是否更新成功
        rc=sqlite3_step(stmt);
        if(rc!=SQLITE_DONE){
            ERROR("Failed to update session timestamp: {}",sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_finalize(stmt);
        INFO("Session timestamp updated successfully: {}",sessionId);
        return true;
    }

    //获取所有会话id
    std::vector<std::string> DataManager::getAllSessionIds()const{
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::string> sessionIds;
        const std::string sql="select session_id from sessions";
        
        //准备SQL语句
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(_db,sql.c_str(),-1,&stmt,nullptr);
        if(rc!=SQLITE_OK){
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return sessionIds;
        }
        
        
        //查询到结果，创建会话对象
        while((rc=sqlite3_step(stmt))==SQLITE_ROW){
            sessionIds.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt,0)));
        }
        sqlite3_finalize(stmt);
        return sessionIds;
    }
    //获取所有Session信息，并按照更新时间降序排序
    std::vector<std::shared_ptr<Session>> DataManager::getAllSessions()const{
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::shared_ptr<Session>> sessions;
        const std::string sql="select session_id,model_name,created_time,updated_time from sessions order by updated_time desc";
        
        //准备SQL语句
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(_db,sql.c_str(),-1,&stmt,nullptr);
        if(rc!=SQLITE_OK){
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return sessions;
        }
        
        
        //查询到结果，创建会话对象
        while((rc=sqlite3_step(stmt))==SQLITE_ROW){
            std::string sessionId(reinterpret_cast<const char*>(sqlite3_column_text(stmt,0)));
            std::string modelName(reinterpret_cast<const char*>(sqlite3_column_text(stmt,1)));
            std::time_t createdTime=static_cast<std::time_t>(sqlite3_column_int64(stmt,2));
            std::time_t updatedTime=static_cast<std::time_t>(sqlite3_column_int64(stmt,3));
            sessions.push_back(std::make_shared<Session>(modelName));
            sessions.back()->id=sessionId;
            sessions.back()->created_time=createdTime;
            sessions.back()->updated_time=updatedTime;
        }
        sqlite3_finalize(stmt);
        return sessions;
    }

    //获取会话总个数
    size_t DataManager::getSessionCount()const{
        std::lock_guard<std::mutex> lock(_mutex);
        const std::string sql="select count(*) from sessions";
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(_db,sql.c_str(),-1,&stmt,nullptr);
        if(rc!=SQLITE_OK){
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return 0;
        }

        //执行SQL语句,检查是否查询到结果
        rc=sqlite3_step(stmt);
        size_t count=0;
        if(rc==SQLITE_ROW){
            count=static_cast<size_t>(sqlite3_column_int64(stmt,0));
        }
        sqlite3_finalize(stmt);
        INFO("Session count: {}",count);
        return count;
    }
    //删除所有会话
    bool DataManager::deleteAllSessions(){
        std::lock_guard<std::mutex> lock(_mutex);
        //构建SQL语句
        const std::string sql="delete from sessions";
        
        //准备SQL语句
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(_db,sql.c_str(),-1,&stmt,nullptr);
        if(rc!=SQLITE_OK){
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return false;
        }
        
        //执行SQL语句,检查是否删除成功
        rc=sqlite3_step(stmt);
        if(rc!=SQLITE_DONE){
            ERROR("Failed to delete all sessions: {}",sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_finalize(stmt);
        INFO("All sessions deleted successfully");
        return true;
    }

    //删除指定会话的所有消息和会话
    bool DataManager::deleteSession(const std::string& sessionId){
    std::lock_guard<std::mutex> lock(_mutex);
    
    // 先删除该会话的所有消息
    std::string deleteMessagesSQL = "DELETE FROM messages WHERE session_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(_db, deleteMessagesSQL.c_str(), -1, &stmt, nullptr);
    if(rc != SQLITE_OK){
        ERROR("deleteSession - 准备删除消息语句失败：{}", sqlite3_errmsg(_db));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if(rc != SQLITE_DONE){
        WARN("deleteSession - 删除消息失败（可能没有消息）：{}", sqlite3_errmsg(_db));
        // 不返回 false，继续删除会话
    } else {
        INFO("deleteSession - 删除会话 {} 的所有消息成功", sessionId);
    }
    
    // 删除会话
    std::string deleteSessionSQL = "DELETE FROM sessions WHERE session_id = ?;";
    
    rc = sqlite3_prepare_v2(_db, deleteSessionSQL.c_str(), -1, &stmt, nullptr);
    if(rc != SQLITE_OK){
        ERROR("deleteSession - 准备删除会话语句失败：{}", sqlite3_errmsg(_db));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if(rc != SQLITE_DONE){
        ERROR("deleteSession - 删除会话失败：{}", sqlite3_errmsg(_db));
        return false;
    }
    
    INFO("deleteSession - 删除会话成功：{}", sessionId);
    return true;
}
    //向指定会话插入消息
    bool DataManager::insertMessage(const std::string& sessionId,const ChatMessage& message){
        std::string sql="insert into messages (message_id,session_id,role,content,timestamp) values (?,?,?,?,?)";   
        //准备SQL语句
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(_db,sql.c_str(),-1,&stmt,nullptr);
        if(rc!=SQLITE_OK){
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return false;
        }

        //绑定参数
        sqlite3_bind_text(stmt,1,message.id.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt,2,sessionId.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt,3,message.role.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt,4,message.content.c_str(),-1,SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt,5,static_cast<int64_t>(message.timestamp));
        //执行SQL语句,检查是否插入成功
        rc=sqlite3_step(stmt);
        if(rc!=SQLITE_DONE){
            ERROR("Failed to insert message: {}",sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_finalize(stmt);

        //同时更新session的updated_time
        const std::string updateSessionSql="update sessions set updated_time=? where session_id=?";
        sqlite3_stmt* updateStmt;
        rc=sqlite3_prepare_v2(_db,updateSessionSql.c_str(),-1,&updateStmt,nullptr); 
        
        if(rc==SQLITE_OK){
             //绑定参数
        sqlite3_bind_int64(updateStmt,1,static_cast<int64_t>(message.timestamp));
        sqlite3_bind_text(updateStmt,2,sessionId.c_str(),-1,SQLITE_TRANSIENT);
        //执行SQL语句,检查是否更新成功
        rc=sqlite3_step(updateStmt);
        }
        else{
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return false;
        }
       
        DEBUG("Inserted message:{} into session: {}",message.id,sessionId);
        return true;
    }
    //获取指定会话历史消息
    std::vector<ChatMessage> DataManager::getMessagesBySessionId(const std::string& sessionId)const {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<ChatMessage> messages;
        const std::string sql="select message_id,role,content,timestamp from messages where session_id=? order by timestamp ASC";
        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(_db,sql.c_str(),-1,&stmt,nullptr);
        if(rc!=SQLITE_OK){
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return messages;
        }
        //绑定参数
        sqlite3_bind_text(stmt,1,sessionId.c_str(),-1,SQLITE_TRANSIENT);    

        while(rc=sqlite3_step(stmt)==SQLITE_ROW){
            ChatMessage msg;
            msg.id=(reinterpret_cast<const char*>(sqlite3_column_text(stmt,0)));
            msg.role=(reinterpret_cast<const char*>(sqlite3_column_text(stmt,1)));
            msg.content=(reinterpret_cast<const char*>(sqlite3_column_text(stmt,2)));
            msg.timestamp=static_cast<std::time_t>(sqlite3_column_int64(stmt,3));
            messages.push_back(msg);
        } 
        sqlite3_finalize(stmt);
        return messages;
    }
    //删除指定会话的历史消息
    bool DataManager::deleteMessagesBySessionId(const std::string& sessionId){
        std::lock_guard<std::mutex> lock(_mutex);
        const std::string sql="delete from messages where session_id=?";

        sqlite3_stmt* stmt;
        int rc=sqlite3_prepare_v2(_db,sql.c_str(),-1,&stmt,nullptr);
        if(rc!=SQLITE_OK){
            ERROR("Failed to prepare SQL: {}",sqlite3_errmsg(_db));
            return false;
        }
        //绑定参数
        sqlite3_bind_text(stmt,1,sessionId.c_str(),-1,SQLITE_TRANSIENT);    
        //执行SQL语句,检查是否删除成功
        rc=sqlite3_step(stmt);
        if(rc!=SQLITE_DONE){
            ERROR("Failed to delete messages: {}",sqlite3_errmsg(_db));
            return false;
        }
        sqlite3_finalize(stmt);
        INFO("Messages deleted successfully from session: {}",sessionId);
        return true;
    }
}
