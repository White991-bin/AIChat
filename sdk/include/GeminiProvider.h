#pragma once
#include "LLMProvider.h"
#include "commen.h"

namespace ai_chat_sdk{
    class GeminiProvider:public LLMProvider{
    public:
        //初始化模型
        virtual bool InitModel(const std::map<std::string,std::string>& modelConfig) override;
        //检查模型是否被初始化
        virtual bool IsModelAvailable() const override;
        //获取模型名称       
        virtual std::string GetModelName() const override;
        //获取模型描述
        virtual std::string GetModelDesc() const override;
        //发送消息 全量返回
        virtual std::string SendMessage(const std::vector<ChatMessage>& messages,const std::map<std::string,std::string>& requestparam) override;
        //发送消息 增量返回 流式相应
        virtual std::string SendMessageStream(const std::vector<ChatMessage>& messages,
            const std::map<std::string,std::string>& requestparam,
            std::function<void(const std::string&,bool)> callback) override;
    };
} //end ai_chat_sdk 

