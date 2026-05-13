const API_BASE_URL = 'http://115.175.6.15:8080';
const MAX_MESSAGE_LENGTH = 2000;
const STREAM_RENDER_INTERVAL = 30; // 减小到30ms，让流式显示更流畅

const state = {
    sessions: [],
    models: [],
    currentSessionId: null,
    currentModel: '',
    selectedModel: '',
    isSending: false,
    isCreatingSession: false,
    streamBuffer: '',
};

const elements = {
    newChatBtn: document.getElementById('newChatBtn'),
    welcomeNewChatBtn: document.getElementById('welcomeNewChatBtn'),
    sessionList: document.getElementById('sessionList'),
    sessionCount: document.getElementById('sessionCount'),
    welcomeState: document.getElementById('welcomeState'),
    chatState: document.getElementById('chatState'),
    chatTitle: document.getElementById('chatTitle'),
    chatModelBadge: document.getElementById('chatModelBadge'),
    chatMetaText: document.getElementById('chatMetaText'),
    messagesContainer: document.getElementById('messagesContainer'),
    messageInput: document.getElementById('messageInput'),
    sendBtn: document.getElementById('sendBtn'),
    charCount: document.getElementById('charCount'),
    modelModal: document.getElementById('modelModal'),
    modelGrid: document.getElementById('modelGrid'),
    closeModal: document.getElementById('closeModal'),
    cancelBtn: document.getElementById('cancelBtn'),
    confirmBtn: document.getElementById('confirmBtn'),
    loadingOverlay: document.getElementById('loadingOverlay'),
    loadingText: document.getElementById('loadingText'),
    toastContainer: document.getElementById('toastContainer'),
    headerStatus: document.getElementById('headerStatus'),
};

document.addEventListener('DOMContentLoaded', () => {
    configureMarkdown();
    bindEvents();
    updateCharCount();
    initializePage();
    addStreamingStyles();
});

function configureMarkdown() {
    if (typeof marked === 'undefined') {
        return;
    }

    marked.use({
        breaks: true,
        gfm: true,
    });
}

function bindEvents() {
    elements.newChatBtn.addEventListener('click', openModelModal);
    elements.welcomeNewChatBtn.addEventListener('click', openModelModal);
    elements.closeModal.addEventListener('click', closeModelModal);
    elements.cancelBtn.addEventListener('click', closeModelModal);
    elements.confirmBtn.addEventListener('click', createSessionFromSelection);
    elements.sendBtn.addEventListener('click', sendMessage);
    elements.messageInput.addEventListener('input', updateCharCount);
    elements.messageInput.addEventListener('keydown', handleComposerKeydown);

    elements.modelModal.addEventListener('click', (event) => {
        if (event.target === elements.modelModal) {
            closeModelModal();
        }
    });
}

async function initializePage() {
    setLoading(true, '正在加载会话列表...');
    try {
        await loadSessions();
        switchToWelcome();
    } catch (error) {
        console.error(error);
    } finally {
        setLoading(false);
    }
}

async function loadSessions() {
    const result = await requestJson('/api/sessions');
    const data = Array.isArray(result.data) ? result.data.slice() : [];

    state.sessions = data.sort((a, b) => Number(b.updated_at || 0) - Number(a.updated_at || 0));
    renderSessionList();

    if (state.currentSessionId) {
        const currentSession = state.sessions.find((item) => item.id === state.currentSessionId);
        if (!currentSession) {
            state.currentSessionId = null;
            state.currentModel = '';
            switchToWelcome();
        }
    }
}

async function loadModels() {
    const result = await requestJson('/api/models');
    const data = Array.isArray(result.data) ? result.data : [];
    state.models = data;
    return data;
}

