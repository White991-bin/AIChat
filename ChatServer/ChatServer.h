#pragma once
#include <memory>
#include <httplib.h>
#include <ai_chat_sdk/ChatSdk.h>

namespace ai_chat_server{
//服务器配置
struct ChatServerConfig {
    std::string host="0.0.0.0";   //服务器绑定ip
    int port=8080;                //服务器端口
    std::string loglevel="INFO";  //日志级别

    //模型需要配置的信息
    float temperature=0.7;        //温度
    int max_tokens=1024;          //最大token数

    //API
    std::string DeepSeekAPIkey=""; //DeepSeek API密钥
    std::string GeminiAPIkey="";   //Gemini API密钥
    
    //ollama模型
    std::string ollama_ModelName="";//ollama模型名称
    std::string ollama_ModelDesc="";//ollama模型描述
    std::string ollama_Endpoint=""; //ollama模型地址

};
class ChatServer{
public:
    ChatServer(const ChatServerConfig& config);
    bool start();         //启动服务器
    void stop();          //停止服务器
    bool isRunning()const;//是否正在运行

private:
    //构造相应
    std::string buildResponse(const std::string& message,bool success=false);
    //处理创建会话请求
    void handleCreateSessionRequest(const httplib::Request& request,httplib::Response& response);
    //处理获取会话列表请求
    void handleGetSessionsListRequest(const httplib::Request& request,httplib::Response& response);
    //处理获取可用模型列表请求
    void handleGetAvailableModelsListRequest(const httplib::Request& request,httplib::Response& response);
    //处理获取历史消息请求
    void handleGetHistoryMessagesRequest(const httplib::Request& request,httplib::Response& response);
    //处理删除会话请求
    void handleDeleteSessionRequest(const httplib::Request& request,httplib::Response& response);
    //处理发送消息请求--全量返回
    void handleSendMessageRequest(const httplib::Request& request,httplib::Response& response);
    //处理发送消息请求--流式返回
    void handleSendMessageStreamRequest(const httplib::Request& request,httplib::Response& response);
    

    void setHttpRoutes();  //设置Http路由规则

   private:
    ChatServerConfig _config;                    //服务器配置
    std::unique_ptr<httplib::Server> _chatserver=nullptr;    //服务器实例
    std::shared_ptr<ai_chat_sdk::ChatSdk> _chat_sdk=nullptr; //聊天sdk实例
    std::atomic<bool> _isrunning={false};        //是否正在运行
};

}