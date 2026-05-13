#include "commen.h"
#include "SessionManager.h"
#include "LLMManager.h"
#include <map>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ai_chat_sdk{
class ChatSdk{
public:
    std::string testFunction();
    //初始化模型
    bool initModels(const std::vector<std::shared_ptr<ChatModelConfig>>& configs);
    //创建会话
    std::string createSession(const std::string& modelname);
    //获取会话
    std::shared_ptr<Session> getSession(const std::string& sessionid);
    //获取会话列表
    std::vector<std::string> getSessionsList();
    //删除会话
    bool deleteSession(const std::string& sessionid);
    //获取可用模型
    std::vector<ModelInfo> getAvailableModels();
    //发送消息，全量返回
    std::string sendMessage(const std::string& session_id,const std::string& message);
    //发送消息，增量返回 流式相应
    std::string sendMessageStream(const std::string& session_id,const std::string& message,
        std::function<void(const std::string&,bool)> callback);
    private:
    //注册所有模型
    void registerAllProvider(const std::vector<std::shared_ptr<ChatModelConfig>>& configs);
    //初始化所有模型提供者
    void initAllProviders(const std::vector<std::shared_ptr<ChatModelConfig>>& configs);
    //初始化模型提供者，通过api调用
    bool initProviderByApi(const std::string& modelname,const std::shared_ptr<ApiConfig>& config);
    //初始化ollama模型提供者
    bool initProviderByOllama(const std::string& modelname,const std::shared_ptr<OllamaConfig>& ollamaconfig);
    private:
    bool _initialized=false;
    std::unordered_map<std::string,std::shared_ptr<ChatModelConfig>> _modelconfigs;
    LLMManager _llmmanager;
    SessionManager _sessionmanager;
};
} //end ai_chat_sdk