function renderSessionList() {
    elements.sessionList.innerHTML = '';
    elements.sessionCount.textContent = `${state.sessions.length} 条`;

    state.sessions.forEach((session) => {
        const item = document.createElement('article');
        item.className = 'session-item';
        if (session.id === state.currentSessionId) {
            item.classList.add('active');
        }

        const firstMessage = (session.first_user_message || '').trim() || '新对话，等待第一条消息';
        item.innerHTML = `
            <div class="session-top">
                <div class="session-time">${escapeHtml(formatSessionTime(session.updated_at || session.created_at))}</div>
                <button class="session-delete" type="button" data-session-delete="${escapeAttribute(session.id)}">...</button>
            </div>
            <div class="session-message">${escapeHtml(firstMessage)}</div>
            <div class="session-meta">
                <span class="message-model">${escapeHtml(session.model || '未命名模型')}</span>
                <span class="message-total">${Number(session.message_count || 0)} 条消息</span>
            </div>
        `;

        item.addEventListener('click', () => selectSession(session.id));

        const deleteButton = item.querySelector('[data-session-delete]');
        deleteButton.addEventListener('click', async (event) => {
            event.stopPropagation();
            await deleteSession(session.id);
        });

        elements.sessionList.appendChild(item);
    });
}

async function selectSession(sessionId) {
    if (!sessionId || state.isSending) {
        return;
    }

    const session = state.sessions.find((item) => item.id === sessionId);
    if (!session) {
        showToast('未找到对应会话', 'error');
        return;
    }

    setLoading(true, '正在加载历史消息...');
    try {
        state.currentSessionId = session.id;
        state.currentModel = session.model || '';
        renderSessionList();
        switchToChat(session);

        const history = await loadSessionHistory(session.id);
        renderHistory(history);
    } catch (error) {
        showToast(error.message || '加载历史消息失败', 'error');
    } finally {
        setLoading(false);
    }
}

async function loadSessionHistory(sessionId) {
    const result = await requestJson(`/api/session/${encodeURIComponent(sessionId)}/history`);
    return Array.isArray(result.data) ? result.data : [];
}

async function openModelModal() {
    setLoading(true, '正在获取模型列表...');
    try {
        await loadModels();
        state.selectedModel = state.currentModel || (state.models[0] ? state.models[0].name : '');
        renderModelGrid();
        elements.modelModal.classList.remove('hidden');
    } catch (error) {
        showToast(error.message || '获取模型列表失败', 'error');
    } finally {
        setLoading(false);
    }
}

function closeModelModal() {
    elements.modelModal.classList.add('hidden');
}

function renderModelGrid() {
    elements.modelGrid.innerHTML = '';

    if (!state.models.length) {
        const empty = document.createElement('div');
        empty.className = 'session-item';
        empty.textContent = '暂无可用模型';
        elements.modelGrid.appendChild(empty);
        return;
    }

    state.models.forEach((model, index) => {
        const card = document.createElement('label');
        card.className = 'model-card';
        if (model.name === state.selectedModel || (!state.selectedModel && index === 0)) {
            card.classList.add('selected');
            state.selectedModel = model.name;
        }

        card.innerHTML = `
            <input type="radio" name="modelOption" value="${escapeAttribute(model.name)}" ${model.name === state.selectedModel ? 'checked' : ''}>
            <h4>${escapeHtml(model.name)}</h4>
            <p>${escapeHtml(model.desc || '暂无模型描述')}</p>
        `;

        const input = card.querySelector('input');
        input.addEventListener('change', () => {
            state.selectedModel = model.name;
            Array.from(elements.modelGrid.querySelectorAll('.model-card')).forEach((item) => {
                item.classList.remove('selected');
            });
            card.classList.add('selected');
        });

        elements.modelGrid.appendChild(card);
    });
}

