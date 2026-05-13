#include "ChatServer.h"
#include <httplib.h>
#include <jsoncpp/json/forwards.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>
#include <ai_chat_sdk/util/mylog.h>
#include <typeinfo>
#include <cxxabi.h>
namespace ai_chat_server{
        ChatServer::ChatServer(const ChatServerConfig& config){
            _config=config;
            _chat_sdk=std::make_shared<ai_chat_sdk::ChatSdk>();
        
            //std::cout << "Test function: " << _chat_sdk->testFunction() << std::endl;
            auto deepseekConfig=std::make_shared<ai_chat_sdk::ApiConfig>();
            deepseekConfig->model_name="deepseek-chat";
            deepseekConfig->api_key=config.DeepSeekAPIkey;
            deepseekConfig->temperature=std::to_string(config.temperature);
            deepseekConfig->max_tokens=std::to_string(config.max_tokens);

             // gpt-4o-mini
            // auto chatGPTConfig = std::make_shared<ai_chat_sdk::APIConfig>();
            // chatGPTConfig->_modelName = "gpt-4o-mini";
            // chatGPTConfig->_apiKey = config.chatGPTAPIKey;
            // chatGPTConfig->_temperature = config.temperature;
            // chatGPTConfig->_maxTokens = config.maxTokens;

            auto geminiConfig=std::make_shared<ai_chat_sdk::ApiConfig>();
            geminiConfig->model_name="gemini-2.5-flash";
            geminiConfig->api_key=config.GeminiAPIkey;
            geminiConfig->temperature=std::to_string(config.temperature);
            geminiConfig->max_tokens=std::to_string(config.max_tokens);

            //ollama本地接入
            auto ollamaConfig=std::make_shared<ai_chat_sdk::OllamaConfig>();
            ollamaConfig->model_name=config.ollama_ModelName;
            ollamaConfig->model_desc=config.ollama_ModelDesc;
            ollamaConfig->endpoint=config.ollama_Endpoint;
            ollamaConfig->temperature=std::to_string(config.temperature);
            ollamaConfig->max_tokens=std::to_string(config.max_tokens);
        
            std::vector<std::shared_ptr<ai_chat_sdk::ChatModelConfig>> modelConfigs={deepseekConfig,geminiConfig,ollamaConfig};

            
            INFO("start init ChatSdk");
            if(!_chat_sdk->initModels(modelConfigs)){
                ERROR("init ChatSdk failed");
                return;
            }
            INFO("init ChatSdk success");

            //创建Http服务器
            _chatserver=std::make_unique<httplib::Server>();
            if(!_chatserver){
                ERROR("create HttpServer failed");
                return;
            }
            INFO("create HttpServer success");
           
    };
    
    bool ChatServer::start(){
        if(_isrunning.load()){
            ERROR("ChatServer is running!");
            return false;
        }
        setHttpRoutes();

        //设置静态资源路径
        // 在httplib中，默认情况下，如果请求路径中只有ip和端口，httplib默认会使用index.html文件
        _chatserver->set_mount_point("/", "./www");

        std::thread serverThread([this](){
            _chatserver->listen(_config.host,_config.port);
            INFO("ChatServer start on {},{}",_config.host,_config.port);
        });
        serverThread.detach();
        _isrunning.store(true);
        INFO("ChatServer start success");
        return true;
    }
    void ChatServer::stop(){
        if(!_isrunning.load()){
            ERROR("ChatServer is not running!");
            return;
        }
        _isrunning.store(false);
        _chatserver->stop();
        INFO("ChatServer stop success");
    }
    bool ChatServer::isRunning()const{
        return _isrunning.load();
    }

    //构造响应
    std::string ChatServer::buildResponse(const std::string& message,bool success){
        Json::Value responsejson;
        responsejson["message"]=message;
        responsejson["success"]=success;
   
        //序列化
        Json::StreamWriterBuilder writebuilder;
        return Json::writeString(writebuilder,responsejson);
    }    
    void ChatServer::handleCreateSessionRequest(const httplib::Request& request,httplib::Response& response){
        //反序列化获取请求体Json
        Json::Value requestjson;
        Json::Reader reader;
        
        if(!reader.parse(request.body,requestjson)){
            std::string errorjsonstr=buildResponse("parse request body failed,requestjson format error",false);
            response.status=400;  //客户端发送的请求有误，服务端无法解析
            response.set_content(errorjsonstr,"application/json");
            return;
        }

        std::string modelname=requestjson.get("model","deepseek-chat").asString();
        
        //创建会话
        std::string sessionid=_chat_sdk->createSession(modelname);
        if(sessionid.empty()){
            std::string errorjsonstr=buildResponse("create session failed",false);
            response.status=500;  //服务器内部错误
            response.set_content(errorjsonstr,"application/json");
            return;
        }

        //构建响应体
        Json::Value datajson;
        datajson["sessionid"]=sessionid;
        datajson["modelname"]=modelname;
        Json::Value responsejson;
        responsejson["message"]="create session success";
        responsejson["success"]=true;
        responsejson["data"]=datajson;
        
        //序列化
        Json::StreamWriterBuilder writebuilder;
        std::string responsejsonstr=Json::writeString(writebuilder,responsejson);
        
        response.status=200;  //请求成功
        response.set_content(responsejsonstr,"application/json");
    }

