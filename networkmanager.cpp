#include "networkmanager.h"
#include <QUrl>
#include <QClipboard>

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent),
      m_geminiApi(m_config, m_memoryManager, m_chatManager, m_longTerm),
      m_speechApi(m_audioEngine)
{
    m_orchestrator = std::make_unique<MemoryOrchestrator>(
        m_longTerm, m_memoryManager, m_chatManager, m_config, m_nam, this);
    m_actions = std::make_unique<MessageActions>(
        m_memoryManager, m_longTerm, m_chatManager, m_geminiApi, m_audioEngine, this);
    m_persistence = std::make_unique<ChatPersistence>(
        m_chatManager, m_memoryManager, m_longTerm, this);

    connect(&m_audioEngine, &AudioEngine::ttsFinished, this, [this]() {
        if (!m_geminiApi.isBusy() && !m_speechApi.isBusy()) {
            emit processingFinished();
        }
    });

    // Ошибка движка речи (нет/битые модели) должна быть видна пользователю,
    // иначе голосовой вывод выглядит рабочим, но молча не работает.
    connect(&m_audioEngine, &AudioEngine::errorOccurred, this, &NetworkManager::errorOccurred);

    // Проброс сигналов истории чатов
    connect(m_persistence.get(), &ChatPersistence::chatListChanged, this, &NetworkManager::chatListChanged);

    // Проброс сигналов настроек
    connect(&m_config, &ApiConfigManager::languageChanged, this, &NetworkManager::languageChanged);
    connect(&m_config, &ApiConfigManager::perChatMemoryChanged, this, &NetworkManager::perChatMemoryChanged);
    connect(&m_config, &ApiConfigManager::speechLanguageAutoChanged, this, &NetworkManager::speechLanguageAutoChanged);

    // Подсказка языка для Groq Whisper
    const auto applySpeechLanguage = [this]() {
        m_speechApi.setLanguage(m_config.speechLanguageAuto() ? QString() : m_config.getLanguage());
    };
    applySpeechLanguage();
    connect(&m_config, &ApiConfigManager::languageChanged, this, applySpeechLanguage);
    connect(&m_config, &ApiConfigManager::speechLanguageAutoChanged, this, applySpeechLanguage);

    // MemoryOrchestrator errors
    connect(m_orchestrator.get(), &MemoryOrchestrator::errorOccurred, this, &NetworkManager::errorOccurred);

    // MessageActions и ChatPersistence никогда не эмитили errorOccurred —
    // подписки на них были пустыми и ничего не делали.

    // Стриминг: дельты уходят в UI сразу, в память ответ попадает только
    // целиком — в обработчике responseReceived ниже (как и до стриминга).
    connect(&m_geminiApi, &GeminiApi::streamStarted, this, &NetworkManager::streamStarted);
    connect(&m_geminiApi, &GeminiApi::responseChunk, this, &NetworkManager::responseChunk);

    // GeminiApi → NetworkManager: ответ привязан к исходному чату (переключились)
    connect(&m_geminiApi, &GeminiApi::chatSwitchedDuringRequest, this,
            [this](const QString &chatId, const QString &response) {
                m_persistence->saveResponseToOriginalChat(chatId, response);
                emit processingFinished();
            });

    // GeminiApi → NetworkManager: non-voice ответ (в чат не переключались)
    connect(&m_geminiApi, &GeminiApi::responseReceived, this,
            [this](const QString &response, bool voiceEnabled) {
                m_memoryManager.addMessage("model", response);
                emit responseReceived(response);
                if (voiceEnabled && m_voiceMode) {
                    m_actions->clearSpeakingMessage();
                    m_audioEngine.enqueueTextForTTS(response);
                } else {
                    emit processingFinished();
                }
                m_orchestrator->checkAndRunBackgroundMemoryTasks();
            });

    // GeminiApi → NetworkManager: ошибка сети
    connect(&m_geminiApi, &GeminiApi::errorReceived, this,
            [this](const QString &message) {
                emit errorOccurred(message);
                emit processingFinished();
            });

    // SpeechApi → NetworkManager: распознанная речь → обработка через Gemini
    connect(&m_speechApi, &SpeechApi::userVoiceRecognized, this,
            [this](const QString &text) {
                processUserMessage(text, m_voiceMode);
                emit userVoiceRecognized(text);
            });

    // SpeechApi → NetworkManager: ошибки расшифровки / отсутствие текста
    connect(&m_speechApi, &SpeechApi::responseReceived, this,
            &NetworkManager::errorOccurred);
    connect(&m_speechApi, &SpeechApi::processingFinished, this,
            &NetworkManager::processingFinished);

    // MessageActions signals
    connect(m_actions.get(), &MessageActions::speakingMessageIdChanged, this, &NetworkManager::speakingMessageIdChanged);
}

// НАСТРОЙКИ API (делегирование в ApiConfigManager)
// ============================================

