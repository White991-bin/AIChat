#pragma once
#include "util/mylog.h"
#include <string>
#include <vector>
#include <iostream>
#include <map>
#include <functional>
#include "commen.h"
namespace ai_chat_sdk{
    class LLMProvider{
    public:
        //初始化模型
        virtual bool InitModel(const std::map<std::string,std::string>& modelConfig)=0;
        //检查模型是否被初始化
        virtual bool IsModelAvailable() const=0;
        //获取模型名称       
        virtual std::string GetModelName() const=0;
        //获取模型描述
        virtual std::string GetModelDesc() const=0;
        //发送消息 全量返回
        virtual std::string SendMessage(const std::vector<ChatMessage>& messages,const std::map<std::string,std::string>& requestparam)=0;
        //发送消息 增量返回 流式相应
        virtual std::string SendMessageStream(const std::vector<ChatMessage>& messages,
            const std::map<std::string,std::string>& requestparam,
            std::function<void(const std::string&,bool)> callback)=0; 
            //callback对数据返回如何处理，第一个参数为数据，第二个参数为是否完成标志
    protected:
        bool _isAvailable=false;   //标记模型是否有效
        std::string _apikey;       //Api密钥
        std::string _endpoint;     //Api地址
    
    };
} //end ai_chat_sdk