async function createSessionFromSelection() {
    if (state.isCreatingSession) {
        return;
    }

    if (!state.selectedModel) {
        showToast('请选择一个模型', 'error');
        return;
    }

    state.isCreatingSession = true;
    elements.confirmBtn.disabled = true;
    setLoading(true, '正在创建新会话...');

    try {
        const result = await requestJson('/api/session', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                model: state.selectedModel,
            }),
        });

        const sessionId = result.data && result.data.session_id;
        if (!sessionId) {
            throw new Error('创建会话失败：未返回会话 ID');
        }

        state.currentSessionId = sessionId;
        state.currentModel = result.data.model || state.selectedModel;
        closeModelModal();
        await loadSessions();
        const currentSession = state.sessions.find((item) => item.id === state.currentSessionId) || {
            id: sessionId,
            model: state.currentModel,
            first_user_message: '',
            updated_at: Date.now(),
        };
        switchToChat(currentSession);
        renderHistory([]);
        showToast('新会话已创建', 'success');
        focusComposer();
    } catch (error) {
        showToast(error.message || '创建会话失败', 'error');
    } finally {
        state.isCreatingSession = false;
        elements.confirmBtn.disabled = false;
        setLoading(false);
    }
}

async function deleteSession(sessionId) {
    const target = state.sessions.find((item) => item.id === sessionId);
    const firstMessage = target && target.first_user_message ? `“${target.first_user_message.slice(0, 16)}”` : '该会话';
    if (!window.confirm(`确定删除${firstMessage}吗？`)) {
        return;
    }

    setLoading(true, '正在删除会话...');
    try {
        await requestJson(`/api/session/${encodeURIComponent(sessionId)}`, {
            method: 'DELETE',
        });

        if (sessionId === state.currentSessionId) {
            state.currentSessionId = null;
            state.currentModel = '';
            switchToWelcome();
        }

        await loadSessions();
        showToast('会话已删除', 'success');
    } catch (error) {
        showToast(error.message || '删除会话失败', 'error');
    } finally {
        setLoading(false);
    }
}

function switchToWelcome() {
    elements.welcomeState.classList.remove('hidden');
    elements.chatState.classList.add('hidden');
    elements.chatTitle.textContent = 'AI聊天助手';
    elements.chatModelBadge.textContent = '未选择模型';
    elements.chatMetaText.textContent = '请选择历史会话或创建新会话';
    elements.messagesContainer.innerHTML = '';
}

function switchToChat(session) {
    elements.welcomeState.classList.add('hidden');
    elements.chatState.classList.remove('hidden');
    elements.chatTitle.textContent = session.first_user_message ? truncateText(session.first_user_message, 28) : '新的对话';
    elements.chatModelBadge.textContent = session.model || state.currentModel || '未选择模型';
    elements.chatMetaText.textContent = `会话更新时间：${formatSessionTime(session.updated_at || session.created_at || Date.now())}`;
}

function renderHistory(history) {
    elements.messagesContainer.innerHTML = '';

    history.forEach((message) => {
        const item = createMessageElement(message.role, message.content || '', message.timestamp || Date.now());
        elements.messagesContainer.appendChild(item);
        renderMessageContent(item, message.content || '', false);
    });

    scrollMessagesToBottom();
}

function createMessageElement(role, content, timestamp) {
    const normalizedRole = role === 'user' ? 'user' : 'assistant';
    const article = document.createElement('article');
    article.className = `message ${normalizedRole}`;
    article.dataset.role = normalizedRole;
    article.dataset.rawContent = content || '';

    const roleName = normalizedRole === 'user' ? '我' : 'AI助手';
    const iconClass = normalizedRole === 'user' ? 'fa-user' : 'fa-robot';

    article.innerHTML = `
        <div class="message-avatar" aria-hidden="true"><i class="fas ${iconClass}"></i></div>
        <div class="message-bubble">
            <div class="message-header">
                <span class="message-role">${roleName}</span>
                <span class="model-pill">${escapeHtml(normalizedRole === 'assistant' ? (state.currentModel || 'AI模型') : '用户消息')}</span>
            </div>
            <div class="message-body"></div>
            <time class="message-time">${escapeHtml(formatMessageTime(timestamp))}</time>
        </div>
    `;

    return article;
}

