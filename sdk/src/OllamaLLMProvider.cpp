#include "../include/OllamaLLMProvider.h"
#include "../include/commen.h"
#include "../include/util/mylog.h"
#include <iostream>
#include <jsoncpp/json/json.h>
#include <httplib.h>
namespace ai_chat_sdk{
    //初始化模型
    bool OllamaLLMProvider::InitModel(const std::map<std::string,std::string>& modelConfig)
    {
        //初始化模型名字
        auto it=modelConfig.find("model_name");
        if(it==modelConfig.end())
        {
            ERROR("OllamaLLMProvider model_name not found in config");
            return false;
        }
        model_name=it->second;
        //初始化模型描述
        it=modelConfig.find("model_desc");
        if(it==modelConfig.end())
        {
            ERROR("model_desc not found in config");
            return false;
        }
        model_desc=it->second;
        it=modelConfig.find("endpoint");
        if(it==modelConfig.end())
        {
            ERROR("endpoint not found in config");
            return false;
        }
        _endpoint=it->second;
        _isAvailable=true;
        INFO("OllamaLLMProvider InitModel success");
        return true;
    }

    //检查模型是否可用
    bool OllamaLLMProvider::IsModelAvailable() const
    {
        return _isAvailable;
    }
    //获取模型名字
    std::string OllamaLLMProvider::GetModelName() const
    {
        return model_name;
    }
    //获取模型描述
    std::string OllamaLLMProvider::GetModelDesc() const
    {
        return model_desc;
    }
    //发送消息给模型
    std::string OllamaLLMProvider::SendMessage(const std::vector<ChatMessage>& messages,const std::map<std::string,std::string>& requestparam)
    { 
        //发送消息，全量返回
        if(!IsModelAvailable())
        {
            ERROR("OllamaLLMProvider is not available");
            return "";
        }

        int temperature=0.7;
        int max_tokens=2048;
        if(requestparam.find("temperature")!=requestparam.end())
        {
            temperature=std::stod(requestparam.find("temperature")->second);
        }
        if(requestparam.find("max_tokens")!=requestparam.end())
        {
            max_tokens=std::stoi(requestparam.find("max_tokens")->second);
        }

        //构建历史消息
        Json::Value history(Json::arrayValue);
        for(auto &msg:messages)
        {
            Json::Value item(Json::objectValue);
            item["role"]=msg.role;
            item["content"]=msg.content;
            history.append(item);
        }
        //构建请求体
        Json::Value request_body(Json::objectValue);
        request_body["model"]=model_name;
        request_body["messages"]=history;
        request_body["temperature"]=temperature;
        request_body["max_tokens"]=max_tokens;
        request_body["stream"]=false;
        //序列化
        Json::StreamWriterBuilder writer;
        std::string json_string=Json::writeString(writer,request_body);
        DEBUG("request_body:{}",json_string);

        //构建请求头
        httplib::Headers headers={
            {"Content-Type","application/json"}
        };

        //创建Httplib Client
        httplib::Client client("127.0.0.1",11434);
        client.set_connection_timeout(30,0);  //30秒连接超时时间
        client.set_read_timeout(300,0);  //30秒读取超时时间
        
        //发送请求
        auto response=client.Post("/api/chat",headers,json_string,"application/json");
        if(!response)
        {   
            ERROR("Connect to %s failed ",_endpoint.c_str());
            return "";
        }
        //DEBUG("response status:{}",response->status);
        //DEBUG("response:{}",response->body);
        //检测相应是否成功
        if(response->status!=200)
        {
            ERROR("OllamaLLMProvider SendMessage failed not-200 status, status:%d, body:%s",response->status,response->body.c_str());
            return "";
        }
        //解析响应体
        Json::Value response_json;
        Json::CharReaderBuilder reader_builder;
        std::string parse_errors;
        std::istringstream response_stream(response->body);
        if(!Json::parseFromStream(reader_builder,response_stream,&response_json,&parse_errors))
        {
            ERROR("OllamaLLMProvider SendMessage failed parse response body failed, errors:%s",parse_errors.c_str());
            return "";
        }
        //解析大模型的回复
        //大模型回复包含在messages，json中
        if(response_json.isMember("message")&&
           response_json["message"].isMember("content"))
        {
            std::string reply_content=response_json["message"]["content"].asString();
            //INFO("reply_content:{}",reply_content);
            return reply_content;
        }
        //解析失败
        ERROR("Invalid response format from Ollama API");
        return "Invalid response format from Ollama API";
    }

