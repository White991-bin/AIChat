#include "../include/ChatSdk.h"
#include "../include/util/mylog.h"
#include "../include/OllamaLLMProvider.h"
#include "../include/GeminiProvider.h"
#include "../include/DeepSeekProvider.h"
#include "../include/commen.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include "../include/SessionManager.h"
#include "../include/LLMManager.h"

namespace ai_chat_sdk{
  
    std::string ChatSdk::testFunction() {
    return "test function works!";
}
    //初始化模型
    bool ChatSdk::initModels(const std::vector<std::shared_ptr<ChatModelConfig>>& configs)
    {
        //注册所有提供程序
        registerAllProvider(configs);
        initAllProviders(configs);
        _initialized = true;
        return true;
    }
    void ChatSdk::registerAllProvider(const std::vector<std::shared_ptr<ChatModelConfig>>& configs){
        //deepseek-chat
        if(!_llmmanager.isModelAvailable("deepseek-chat")){
            auto deepseekprovider=std::make_unique<DeepSeekProvider>();
            _llmmanager.registerProvider("deepseek-chat",std::move(deepseekprovider));
            INFO("ChatSdk registerAllProvider: register deepseek-chat provider success");
        }
        //gemini-2.5-flash
        if(!_llmmanager.isModelAvailable("gemini-2.5-flash")){
            auto geminiprovider=std::make_unique<GeminiProvider>();
            _llmmanager.registerProvider("gemini-2.5-flash",std::move(geminiprovider));
            INFO("ChatSdk registerAllProvider: register gemini-2.5-flash provider success");
        }

        //ollama接入本地模型
        std::unordered_set<std::string> modelnames;
        for(auto& config:configs){
                auto ollamaConfig = std::dynamic_pointer_cast<OllamaConfig>(config);
                if(ollamaConfig){
                    auto modelname = ollamaConfig->model_name;
                    if(modelnames.find(modelname) == modelnames.end()){
                    // 该模型还没有注册
                    modelnames.insert(modelname);

                if(!_llmmanager.isModelAvailable(modelname)){
                    // 该模型还没有注册
                    _llmmanager.registerProvider(modelname, std::make_unique<OllamaLLMProvider>());
                    INFO("ChatSdk registerAllProvider: register OllamaLLMProvider {} registered successed", modelname);
                }
                }
            }
        }   
    }
    void ChatSdk::initAllProviders(const std::vector<std::shared_ptr<ChatModelConfig>>& configs){
        for(const auto& config:configs){
            if(auto apiconfig = std::dynamic_pointer_cast<ApiConfig>(config)){
                if(apiconfig->model_name=="deepseek-chat"||
                apiconfig->model_name=="gemini-2.5-flash"){
                    //支持的云端模型
                    initProviderByApi(apiconfig->model_name,apiconfig);
                }
                else{
                    ERROR("ChatSdk initAllProviders: model {} not supported",apiconfig->model_name);
                }
            }else if(auto ollamaConfig = std::dynamic_pointer_cast<OllamaConfig>(config)){
                //初始化ollama模型提供者
                initProviderByOllama(ollamaConfig->model_name,ollamaConfig);
            }
            else{
                ERROR("ChatSdk initAllProviders: model {} not supported",config->model_name);  
            }
        }
    }
    bool ChatSdk::initProviderByApi(const std::string& modelname,const std::shared_ptr<ApiConfig>& apiconfig){
        if(modelname.empty()){
            ERROR("ChatSdk initProviderByApi: modelname is empty");
            return false;
        }
        if(!apiconfig||apiconfig->api_key.empty()){
            ERROR("ChatSdk initProviderByApi: apiconfig is nullptr or api_key is empty");
            return false;
        }
        //初始化提供者
        if(_llmmanager.isModelAvailable(modelname)){
            INFO("ChatSdk initProviderByApi: model {} already available, register it",modelname);
            return true;
        }

        std::map<std::string,std::string> params;
        params["api_key"]=apiconfig->api_key;
        if(!_llmmanager.initModel(modelname,params)){
            ERROR("ChatSdk initProviderByApi: init model {} failed",modelname);
            return false;
        }
        
        //模型配置
        _modelconfigs[modelname]=apiconfig;
        INFO("ChatSdk initProviderByApi: model {} init success",modelname);
        return true;
    }

   //初始化模型提供者，通过ollama调用
   bool ChatSdk::initProviderByOllama(const std::string& modelname,const std::shared_ptr<OllamaConfig>& ollamaconfig)
   {
        //参数检测
        if(modelname.empty()){
            ERROR("ChatSdk initProviderByOllama: modelname is empty");
            return false;
        }
        if(!ollamaconfig||ollamaconfig->endpoint.empty()){
            ERROR("ChatSdk initProviderByOllama: ollamaconfig is nullptr or endpoint is empty");
            return false;
        }
        //初始化提供者
        if(_llmmanager.isModelAvailable(modelname)){
            INFO("ChatSdk initProviderByOllama: model {} already available, register it",modelname);
            return true;
        }

        //初始化模型提供者
        std::map<std::string,std::string> params;
        params["model_name"] = modelname;
        params["model_desc"] = ollamaconfig->model_desc;
        params["endpoint"]=ollamaconfig->endpoint;
        if(!_llmmanager.initModel(modelname,params)){
            ERROR("ChatSdk initProviderByOllama: init model {} failed",modelname);
            return false;
        }
        //模型配置
        _modelconfigs[modelname]=ollamaconfig;
        INFO("ChatSdk initProviderByOllama: model {} init success",modelname);
        return true;
   }