function appendUserMessage(content) {
    const item = createMessageElement('user', content, Date.now());
    elements.messagesContainer.appendChild(item);
    renderMessageContent(item, content, false);
    scrollMessagesToBottom();
    return item;
}

function appendStreamingAssistantMessage() {
    const item = createMessageElement('assistant', '', Date.now());
    item.classList.add('is-streaming');
    elements.messagesContainer.appendChild(item);
    renderMessageContent(item, '', true);
    scrollMessagesToBottom();
    return item;
}

function renderMessageContent(messageElement, content, isStreaming) {
    const body = messageElement.querySelector('.message-body');
    messageElement.dataset.rawContent = content;

    if (!content && isStreaming) {
        body.innerHTML = '<div class="streaming-indicator"><i class="fas fa-circle-notch fa-spin"></i><span>思考中...</span></div>';
        return;
    }

    // 流式渲染时显示光标效果
    if (isStreaming && content) {
        let html = '';
        if (typeof marked !== 'undefined') {
            html = marked.parse(content || '');
            // 让流式内容更自然地流动
            html = html.replace(/<p>/g, '<span class="streaming-text">').replace(/<\/p>/g, '</span><br>');
        } else {
            html = escapeHtml(content || '');
        }
        body.innerHTML = html + '<span class="streaming-cursor">▊</span>';
    } else {
        const html = typeof marked !== 'undefined' ? marked.parse(content || '') : escapeHtml(content || '');
        body.innerHTML = html;
    }
    
    enhanceCodeBlocks(body);
}

function enhanceCodeBlocks(container) {
    const codeBlocks = container.querySelectorAll('pre');
    codeBlocks.forEach((pre) => {
        if (!pre.parentElement.classList.contains('code-block')) {
            const wrapper = document.createElement('div');
            wrapper.className = 'code-block';
            pre.parentNode.insertBefore(wrapper, pre);
            wrapper.appendChild(pre);

            const copyButton = document.createElement('button');
            copyButton.className = 'copy-code-btn';
            copyButton.type = 'button';
            copyButton.textContent = '复制代码';
            copyButton.addEventListener('click', async () => {
                try {
                    await navigator.clipboard.writeText(pre.innerText);
                    copyButton.textContent = '已复制';
                    window.setTimeout(() => {
                        copyButton.textContent = '复制代码';
                    }, 1200);
                } catch (error) {
                    showToast('复制失败，请手动复制', 'error');
                }
            });
            wrapper.appendChild(copyButton);
        }

        const code = pre.querySelector('code');
        if (code && typeof hljs !== 'undefined' && !code.dataset.highlighted) {
            try {
                hljs.highlightElement(code);
                code.dataset.highlighted = 'true';
            } catch (error) {
                console.error('代码高亮失败', error);
            }
        }
    });
}

function handleComposerKeydown(event) {
    if (event.key === 'Enter' && !event.shiftKey) {
        event.preventDefault();
        sendMessage();
    }
}

function updateCharCount() {
    const length = elements.messageInput.value.length;
    elements.charCount.textContent = `${length}/${MAX_MESSAGE_LENGTH}`;
}

async function sendMessage() {
    if (state.isSending) {
        return;
    }

    const message = elements.messageInput.value.trim();
    if (!message) {
        showToast('请输入消息内容', 'error');
        return;
    }

    if (!state.currentSessionId) {
        showToast('请先选择或创建会话', 'error');
        return;
    }

    state.isSending = true;
    elements.sendBtn.disabled = true;

    appendUserMessage(message);
    elements.messageInput.value = '';
    updateCharCount();
    focusComposer();

    const assistantMessage = appendStreamingAssistantMessage();

    try {
        const controller = new AbortController();
        const timeout = window.setTimeout(() => controller.abort(), 120000);

        const response = await fetch(`${API_BASE_URL}/api/message/async`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                session_id: state.currentSessionId,
                message,
            }),
            signal: controller.signal,
        });

        window.clearTimeout(timeout);

        if (!response.ok || !response.body) {
            throw new Error(`消息发送失败，状态码：${response.status}`);
        }

        await consumeSseStream(response.body, assistantMessage);
        await loadSessions();
        renderSessionList();
    } catch (error) {
        const body = assistantMessage.querySelector('.message-body');
        body.innerHTML = '<div class="streaming-indicator"><i class="fas fa-circle-exclamation"></i><span>消息发送失败，请稍后重试</span></div>';
        showToast(error.name === 'AbortError' ? '请求超时，请稍后重试' : (error.message || '消息发送失败'), 'error');
        assistantMessage.classList.remove('is-streaming');
    } finally {
        state.isSending = false;
        elements.sendBtn.disabled = false;
    }
}