    //处理获取会话列表请求
    void ChatServer::handleGetSessionsListRequest(const httplib::Request& request,httplib::Response& response){
        //获取会话列表
        std::vector<std::string> sessionsids=_chat_sdk->getSessionsList();
        
       //构建session信息
       Json::Value dataArray(Json::arrayValue);
       for(const auto& sessionid:sessionsids){
            auto session=_chat_sdk->getSession(sessionid);
            if(session){
            Json::Value sessionjson;
            sessionjson["id"]=sessionid;
            sessionjson["model"]=session->model_name;
            sessionjson["created_at"]=static_cast<int64_t>(session->created_time);
            sessionjson["updated_at"]=static_cast<int64_t>(session->updated_time);
            sessionjson["message_count"]=session->messages.size();
            if(!session->messages.empty()){
               sessionjson["first_user_message"]=session->messages.front().content;
            }
            dataArray.append(sessionjson);
        }
       }
       //构建响应体
       Json::Value responsejson;
       responsejson["message"]="get sessions list success";
       responsejson["success"]=true;
       responsejson["data"]=dataArray;
       //序列化
       Json::StreamWriterBuilder writebuilder;
       std::string responsejsonstr=Json::writeString(writebuilder,responsejson);
       response.status=200;  //请求成功
       response.set_content(responsejsonstr,"application/json");
    }

    //处理获取历史消息请求
    void ChatServer::handleGetHistoryMessagesRequest(const httplib::Request& request,httplib::Response& response){

        std::cout<<"test11"<<std::endl;
        std::string sessionid=request.matches[1];
        Json::Value dataArray(Json::arrayValue);
        auto session=_chat_sdk->getSession(sessionid);
        std::cout<<"session:"<<session<<std::endl;
        if(session){
            for(const auto& msg:session->messages){
                Json::Value messagejson;
                messagejson["id"]=msg.id;
                messagejson["role"]=msg.role;
                messagejson["content"]=msg.content;
                messagejson["timestamp"]=static_cast<int64_t>(msg.timestamp);
                dataArray.append(messagejson);
            }
        }
        //构建响应体
        Json::Value responsejson;
        responsejson["message"]="get session history messages success";
        responsejson["success"]=true;
        responsejson["data"]=dataArray;
        //序列化
        Json::StreamWriterBuilder writebuilder;
        std::string responsejsonstr=Json::writeString(writebuilder,responsejson);
        response.status=200;  //请求成功
        response.set_content(responsejsonstr,"application/json");
           
    }

    //处理获取可用模型列表请求
    void ChatServer::handleGetAvailableModelsListRequest(const httplib::Request& request,httplib::Response& response){
        auto models=_chat_sdk->getAvailableModels();
        Json::Value dataArray(Json::arrayValue);
        for(const auto& model:models){
            Json::Value modeljson;
            modeljson["name"]=model._name;
            modeljson["desc"]=model._desc;
            dataArray.append(modeljson);
        }
        //构建响应体
        Json::Value responsejson;
        responsejson["message"]="get available models success";
        responsejson["success"]=true;
        responsejson["data"]=dataArray;
        //序列化
        Json::StreamWriterBuilder writebuilder;
        std::string responsejsonstr=Json::writeString(writebuilder,responsejson);
        response.status=200;  //请求成功
        response.set_content(responsejsonstr,"application/json");
    }
    //处理删除会话请求
    void ChatServer::handleDeleteSessionRequest(const httplib::Request& request,httplib::Response& response){
        std::string sessionid=request.matches[1];

        if(_chat_sdk->deleteSession(sessionid)){
            std::cout<<"delete session success"<<std::endl;
            std::string errorjsonstr=buildResponse("delete session success",true);
            response.status=200;  //请求成功
            response.set_content(errorjsonstr,"application/json");
        }else{
            std::string errorjsonstr=buildResponse("delete session failed, session not found",false);
            response.status=404;  //会话不存在
            response.set_content(errorjsonstr,"application/json");
        }
    }

