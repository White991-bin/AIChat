#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include "LLMProvider.h"
#include "commen.h"
namespace ai_chat_sdk
{
    class LLMManager
    {
        public:
        //注册大模型提供器
        bool registerProvider(const std::string& modelname,std::unique_ptr<LLMProvider> provider);
        //初始化大模型提供器
        bool initModel(const std::string& modelname,const std::map<std::string,std::string>& modelparams);
        //获取可用模型列表
        std::vector<ModelInfo> getAvailableModels() const;
        //检查模型可用
        bool isModelAvailable(const std::string& modelname) const;
        //发送消息全量返回回复
        std::string sendMessage(const std::string& modelname,const std::vector<ChatMessage>& messages,
        const std::map<std::string,std::string>& requestparams);

        //发送消息到指定模型，流式返回回复
        std::string sendMessageStream(const std::string& modelname,const std::vector<ChatMessage>& messages,
        const std::map<std::string,std::string>& requestparams,
        std::function<void(const std::string&,bool)> callback);

        private:
        std::unordered_map<std::string,std::unique_ptr<LLMProvider>> _providers;
        std::unordered_map<std::string,ModelInfo> _models;
    };
}
