#include "../include/DeepSeekProvider.h"
#include "../include/util/mylog.h"
#include <sstream>
#include <jsoncpp/json/json.h> 
#include <httplib.h>
namespace ai_chat_sdk{ 

    bool DeepSeekProvider::InitModel(const std::map<std::string,std::string>& modelConfig){
        //初始化API key和endpoint
        auto it = modelConfig.find("api_key");
        if(it == modelConfig.end()){
            ERROR("DeepSeekProvider initModel api_key not found");
            return false;
        }else{
            _apikey = it->second;
        }
        it = modelConfig.find("endpoint");
        if(it == modelConfig.end()){
            _endpoint = "https://api.deepseek.com";
        }else{
            _endpoint = it->second;
        }
        INFO("DeepSeekProvider initModel success");
        return true;
    }
    //检测模型是否可用
    bool DeepSeekProvider::IsModelAvailable() const{
        return _isAvailable;
    }

    //获取模型名称
    std::string DeepSeekProvider::GetModelName() const{
        return "deepseek-chat";
    }

    //获取模型描述
    std::string DeepSeekProvider::GetModelDesc() const{
        return "DeepSeek-Chat 是一款由深度求索公司打造的高性能对话大语言模型。";
    }

    //发送消息给模型，作为http请求
    std::string DeepSeekProvider::SendMessage(const std::vector<ChatMessage>& messages,const std::map<std::string,std::string>& requestparam){
        //发送消息 全量返回
        // if(!IsModelAvailable()){
        //     ERROR("DeepSeekProvider SendMessage model not available");
        //     return "";
        // }
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
        for(auto& msg:messages){
            Json::Value item;
            item["role"]=msg.role;
            item["content"]=msg.content;
            history.append(item);
        }

        //构建请求体
        Json::Value request_body;
        request_body["model"]=GetModelName();
        request_body["messages"]=history;
        request_body["temperature"]=temperature;
        request_body["max_tokens"]=max_tokens;
        request_body["stream"]=false;
        
        //序列化
        Json::StreamWriterBuilder writer;
        std::string Json_string=Json::writeString(writer,request_body);     
        DEBUG("DeepSeekProvider SendMessage request body:%s",Json_string.c_str());

        //创建http请求头
        httplib::Client client(_endpoint);
        client.set_connection_timeout(30,0); //30秒超时
        client.set_read_timeout(60,0); //60秒超时

        //设置请求头
        httplib::Headers headers;
        headers.emplace("Authorization","Bearer "+_apikey);
        headers.emplace("Content-Type","application/json");

        //发送POST请求
        auto res=client.Post("/v1/chat/completions",headers,Json_string,"application/json");
        if(res->status!=200){
            ERROR("DeepSeekProvider SendMessage error:{}",res->body);
            return "";
        }

        //解析响应体
        Json::Value response_json;
        Json::CharReaderBuilder reader_builder;
        std::string parse_errors;
        std::istringstream response_stream(res->body);
        if(!Json::parseFromStream(reader_builder,response_stream,&response_json,&parse_errors)){
            ERROR("DeepSeekProvider SendMessage error:{}",parse_errors);
            return "";
        }

        //解析大模型回复内容
        //大模型回复包含在jsonchoices数组的第一个元素的message.content字段中
        if(response_json.isMember("choices")&&response_json["choices"].isArray()&&!response_json["choices"].empty()){
            auto& choice=response_json["choices"][0];
            if(choice.isMember("message")&&choice["message"].isMember("content")){
                std::string reply_content=choice["message"]["content"].asString();
                //INFO("Received response:{}",reply_content);
                return reply_content;
            }
            
        }
        //解析失败 
            ERROR("Invalid response format from DeepSeek API");      
            return "Invalid response format from DeepSeek API";
       
    }

    
    std::string DeepSeekProvider::SendMessageStream(const std::vector<ChatMessage>& messages,
        const std::map<std::string,std::string>& requestparam,
        std::function<void(const std::string&,bool)> callback){
        //发送消息 增量返回 流式相应
        INFO("DeepSeekProvider SendMessageStream");
        // if(!IsModelAvailable()){
        //     ERROR("DeepSeekProvider SendMessageStream model not available");
        //     return "";
        // }
        //获取温度，max_tokens
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
        Json::Value request_body;
        request_body["model"]=GetModelName();
        request_body["messages"]=history;
        request_body["temperature"]=temperature;
        request_body["max_tokens"]=max_tokens;
        request_body["stream"]=true;

        //序列化
        Json::StreamWriterBuilder writer;
        std::string Json_string=Json::writeString(writer,request_body);     
        DEBUG("DeepSeekProvider SendMessageStream request body:{}",Json_string);

        //创建HttpClient
        httplib::Client client(_endpoint);
        client.set_connection_timeout(30,0); //30秒超时
        client.set_read_timeout(300,0); //60秒超时
        //设置请求头
        httplib::Headers headers;
        headers.emplace("Authorization","Bearer "+_apikey);
        headers.emplace("Content-Type","application/json");
        headers.emplace("Accept","application/json");
        
        //流式变量处理
        std::string reply_content="";   //接收流式处理结果
        bool gotError=false;            //标记是否错误
        std::string errorMsg="";        //错误描述符
        int status_code=0;              //状态码
        bool is_end=false;              //标记是否完成
        std::string full_reply="";      //完整回复

        //处理请求对象
        httplib::Request rep;
        rep.method="POST";
        rep.path="/v1/chat/completions";
        rep.headers=headers;
        rep.body=Json_string;
       //普通相应，一头一体
       //流式相应，一体被拆成多块，即一头多块

        rep.response_handler=[&](const httplib::Response& resp){
            status_code=resp.status;
            if(status_code!=200){
                gotError=true;
                errorMsg=resp.body;
                return false;  //错误，终止请求
            }
            return true;  //正常，继续请求
        };
        

        //响应体处理
        rep.content_receiver=[&](const char* data,size_t len,uint64_t offset,uint64_t total_len){
            //如果http请求错误，不需要再接收了
            if(gotError){
                return false;
            }

            //追加新数据
            reply_content+=std::string(data,len);
            std::cout<<"Received data:"<<reply_content<<std::endl;
       

        //处理所有完整数据
        size_t pos=0;
        while((pos=reply_content.find("\n\n",pos))!=std::string::npos){
            std::string event=reply_content.substr(0,pos);
            reply_content.erase(0,pos+2);   //移除已处理事件
            
            //处理空行和注释，以：开头的是注释行
            if(event.empty()||event[0]==':'){
                continue;
            }

            //检查事件类型
            if(event.compare(0,6,"data: ")==0){
                std::string jsonstr=event.substr(6);
                
                //处理结束标记
                if(jsonstr=="[DONE]"){
                    callback("",true);
                    is_end=true;
                    return true;
                }
                

                //解析Json数据
                Json::Value chunk;
                Json::CharReaderBuilder readerBuilder;
                std::string errs;
                std::istringstream jsonstream(jsonstr);
                
                if(Json::parseFromStream(readerBuilder,jsonstream,&chunk,&errs)){
                    //提取增量内容
                    if(chunk.isMember("choices")&&
                       chunk["choices"].isArray()&&
                       !chunk["choices"].empty()&&
                       chunk["choices"][0].isMember("delta")&&
                       chunk["choices"][0]["delta"].isMember("content")){
                        std::string content=chunk["choices"][0]["delta"]["content"].asString();
                        //累计到完整回复
                        full_reply+=content;
                        callback(content,false);
                    }
                }
                        else{
                    DEBUG("DeepSeekProvider SendMessageStream parse json error: %s",errs.c_str());
                    continue;
                }
            }
        }
        return true; //继续接收数据
        
        };
        //发送请求并处理结果
        auto res=client.send(rep);
        //send返回result类型，result 内置operator bool()隐式转换 bool类型 
        if(!res){
            //请求失败，网络问题，DNS解析失败等
            DEBUG("DeepSeekProvider SendMessageStream send error:%d",static_cast<int>(res.error()));
            return "";
        }

        //确保流式处理完成
        if(!is_end){
            WARN("DeepSeekProvider SendMessageStream not end");
            callback("",true);
        }
        return full_reply;
    }
}