async function consumeSseStream(stream, messageElement) {
    const reader = stream.getReader();
    const decoder = new TextDecoder();
    let done = false;
    let sseBuffer = '';
    let accumulated = '';
    let lastRenderTime = 0;
    let chunkCount = 0;

    console.log('开始接收 SSE 流...');

    while (!done) {
        const result = await reader.read();
        done = result.done;
        
        const decodedChunk = decoder.decode(result.value || new Uint8Array(), { stream: !done });
        sseBuffer += decodedChunk;
        
        chunkCount++;
        console.log(`收到数据块 ${chunkCount}:`, decodedChunk.substring(0, 200));

        const parts = sseBuffer.split('\n\n');
        sseBuffer = parts.pop() || '';

        for (const part of parts) {
            const lines = part.split('\n');
            const payload = lines
                .filter((line) => line.startsWith('data:'))
                .map((line) => line.replace(/^data:\s?/, ''))
                .join('\n');

            if (!payload) {
                continue;
            }

            if (payload === '[DONE]') {
                console.log('SSE 流完成，最终内容长度:', accumulated.length);
                renderMessageContent(messageElement, accumulated, false);
                scrollMessagesToBottom();
                messageElement.classList.remove('is-streaming');
                return;
            }

            const chunkContent = decodeStreamChunk(payload);
            
            // 累积内容（即使是空字符串也不影响）
            if (chunkContent !== undefined && chunkContent !== null) {
                accumulated += chunkContent;
            }
            
            // 更流畅的渲染：控制渲染频率
            const now = Date.now();
            if (accumulated && (now - lastRenderTime >= STREAM_RENDER_INTERVAL || lastRenderTime === 0)) {
                renderMessageContent(messageElement, accumulated, true);
                scrollMessagesToBottom();
                lastRenderTime = now;
            }
        }
    }

    // 流结束时确保显示完整内容
    if (accumulated) {
        console.log('SSE 流结束，最终内容长度:', accumulated.length);
        renderMessageContent(messageElement, accumulated, false);
        scrollMessagesToBottom();
    } else {
        console.warn('SSE 流结束但没有收到任何内容');
        const body = messageElement.querySelector('.message-body');
        if (body && body.innerHTML.includes('streaming-indicator')) {
            body.innerHTML = '<div class="streaming-indicator"><i class="fas fa-circle-exclamation"></i><span>未收到回复内容</span></div>';
        }
    }
    
    messageElement.classList.remove('is-streaming');
}

