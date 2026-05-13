#include "../include/GeminiProvider.h"
#include "../include/commen.h"
#include <sstream>
#include <jsoncpp/json/json.h>
#include <httplib.h>

namespace ai_chat_sdk{
    bool GeminiProvider::InitModel(const std::map<std::string,std::string>& modelConfig){
        //初始化API key和endpoint
        auto it = modelConfig.find("api_key");
            if(it == modelConfig.end())
            {
                ERROR("GeminiProvider::initModel: api_key not found in modelConfig");
                return false;
            }
            _apikey = it->second;

            it = modelConfig.find("endpoint");
            if(it == modelConfig.end()){
                _endpoint = "https://api.viviai.cc";
            }else{
                _endpoint = it->second;
            }
        _isAvailable=true;
        INFO("GeminiProvider InitModel success");
        return true;
    }
    //检测模型是否可用
    bool GeminiProvider::IsModelAvailable() const{
        return _isAvailable;
    }

    //获取模型名称
    std::string GeminiProvider::GetModelName() const{
        return "gemini-2.5-flash";
    }
    //获取模型描述
    std::string GeminiProvider::GetModelDesc() const{
        return "Gemini 2.5 Flash 是 Google 推出的新一代极速响应大语言模型。";
    }

    //发送消息给模型，作为http请求
    std::string GeminiProvider::SendMessage(const std::vector<ChatMessage>& messages,const std::map<std::string,std::string>& requestparam){
        //发送消息 全量返回
        if(!IsModelAvailable()){
            ERROR("GeminiProvider SendMessage model not available");
            return "";
        }
        double temperature=0.7;
        int max_tokens=2048;
        if(requestparam.find("temperature")!=requestparam.end()){
            temperature=std::stod(requestparam.find("temperature")->second);
        }
        if(requestparam.find("max_tokens")!=requestparam.end()){
            max_tokens=std::stoi(requestparam.find("max_tokens")->second);
        }

        //构建历史消息
        Json::Value history(Json::arrayValue);
        for(const auto& msg:messages){
            Json::Value item;
            item["role"]=msg.role;
            item["content"]=msg.content;
            history.append(item);
        }

        //构建请求体
        Json::Value request_body(Json::objectValue);
        request_body["model"]=GetModelName();
        request_body["messages"]=history;
        request_body["temperature"]=temperature;
        request_body["max_tokens"]=max_tokens;
        request_body["stream"]=false;

        //序列化
        Json::StreamWriterBuilder writer;
        std::string json_string=Json::writeString(writer,request_body);  
        DEBUG("GeminiProvider SendMessage request body: %s",json_string.c_str());
        
        //构建请求头
        httplib::Headers headers={
            {"Authorization","Bearer "+_apikey},
            {"Content-Type","application/json"}
        };
        //发送请求
        httplib::Client client(_endpoint.c_str());
        auto response=client.Post("/v1/chat/completions",headers,json_string,"application/json");
        if(!response){
            ERROR("GeminiProvider SendMessage error: {}",response->body);
            return "";
        }
        //DEBUG("GeminiProvider response status: {}",response->status);
        //DEBUG("GeminiProvider response body: {}",response->body);

        //检查相应是否成功
        if(response->status!=200){
            ERROR("GeminiProvider SendMessage error: {}",response->body);
            return "";
        }

        //解析相应体
        Json::Value response_json;
        Json::CharReaderBuilder reader_builder;
        std::string parse_errors;
        std::istringstream response_stream(response->body);
        if(!Json::parseFromStream(reader_builder,response_stream,&response_json,&parse_errors)){
            ERROR("GeminiProvider SendMessage error: {}",parse_errors);
            return "";
        }
        //解析大模型回复内容
        //大模型回复存在choicejson数组中
        if(response_json.isMember("choices")&&response_json["choices"].isArray()&&!response_json.empty())
        {
            auto& choice=response_json["choices"][0];   
            if(choice.isMember("message")&&choice["message"].isObject()){
                std::string content=choice["message"]["content"].asString();
                //INFO("Received content: {}",content);
                return content;
            }
           
            
        }
         //解析失败，返回错误信息
         ERROR("Invalid response body from Gemini API");
         return "Invalid response body from Gemini API";

    }
    std::string GeminiProvider::SendMessageStream(const std::vector<ChatMessage>& messages,
        const std::map<std::string,std::string>& requestparam,
        std::function<void(const std::string&,bool)> callback){
        //发送消息 流式返回
        if(!IsModelAvailable()){
            ERROR("GeminiProvider SendMessageStream model not available");
            return "";
        }
        //获取温度和最大token数参数
        double temperature=0.7;
        int max_tokens=2048;
        if(requestparam.find("temperature")!=requestparam.end()){
            temperature=std::stod(requestparam.find("temperature")->second);
        }
        if(requestparam.find("max_tokens")!=requestparam.end()){
            max_tokens=std::stoi(requestparam.find("max_tokens")->second);
        }

        //构建历史消息
        Json::Value history(Json::arrayValue);
        for(const auto& msg:messages){
            Json::Value item;
            item["role"]=msg.role;
            item["content"]=msg.content;
            history.append(item);
        }
        //构建请求体
        Json::Value request_body(Json::objectValue);
        request_body["model"]=GetModelName();
        request_body["messages"]=history;
        request_body["temperature"]=temperature;
        request_body["max_tokens"]=max_tokens;
        request_body["stream"]=true;
        //序列化
        Json::StreamWriterBuilder writer;
        std::string json_string=Json::writeString(writer,request_body);  
        DEBUG("GeminiProvider SendMessageStream request body: %s",json_string.c_str());
        //创建http客户端
        httplib::Client client(_endpoint.c_str());
        client.set_connection_timeout(60,0);//60秒超时
        client.set_read_timeout(300,0);     //300秒超时
        //client.set_proxy("127.0.0.1",7890);

        //设置请求头
        httplib::Headers headers={
            {"Authorization","Bearer "+_apikey},
            {"Content-Type", "application/json"}
        };
        //处理流式变量
        std::string buffer;
        bool geterror=false;
        std::string errormsg;
        int statuscode=0;
        bool streamfinished=false;

        //处理累计响应
        std::string fullresponse;

        //创建请求体
        httplib::Request request;
        request.method="POST";
        request.path="/v1/chat/completions";
        request.headers=headers;
        request.body=json_string;
        
        
        //处理响应头
        request.response_handler=[&](const httplib::Response& response){
            DEBUG("GeminiProvider response body: %s", response.body.c_str());
            statuscode=response.status;
            DEBUG("GeminiProvider response headers: {}",response.status);
            if(statuscode!=200){
                geterror=true;
                errormsg="http error:"+std::to_string(statuscode);
                return false;  //终止处理
            }
            return true;  //继续处理响应
        };
        //处理流式相应
        request.content_receiver=[&](const char* data,size_t len,uint64_t offset,uint64_t total_length){
            // 在 request.content_receiver 开头添加：
            DEBUG("GeminiProvider raw chunk: %s", std::string(data, len).c_str());


            //如果HTTP请求失败，直接返回false
            if(geterror){
                return false;
            }   
           buffer+=std::string(data,len);
           DEBUG("GeminiProvider response chunk: {}",buffer);

            //处理完整数据
            size_t pos=0;
            while((pos=buffer.find("\n\n"))!=std::string::npos){
                std::string event=buffer.substr(0,pos);
                buffer.erase(0,pos+2);
                //callback(event,true);   
                
                if(event.empty()||event[0]==':'){
                    continue;
                }

                DEBUG("GeminiProvider response event: {}",event);
                
                if(0==event.compare(0,6,"data: ")){
                    std::string jsonstr=event.substr(6);
                    
                    if(jsonstr=="[DONE]"){
                        callback("",true);
                        streamfinished=true;
                        return true;
                    }

                    //解析Json字符串
                    Json::Value chunk;
                    Json::CharReaderBuilder reader;
                    std::string errs;
                    std::istringstream jsonStream(jsonstr);

                    if(Json::parseFromStream(reader,jsonStream,&chunk,&errs)){
                      
                        if(chunk.isMember("choices")&&
                        chunk["choices"].isArray()&&
                        !chunk["choices"].empty()&&
                        chunk["choices"][0].isMember("delta")&&
                        chunk["choices"][0]["delta"].isMember("content")){
                            std::string content=chunk["choices"][0]["delta"]["content"].asString();
                            fullresponse+=content;
                            DEBUG("fullresponse length: %zu, added: '%s'", fullresponse.length(), content.c_str());
                            callback(content,false);
                        }
                    }
                    else{
                        WARN("Invalid JSON string in response body from Gemini API: {}",errs);
                        continue;
                    }
                }
                }
                return true;  //继续接收响应
            };

        //发送请求
        auto result=client.send(request);
        if(!result){
            //请求，DNS问题
            DEBUG("GeminiProvider SendMessageStream request failed: {}",static_cast<int>(result.error()));
            return "";
        }
        //确保流式相应正确结束
        if(!streamfinished){
            WARN("GeminiProvider SendMessageStream stream not finished");
            callback("",true);
        }
        return fullresponse;
    
    }
}
