#include "../include/LLMManager.h"
#include "../include/util/mylog.h"

namespace ai_chat_sdk{
    bool LLMManager::registerProvider(const std::string& modelname,std::unique_ptr<LLMProvider> provider){

        if(!provider){
            ERROR("registerProvider: provider is null");
            return false;
        }
        //unique_ptr 防拷贝，只能移动赋值
        //之后provider所有权失效
        _providers[modelname]=std::move(provider);
        _models[modelname]=ModelInfo(modelname);
        //模型注册成功
        INFO("registerProvider: modelname {}",modelname);
        return true;
    }
    //初始化模型
    bool LLMManager::initModel(const std::string& modelname,const std::map<std::string,std::string>& modelparams){
        //检测模型是否已注册
        if(_providers.find(modelname)==_providers.end()){
            ERROR("initModel: modelname {} not registered",modelname);
            return false;
        }
        //模型注册过了，进行初始化
        bool ret=_providers[modelname]->InitModel(modelparams);
        if(ret){
            INFO("initModel: init model {} success",modelname);
            //初始化成功，标记模型可用
            _models[modelname]._desc=_providers[modelname]->GetModelDesc();
            _models[modelname]._isAvailable=true;
        }else{
            ERROR("initModel: init model {} failed",modelname);
            _models[modelname]._isAvailable=false;
        }
        return ret;
        
    }
    //获取可用模型列表
    std::vector<ModelInfo> LLMManager::getAvailableModels() const{
        std::vector<ModelInfo> ret;
        for(auto it=_models.begin();it!=_models.end();it++){
            if(it->second._isAvailable){
                ret.push_back(it->second);
            }
        }
        return ret;
    }
    //检查模型可用
    bool LLMManager::isModelAvailable(const std::string& modelname) const{
        auto it=_models.find(modelname);
        if(it==_models.end()){
            ERROR("isModelAvailable: modelname {} not initialized",modelname);
            return false;
        }
        return it->second._isAvailable;
    }
    
    std::string LLMManager::sendMessage(const std::string& modelname,const std::vector<ChatMessage>& messages,
    const std::map<std::string,std::string>& requestparams){
        //检测模型是否注册
        auto it=_providers.find(modelname);
        if(it==_providers.end()){
            ERROR("sendMessage: modelname {} not registered",modelname);
            return "";
        }
        //检测模型是否可用
        if(!isModelAvailable(modelname)){
            ERROR("sendMessage: modelname {} is not available",modelname);
            return "";
        }
        //已注册并可用
        return _providers[modelname]->SendMessage(messages,requestparams);
    }
    std::string LLMManager::sendMessageStream(const std::string& modelname,const std::vector<ChatMessage>& messages,
    const std::map<std::string,std::string>& requestparams,
    std::function<void(const std::string&,bool)> callback){
        //检测模型是否注册
        auto it=_providers.find(modelname);
        if(it==_providers.end()){
            ERROR("sendMessageStream: modelname {} not registered",modelname);
            return "";
        }
        //检测模型是否可用
        if(!isModelAvailable(modelname)){
            ERROR("sendMessageStream: modelname {} is not available",modelname);
            return "";
        }
        //已注册并可用
        return _providers[modelname]->SendMessageStream(messages,requestparams,callback);
    }
}