    //处理发送消息请求--全量返回
    void ChatServer::handleSendMessageRequest(const httplib::Request& request,httplib::Response& response){
        Json::Value requestjson;
        Json::Reader reader;
        if(!reader.parse(request.body,requestjson)){
            std::string errorjsonstr=buildResponse("parse request body failed,format error",false);
            response.status=400;  //请求体无效
            response.set_content(errorjsonstr,"application/json");
            return;
        }
        
   
        std::string sessionid=requestjson["session_id"].asString();
        std::string message=requestjson["message"].asString();
        if(sessionid.empty()||message.empty()){
            std::string errorjsonstr=buildResponse("session_id or message is empty",false);
            response.status=400;  //请求体无效
            response.set_content(errorjsonstr,"application/json");
            return;
        }

    
        //发送消息（是服务器向AI模型发送消息，返回模型回复）
        if(!_chat_sdk){
            ERROR("ChatServer sendMessage: chat sdk not initialized");
            return;
        }
        std::string assistantMessage= _chat_sdk->sendMessage(sessionid,message);
        std::cout<<"assistantMessage:"<<assistantMessage<<std::endl;
        if(assistantMessage.empty()){
            std::string errorjsonstr=buildResponse("send message failed",false);
            response.status=500;  //发送消息失败
            response.set_content(errorjsonstr,"application/json");
            return;
        }
        
        
        //构建响应参数
        Json::Value datajson;
        datajson["session_id"]=sessionid;
        datajson["response"]=assistantMessage;
        datajson["data"]["assistant_message"] = assistantMessage;
        //构建响应体
        Json::Value responsejson;
        responsejson["message"]="send message success";
        responsejson["success"]=true;
        responsejson["data"]=datajson;
        //序列化
        Json::StreamWriterBuilder writebuilder;
        std::string responsejsonstr=Json::writeString(writebuilder,responsejson);
        response.status=200;  //请求成功
        response.set_content(responsejsonstr,"application/json");
    }

    void ChatServer::handleSendMessageStreamRequest(const httplib::Request& request,httplib::Response& response){
        Json::Value requestjson;
        Json::Reader reader;
        if(!reader.parse(request.body,requestjson)){
            std::string errorjsonstr=buildResponse("parse request body failed,format error",false);
            response.status=400;  //请求体无效
            response.set_content(errorjsonstr,"application/json");
            return;
        }

        std::string sessionid=requestjson["session_id"].asString();
        std::string message=requestjson["message"].asString();
        INFO("Received message from client (stream): session_id={}, message={}", sessionid, message);
        if(sessionid.empty()||message.empty()){
            std::string errorjsonstr=buildResponse("session_id or message is empty",false);
            response.status=400;  //请求体无效
            response.set_content(errorjsonstr,"application/json");
            return;
        }
        
        //准备响应
        response.status=200;  //请求成功;
        response.set_header("Cache-Control","no-cache");    //不使用缓存，服务器立即将数据发送给网络
        response.set_header("Connection","keep-alive");     //保持连接，服务器不会关闭连接
        response.set_header("Access-Control-Allow-Origin","*");    //允许跨域请求
        response.set_header("Access-Control-Allow-Headers","*");   //允许所有请求头

        response.set_chunked_content_provider("text/event-stream",[this,sessionid,message](size_t offset,httplib::DataSink& dataSink)->bool{
            auto writeChunk= [&](const std::string& chunk,bool last){
                //把chunk转为SEEK格式
                // Json::valueToQuotedString: 对chunk进行Json转换，目的防止chunk中包含一些特殊字符来破坏数据格式，比如：在chunk中包含了两个连续的换行，就会影响SSE数据格式
                std::string seeData="data: "+ Json::valueToQuotedString(chunk.c_str())+"\n\n";
               
                //把模型返回的chunk发送给客户端
                dataSink.write(seeData.c_str(),seeData.size());   //将数据写入相应流，即发送给客户端，该方法不会等缓冲区满才发送

                //处理结束标记
                if(last)
                {
                    //流式相应结束
                    std::string doneData="data: [DONE]\n\n";
                    dataSink.write(doneData.c_str(),doneData.size());
                    dataSink.done();
                    return false;    //不再有后续数据
                }
                return true;
            };

            //先给客户端发送一个空的SEEK格式，确保客户端会话开始
            if(!writeChunk("",false)){
                return false;
            }

            //发送消息流
            _chat_sdk->sendMessageStream(sessionid,message,writeChunk);

            return false;
            
        });
    }

        // 设置HTTP路由规则
        void ChatServer::setHttpRoutes(){
        // 处理创建会话请求
        _chatserver->Post("/api/session", [this](const httplib::Request& request, httplib::Response& response){
            handleCreateSessionRequest(request, response);
        });
        // 处理获取会话列表请求
        _chatserver->Get("/api/sessions", [this](const httplib::Request& request, httplib::Response& response){
            handleGetSessionsListRequest(request, response);
        }); 
        // 处理获取可用模型列表请求
        _chatserver->Get("/api/models", [this](const httplib::Request& request, httplib::Response& response){
            handleGetAvailableModelsListRequest(request, response);
        });
        // 处理删除会话请求
        _chatserver->Delete("/api/session/(.*)", [this](const httplib::Request& request, httplib::Response& response){
            handleDeleteSessionRequest(request, response);
        });
        // 处理获取历史消息请求
        _chatserver->Get("/api/session/(.*)/history", [this](const httplib::Request& request, httplib::Response& response){ 
            handleGetHistoryMessagesRequest(request, response);
        });
        // 处理发送消息请求-全量返回
        _chatserver->Post("/api/message", [this](const httplib::Request& request, httplib::Response& response){
            handleSendMessageRequest(request, response);
        });
        // 处理发送消息请求-增量返回
        _chatserver->Post("/api/message/async", [this](const httplib::Request& request, httplib::Response& response){
            handleSendMessageStreamRequest(request, response);
        });
    }
    
}
