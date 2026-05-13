#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <ctime>
namespace ai_chat_sdk{

    //消息主体
    struct ChatMessage{
        std::string id;          //消息唯一标识符
        std::string role;        //assistant or user
        std::string content;     //消息主体
        std::time_t timestamp;   //消息生成时间

        ChatMessage() = default;
        ChatMessage(const std::string& r,const std::string& c)
        :role(r),content(c),timestamp(std::time(nullptr))
        {}
    };

    //会话结构
    struct Session{
        std::string id;            //会话唯一标识符
        std::vector<ChatMessage> messages;   //消息列表
        std::string model_name;         //会话模型名称
        std::time_t created_time;    //创建时间 
        std::time_t updated_time;    //更新时间

        Session(const std::string& model)
        :model_name(model),created_time(std::time(nullptr)),updated_time(std::time(nullptr))
        {}
    };

    //调用模型时配置信息
    struct ChatModelConfig{
        virtual ~ChatModelConfig()=default; //启动运行类型识别，默认析构函数
        std::string model_name;         //会话模型名称
        std::string temperature="0.7";    //温度参数
        std::string max_tokens="2048";    //最大token数
        std::string top_p="0.7";              //top_p参数        
    };

    //API配置结构
    //ollama本地接入大模型不需要api key,因此将api key提取出来
    struct ApiConfig :public ChatModelConfig
    {
        std::string api_key; //接入模型时认证信息 
    };

    // 通过Ollama接入本地模型---不需要apikey
    struct OllamaConfig : public ChatModelConfig{
        std::string model_name;       // 模型名称
        std::string model_desc;       // 模型描述
        std::string endpoint;        // 模型API endpoint  base url
    };

    //LLM模型信息
    struct ModelInfo    
    {
        std::string _name;    //模型名称
        std::string _desc;    //模型描述
        std::string _provider;//模型提供者
        std::string _endpoint;//模型base url
        bool _isAvailable; //模型是否被初始化
        
        ModelInfo() = default; 
        ModelInfo(const std::string& name,const std::string& desc="",const std::string& provider="",const std::string& endpoint="")
        :_name(name),
        _desc(desc),
        _provider(provider),
        _endpoint(endpoint),
        _isAvailable(false)
        {}
    };

}