    std::string OllamaLLMProvider::SendMessageStream(const std::vector<ChatMessage>& messages,
        const std::map<std::string,std::string>& requestparam,
        std::function<void(const std::string&,bool)> callback)
    {
        //检测模型是否可用
        if(!IsModelAvailable())
        {
            ERROR("OllamaLLMProvider is not available");
            return "";
        }
        //获取采样温度，max_tokens
        int temperature=0.7;
        int max_tokens=2048;
        if(requestparam.find("temperature")!=requestparam.end())
        {
            temperature=std::stod(requestparam.find("temperature")->second);
        }
        if(requestparam.find("max_tokens")!=requestparam.end())
        {
            max_tokens=std::stoi(requestparam.find("max_tokens")->second);
        }

        //构建历史消息
        Json::Value history(Json::arrayValue);
        for(auto &msg:messages)
        {
            Json::Value item(Json::objectValue);
            item["role"]=msg.role;
            item["content"]=msg.content;
            history.append(item);
        }

        //构建请求体
        Json::Value options;
        options["temperature"]=temperature;
        options["max_tokens"]=max_tokens;
        Json::Value request_body(Json::objectValue);
        request_body["model"]=model_name;
        request_body["messages"]=history;
        request_body["options"]=options;
        request_body["stream"]=true;

        //序列化
        Json::StreamWriterBuilder writer;
        std::string json_string=Json::writeString(writer,request_body);
        DEBUG("request_body:{}",json_string);
        //创建HTTP请求头
        httplib::Headers headers={
            {"Content-Type","application/json"}
        };
        //创建Httplib Client
        httplib::Client client("127.0.0.1",11434);
        client.set_connection_timeout(30,0);  //30秒连接超时时间
        client.set_read_timeout(300,0);  //30秒读取超时时间
        
        //流式变量
        std::string reply_content;
        bool geterror=false;
        std::string errormsg;  //错误描述
        int statuscode=0;  //状态码
        bool streamfinished=false;  //流式完成标志
        std::string fullresponse;  //全部回复
        //创建请求对象
        httplib::Request req;
        req.path="/api/chat";
        req.method="POST";
        req.headers=headers;
        req.body=json_string;
        
        //相应头处理
        req.response_handler=[&](const httplib::Response& resp){
            statuscode=resp.status;
            if(statuscode!=200)
            {
                geterror=true;
                errormsg="http error"+std::to_string(statuscode);
                return false;
            }
            return true;
        };
        //流式处理增量
        req.content_receiver=[&](const char* data,size_t len,uint64_t offset,uint64_t totallength){
            //如果http错误，直接返回
            if(geterror)
            {
                return false;
            }
            //追加数据到缓冲区
            reply_content+=std::string(data,len);
            DEBUG("reply_content:{}",reply_content);
            size_t pos=0;
            while((pos=reply_content.find("\n"))!=std::string::npos)
            {
                std::string jsonline=reply_content.substr(0,pos);
                reply_content.erase(0,pos+1);  
            
                //处理注释和空行
                if(jsonline.empty())
                {
                    continue;
                }
                
                //解析json数据
                Json::Value chunk;
                Json::CharReaderBuilder reader;
                std::string errs;
                std::istringstream jsonstream(jsonline);

            if(Json::parseFromStream(reader,jsonstream,&chunk,&errs))
            {
                //处理结束标记
                if(chunk.get("done",false).asBool())
                {
                    callback("",true);
                    streamfinished=true;
                    return true;
                }
                if(chunk.isMember("message")&&
            chunk["message"].isMember("content"))
                {
                    std::string content=chunk["message"]["content"].asString();

                    //累计增量回复
                    fullresponse+=content;
                    callback(content,false);
                }else{
                    WARN("OllamaProvider SendMessageStream chunk error:{}",errs);
                }
                
                }
              }
            return true;//继续处理
            };

            //发送请求
            auto response=client.send(req);
            if(!response){
                //请求，DNS问题,网络错误
                DEBUG("OllamaProvider SendMessageStream request failed: {}",static_cast<int>(response.error()));    
                return "";
            }
            //确保流式相应正确结束
            if(!streamfinished)
                {
                    WARN("OllamaProvider without done-true");
                    callback("",true);
                }
               
                return fullresponse;
      

    }
}
