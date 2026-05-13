#include <gtest/gtest.h>
#include "../sdk/include/OllamaLLMProvider.h"
#include "../sdk/include/DeepSeekProvider.h"
#include "../sdk/include/GeminiProvider.h"
#include "../sdk/include/util/mylog.h"
#include "../sdk/include/ChatSdk.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <map>
// //#ifdef ENABLE_DEEPSEEK
// TEST(DeepSeekProviderTest,SendMessageDeepSeek){
//     //实例化provide
//     auto provider=std::make_shared<ai_chat_sdk::DeepSeekProvider>();
//     ASSERT_TRUE(provider!=nullptr);

//     std::map<std::string,std::string> modelConfig;
//     modelConfig["apikey"]=std::getenv("deepseek_apikey");
//     modelConfig["endpoint"]="https://api.deepseek.com";
//     provider->InitModel(modelConfig);
//     ASSERT_TRUE(provider->IsModelAvailable());
    
//     std::map<std::string,std::string> requestparam;
//     requestparam["temperature"]="0.7";
//     requestparam["max_tokens"]="2048";
//     //构建历史消息
//     std::vector<ai_chat_sdk::ChatMessage> messages;
//     messages.push_back({"user","你是谁"});
//     messages.push_back({"assistant",""});
   

//     //发送消息
//     //std::string response=provider->SendMessage(messages,requestparam);
//     auto writechunk=[&](const std::string& chunk,bool is_end){
//         INFO("DeepSeekProvider SendMessageStream chunk:{}",chunk);
//         if(is_end){
//             INFO("DeepSeekProvider SendMessageStream end");
//         }
//     };
//     std::string response=provider->SendMessageStream(messages,requestparam,writechunk);
//     ASSERT_TRUE(!response.empty());
//     INFO("DeepSeekProvider SendMessageStream response:{}",response);
// }
// //#endif

// //#ifdef ENABLE_GEMINI
// TEST(GeminiProviderTest,SendMessageGemini){
//     //实例化provide
//     auto provider=std::make_shared<ai_chat_sdk::GeminiProvider>();
//     ASSERT_TRUE(provider!=nullptr);

//     std::map<std::string,std::string> modelConfig;
//     modelConfig["api_key"]=std::getenv("gemini_apikey");
//     modelConfig["endpoint"]="https://api.viviai.cc";
//     provider->InitModel(modelConfig);
//     ASSERT_TRUE(provider->IsModelAvailable());

//     std::map<std::string,std::string> requestparam;
//     requestparam["temperature"]="0.7";
//     requestparam["max_tokens"]="2048";
//     //构建历史消息
//     std::vector<ai_chat_sdk::ChatMessage> messages;
//     messages.push_back({"user","你是谁"});
//     messages.push_back({"assistant",""});
   
//     //std::string response=provider->SendMessage(messages,requestparam);
//     auto writechunk=[&](const std::string& chunk,bool is_end){
//         INFO("GeminiProvider SendMessageStream chunk:{}",chunk);
//         if(is_end){
//             INFO("GeminiProvider SendMessageStream end");
//         }
//     };
//     std::string response=provider->SendMessageStream(messages,requestparam,writechunk);
//     ASSERT_TRUE(!response.empty());
//     INFO("GeminiProvider SendMessageStream response:{}",response);
// }
// //#endif
// TEST(OllamaProviderTest,SendMessageOllama){
//     //实例化provide
//     auto provider=std::make_shared<ai_chat_sdk::OllamaLLMProvider>();
//     ASSERT_TRUE(provider!=nullptr);

//     std::map<std::string,std::string> modelConfig;  
//     modelConfig["model_name"]="tinyllama";
//     modelConfig["endpoint"]="http://127.0.0.1:11434";
//     modelConfig["model_desc"]="本地部署的tinyllama模型";
//     provider->InitModel(modelConfig);
//     ASSERT_TRUE(provider->IsModelAvailable());

   
//     //构建历史消息
//     std::vector<ai_chat_sdk::ChatMessage> messages;
//     messages.push_back({"user","你是谁"});
   
//     //std::string response=provider->SendMessage(messages,modelConfig);
//     auto writechunk=[&](const std::string& chunk,bool is_end){
//         INFO("OllamaProvider SendMessageStream chunk:{}",chunk);
//         if(is_end){
//             INFO("OllamaProvider SendMessageStream end");
//         }
//     };
//     std::string response=provider->SendMessageStream(messages,modelConfig,writechunk);
//     ASSERT_TRUE(!response.empty());
//     INFO("OllamaProvider SendMessageStream response:{}",response);
// }

TEST(ChatSdkTest,SendMessage){
    auto sdk=std::make_shared<ai_chat_sdk::ChatSdk>();
    ASSERT_TRUE(sdk!=nullptr);

    //配置支持的参数 云模型 deepseek-chat ，gemini-2.5-flash ，本地接入 Ollama-tinyllama
    //deepseek-chat
    auto deepseekconfig=std::make_shared<ai_chat_sdk::ApiConfig>();
    ASSERT_TRUE(deepseekconfig!=nullptr);
    deepseekconfig->model_name="deepseek-chat";
    deepseekconfig->api_key=std::getenv("deepseek_apikey");
    ASSERT_TRUE(!deepseekconfig->api_key.empty());
    deepseekconfig->temperature="0.7";
    deepseekconfig->max_tokens="2048";
    
    //gemini-2.5-flash
    auto geminiconfig=std::make_shared<ai_chat_sdk::ApiConfig>();
    ASSERT_TRUE(geminiconfig!=nullptr);
    geminiconfig->model_name="gemini-2.5-flash";
    geminiconfig->api_key=std::getenv("gemini_apikey");
    ASSERT_TRUE(!geminiconfig->api_key.empty());
    geminiconfig->temperature="0.7";
    geminiconfig->max_tokens="2048";

    //Ollama-tinyllama
    auto ollamaconfig=std::make_shared<ai_chat_sdk::OllamaConfig>();
    ASSERT_TRUE(ollamaconfig!=nullptr);
    ollamaconfig->model_name="tinyllama";
    ollamaconfig->endpoint="http://localhost:11434";
    ollamaconfig->model_desc="本地部署的tinyllama模型";

    std::vector<std::shared_ptr<ai_chat_sdk::ChatModelConfig>> modelcondigs={deepseekconfig,geminiconfig,ollamaconfig};
    sdk->initModels(modelcondigs);
    
     // 添加流式回调函数
    auto writechunk = [](const std::string& chunk, bool is_end) {
        if(is_end) {
            INFO("ChatSdk SendMessageStream end");
        } else {
            std::cout << chunk << std::flush;  // 实时输出chunk内容
            INFO("ChatSdk SendMessageStream chunk:{}", chunk);
        }
    };
    //创建会话
    auto sessionid=sdk->createSession(geminiconfig->model_name);
    ASSERT_TRUE(!sessionid.empty());

    std::string message;
    std::cout<<">>>";
    getline(std::cin,message);
    auto response=sdk->sendMessageStream(sessionid,message,writechunk);
    ASSERT_TRUE(!response.empty());

    // std::string message;
    // std::cout<<">>>";
    // getline(std::cin,message);
    // sdk->sendMessage(sessionid,message);
    // ASSERT_TRUE(!response.empty());
}

int main(int argc,char* argv[])
{
    //初始化日志
    Bite::Logger::InitLogger("aiChatServer","stdout",spdlog::level::debug);
    //初始化gtest库
    testing::InitGoogleTest(&argc, argv);
    //运行测试
    return RUN_ALL_TESTS();
}