    //创建对话云端模型
    std::string ChatSdk::createSession(const std::string& modelname){
        //检测SDK是否初始化成功
        if(!_initialized){
            ERROR("ChatSdk createSession: models not initialized");
            return "";
        }
        //通过SessionManager创建会话
        std::string sessionid=_sessionmanager.createSession(modelname);
        if(sessionid.empty()){
            ERROR("ChatSdk createSession: create session {} failed",modelname);
            return "";
        }
        INFO("ChatSdk createSession: sessionid {}",sessionid);
        return sessionid;
    }
   //获取指定会话
   std::shared_ptr<Session> ChatSdk::getSession(const std::string& sessionid){
    //检测SDK是否初始化成功
        if(!_initialized){
            ERROR("ChatSdk getSession: models not initialized");
            return nullptr;
        }
        INFO("ChatSdk getSession:ENTERED getSession session {}",sessionid);
        auto session=_sessionmanager.getSession(sessionid);
        return session;
   }
   //获取所有会话列表
   std::vector<std::string> ChatSdk::getSessionsList(){
        if(!_initialized){
            ERROR("ChatSdk getSessionsList: models not initialized"); 
            return {};
        }
        return _sessionmanager.getSessionsList();
   }
   //删除对话
   bool ChatSdk::deleteSession(const std::string& sessionid){
        INFO("ChatSdk deleteSession:ENTERED delete session {}",sessionid);
        if(!_initialized){
            ERROR("ChatSdk deleteSession: models not initialized");
            return false;
        }
        INFO("ChatSdk deleteSession: delete session {}",sessionid);
        bool result= _sessionmanager.deleteSession(sessionid);
        if(!result){
            ERROR("ChatSdk deleteSession: delete session {} failed",sessionid);
            return false;
        }
        INFO("ChatSdk deleteSession: delete session {} success",sessionid);
        return true;
   }
   //获取可用模型
   std::vector<ModelInfo> ChatSdk::getAvailableModels(){
        
        return _llmmanager.getAvailableModels();
   }

   //给模型发消息-全量返回
   std::string ChatSdk::sendMessage(const std::string& sessionid,const std::string& message)
   {
        INFO("ChatSdk sendMessage:ENTERED sendMessage, sessionid {}, message: {}",sessionid,message);
        
        if(!_initialized){
            ERROR("ChatSdk sendMessage: models not initialized");
            return "";
        }
        auto session=_sessionmanager.getSession(sessionid);
        if(!session){
            ERROR("ChatSdk sendMessage: session {} not found",sessionid);
            return "";
        }
        //构建历史消息
        ChatMessage usermessage("user",message);
        _sessionmanager.addMessage(sessionid,usermessage);
        auto historymessages=_sessionmanager.getMessagesHistory(sessionid);

        //构建请求参数
        auto it=_modelconfigs.find(session->model_name);
        if(it==_modelconfigs.end()){
            ERROR("ChatSdk sendMessage: model {} not found",session->model_name);
            return "";
        }
        std::map<std::string,std::string> requestparams;
        requestparams["temperature"]=it->second->temperature;
        requestparams["max_tokens"]=it->second->max_tokens;

        //调用LLMManager发送消息全量返回回复
        std::string response=_llmmanager.sendMessage(session->model_name,historymessages,requestparams);
        if(response.empty()){
            ERROR("ChatSdk sendMessage: sendMessage failed");
            return "";
        }
        INFO("ChatSdk sendMessage: sendMessage success, response: {}",response);
        
        //添加助手消息更新时间
        ChatMessage assistantmessage("assistant",response);
        _sessionmanager.addMessage(sessionid,assistantmessage);
        INFO("ChatSdk sendMessage: sendMessage success, assistant message: {}",assistantmessage.content);
        
        _sessionmanager.updateSessionTimestamp(sessionid,session->updated_time);
        INFO("ChatSdk sendMessage: sendMessage success, session {} updated",sessionid);
        
        return response;
   }
   //给模型发消息-流式返回
   std::string ChatSdk::sendMessageStream(const std::string& sessionid,const std::string& message,
   std::function<void(const std::string&,bool)> callback){
        if(!_initialized){
            ERROR("ChatSdk sendMessageStream: models not initialized");
            return "";
        }
        auto session=_sessionmanager.getSession(sessionid);
        if(!session){
            ERROR("ChatSdk sendMessageStream: session {} not found",sessionid);
            return "";
        }
        //构建历史消息
        ChatMessage usermessage("user",message);
        _sessionmanager.addMessage(sessionid,usermessage);
        auto historymessages=_sessionmanager.getMessagesHistory(sessionid);

        //构建请求参数
        auto it=_modelconfigs.find(session->model_name);
        if(it==_modelconfigs.end()){
            ERROR("ChatSdk sendMessageStream: model {} not found",session->model_name);
            return "";
        }
        std::map<std::string,std::string> requestparams;
        requestparams["temperature"]=it->second->temperature;
        requestparams["max_tokens"]=it->second->max_tokens;

        //调用LLMManager发送消息流式返回回复
        auto response=_llmmanager.sendMessageStream(session->model_name,historymessages,requestparams,callback);
        if(response.empty()){
            ERROR("ChatSdk sendMessageStream: sendMessageStream to {} failed",session->model_name);
            return "";
        }

        //添加助手消息更新时间
        ChatMessage assistantmessage("assistant",response);
        _sessionmanager.addMessage(sessionid,assistantmessage);
        _sessionmanager.updateSessionTimestamp(sessionid,session->updated_time);
        INFO("ChatSdk sendMessageStream: sendMessageStream to {} success",session->model_name);
        return response;
    }
}//end ai_chat_sdk