void NetworkManager::saveApiKeys(const QString &geminiKey, const QString &groqKey)
{
    m_config.saveApiKeys(geminiKey, groqKey);
    m_speechApi.setGroqKey(groqKey);
}

bool NetworkManager::loadSavedKeys()
{
    bool ok = m_config.loadSavedKeys();
    m_speechApi.setGroqKey(m_config.getGroqKey());
    return ok;
}

QString NetworkManager::getGeminiKey() const { return m_config.getGeminiKey(); }
QString NetworkManager::getGroqKey() const { return m_config.getGroqKey(); }

void NetworkManager::setLanguage(const QString &langCode)
{
    m_config.setLanguage(langCode);
}

QString NetworkManager::getLanguage() const
{
    return m_config.getLanguage();
}

bool NetworkManager::perChatMemory() const
{
    return m_config.perChatMemory();
}

void NetworkManager::setPerChatMemory(bool on)
{
    m_config.setPerChatMemory(on);
}

bool NetworkManager::speechLanguageAuto() const
{
    return m_config.speechLanguageAuto();
}

void NetworkManager::setSpeechLanguageAuto(bool on)
{
    m_config.setSpeechLanguageAuto(on);
}

QString NetworkManager::getGreeting() const
{
    return m_config.getGreeting();
}

// ============================================
// ИСТОРИЯ ЧАТОВ (делегирование в ChatPersistence)
// ============================================

void NetworkManager::clearHistory()
{
    m_memoryManager.clear();
    qDebug() << "[NetworkManager] История сообщений очищена.";
}

QJsonArray NetworkManager::getChatList()
{
    return m_persistence->getChatList();
}

QString NetworkManager::createNewChat()
{
    return m_persistence->createNewChat();
}

void NetworkManager::saveCurrentChat()
{
    m_persistence->saveCurrentChat();
}

QJsonArray NetworkManager::loadChat(const QString &chatId)
{
    return m_persistence->loadChat(chatId);
}

QJsonArray NetworkManager::restoreLastSession()
{
    return m_persistence->restoreLastSession();
}

void NetworkManager::deleteChat(const QString &chatId)
{
    m_persistence->deleteChat(chatId);
}

void NetworkManager::deleteAllChats()
{
    m_persistence->deleteAllChats();
}

QString NetworkManager::getCurrentChatId()
{
    return m_persistence->getCurrentChatId();
}

// ============================================
// УПРАВЛЕНИЕ ОБРАБОТКОЙ
// ============================================

void NetworkManager::stopProcessing()
{
    qDebug() << "[NetworkManager] Запрос на остановку обработки.";
    m_audioEngine.stop();
    m_geminiApi.abortCurrentReply();
    m_speechApi.abortCurrentReply();
    emit processingFinished();
}

void NetworkManager::stopTts()
{
    qDebug() << "[NetworkManager] Запрос на остановку озвучивания ответа.";
    m_audioEngine.stop();
}

void NetworkManager::setVoiceMode(bool on)
{
    m_voiceMode = on;
    qDebug() << "[NetworkManager] Режим голосового ответа:" << (on ? "вкл" : "выкл");
}

void NetworkManager::sendMessage(const QString &message, bool voiceEnabled)
{
    processUserMessage(message, voiceEnabled);
}

void NetworkManager::startRecording()
{
    m_speechApi.startRecording();
}

void NetworkManager::stopRecording()
{
    m_speechApi.stopRecording();
}

// ============================================
// ОБРАБОТКА СООБЩЕНИЙ (оркестратор)
// ============================================

void NetworkManager::processUserMessage(const QString &text, bool voiceEnabled)
{
    if (text.isEmpty()) return;

    m_audioEngine.setStopRequested(false);
    m_memoryManager.addMessage("user", text);

    if (!m_longTerm.hasMemory()) {
        QString systemContext = m_geminiApi.buildMemoryContext(text, QVector<float>());
        m_geminiApi.callGeminiDirectly(systemContext, voiceEnabled);
        return;
    }

    m_geminiApi.embedQuery(text, voiceEnabled, true);
}

// ============================================
// ДЕЙСТВИЯ НАД ОТДЕЛЬНЫМИ СООБЩЕНИЯМИ (делегирование в MessageActions)
// ============================================

QString NetworkManager::lastMessageId()
{
    return m_actions->lastMessageId();
}

bool NetworkManager::deleteChatMessage(const QString &messageId)
{
    return m_actions->deleteChatMessage(messageId);
}

bool NetworkManager::rewriteChatMessage(const QString &messageId)
{
    return m_actions->rewriteChatMessage(messageId);
}

int NetworkManager::editUserMessage(const QString &messageId, const QString &newText)
{
    return m_actions->editUserMessage(messageId, newText);
}

void NetworkManager::speakMessage(const QString &messageId)
{
    m_actions->speakMessage(messageId);
}

void NetworkManager::copyToClipboard(const QString &text)
{
    m_actions->copyToClipboard(text);
}

QString NetworkManager::speakingMessageId() const
{
    return m_actions->speakingMessageId();
}