function decodeStreamChunk(payload) {
    const trimmed = payload.trim();
    if (!trimmed) {
        return '';
    }

    try {
        if ((trimmed.startsWith('{') && trimmed.endsWith('}')) || 
            (trimmed.startsWith('[') && trimmed.endsWith(']')) || 
            (trimmed.startsWith('"') && trimmed.endsWith('"'))) {
            const parsed = JSON.parse(trimmed);
            
            if (typeof parsed === 'string') {
                return parsed;
            }
            
            if (parsed && typeof parsed === 'object') {
                // 处理 OpenAI 格式的流式响应
                if (parsed.choices && Array.isArray(parsed.choices) && parsed.choices[0]) {
                    const choice = parsed.choices[0];
                    
                    // 处理 delta 对象中的 content
                    if (choice.delta) {
                        if (choice.delta.content !== undefined && choice.delta.content !== null) {
                            return choice.delta.content;
                        }
                        // 只有 role 没有 content 时返回空字符串
                        if (choice.delta.role !== undefined && choice.delta.content === undefined) {
                            return '';
                        }
                    }
                    
                    // 处理非流式格式
                    if (choice.message && choice.message.content !== undefined) {
                        return choice.message.content;
                    }
                    if (choice.text !== undefined) {
                        return choice.text;
                    }
                }
                
                // 处理其他常见格式
                if (parsed.content !== undefined) return parsed.content;
                if (parsed.text !== undefined) return parsed.text;
                if (parsed.message !== undefined) return parsed.message;
                if (parsed.delta !== undefined) return parsed.delta;
            }
        }
    } catch (error) {
        // JSON 解析失败，返回原始 payload 作为兜底
        console.debug('JSON 解析失败，使用原始内容:', error.message);
    }
    
    // 如果不是 JSON 格式，直接返回原始内容
    return payload;
}

async function requestJson(path, options = {}) {
    const response = await fetch(`${API_BASE_URL}${path}`, options);
    let result = null;

    try {
        result = await response.json();
    } catch (error) {
        throw new Error('服务器返回了无法解析的响应');
    }

    if (!response.ok || !result.success) {
        throw new Error(result && result.message ? result.message : '请求失败');
    }

    elements.headerStatus.textContent = '已连接到 115.175.6.15:8080';
    return result;
}

function setLoading(visible, text = '处理中...') {
    elements.loadingText.textContent = text;
    elements.loadingOverlay.classList.toggle('hidden', !visible);
}

function showToast(message, type = 'info') {
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    elements.toastContainer.appendChild(toast);

    window.setTimeout(() => {
        toast.remove();
    }, 2600);
}

function scrollMessagesToBottom() {
    requestAnimationFrame(() => {
        elements.messagesContainer.scrollTop = elements.messagesContainer.scrollHeight;
    });
}

function focusComposer() {
    requestAnimationFrame(() => {
        elements.messageInput.focus();
    });
}

function formatSessionTime(timestamp) {
    const date = parseTimestamp(timestamp);
    const now = new Date();
    const isToday = date.toDateString() === now.toDateString();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    return isToday ? `今天 ${hours}:${minutes}` : `${month}-${day} ${hours}:${minutes}`;
}

function formatMessageTime(timestamp) {
    const date = parseTimestamp(timestamp);
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    return `${hours}:${minutes}`;
}

function parseTimestamp(timestamp) {
    const numeric = Number(timestamp || Date.now());
    if (numeric > 1e12) {
        return new Date(numeric);
    }
    return new Date(numeric * 1000);
}

function truncateText(text, maxLength) {
    if (!text || text.length <= maxLength) {
        return text || '';
    }
    return `${text.slice(0, maxLength)}...`;
}

function escapeHtml(value) {
    return String(value)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function escapeAttribute(value) {
    return escapeHtml(value).replace(/`/g, '&#96;');
}

function addStreamingStyles() {
    const style = document.createElement('style');
    style.textContent = `
        .streaming-cursor {
            display: inline-block;
            animation: blink 1s step-end infinite;
            color: #6c5ce7;
            font-weight: normal;
            margin-left: 2px;
        }
        
        @keyframes blink {
            0%, 100% { opacity: 1; }
            50% { opacity: 0; }
        }
        
        .streaming-text {
            display: inline;
        }
        
        .message-body {
            transition: all 0.05s ease;
        }
        
        .message.is-streaming {
            background: rgba(108, 92, 231, 0.03);
            transition: background 0.3s ease;
        }
        
        .message.is-streaming .message-bubble {
            border-left: 2px solid #6c5ce7;
        }
    `;
    document.head.appendChild(style);
}