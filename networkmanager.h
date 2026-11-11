#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

#include "apiconfigmanager.h"
#include "geminiapi.h"
#include "speechapi.h"
#include "memorymanager.h"
#include "audioengine.h"
#include "chatmanager.h"
#include "longtermmemory.h"
#include "memoryorchestrator.h"
#include "messageactions.h"
#include "chatpersistence.h"

class NetworkManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ getLanguage WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool perChatMemory READ perChatMemory WRITE setPerChatMemory NOTIFY perChatMemoryChanged)
    Q_PROPERTY(bool speechLanguageAuto READ speechLanguageAuto WRITE setSpeechLanguageAuto NOTIFY speechLanguageAutoChanged)
    Q_PROPERTY(QString speakingMessageId READ speakingMessageId NOTIFY speakingMessageIdChanged)

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager() override = default;

    Q_INVOKABLE void saveApiKeys(const QString &geminiKey, const QString &groqKey);
    Q_INVOKABLE bool loadSavedKeys();
    Q_INVOKABLE QString getGeminiKey() const;
    Q_INVOKABLE QString getGroqKey() const;

    Q_INVOKABLE void setLanguage(const QString &langCode);
    Q_INVOKABLE QString getLanguage() const;
    Q_INVOKABLE QString getGreeting() const;

    bool perChatMemory() const;
    void setPerChatMemory(bool on);

    bool speechLanguageAuto() const;
    void setSpeechLanguageAuto(bool on);

    Q_INVOKABLE void sendMessage(const QString &message, bool voiceEnabled = false);

    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void stopProcessing();
    Q_INVOKABLE void stopTts();
    Q_INVOKABLE void setVoiceMode(bool on);
    Q_INVOKABLE void clearHistory();

    Q_INVOKABLE QJsonArray getChatList();
    Q_INVOKABLE QString createNewChat();
    Q_INVOKABLE void saveCurrentChat();
    Q_INVOKABLE QJsonArray loadChat(const QString &chatId);
    Q_INVOKABLE QJsonArray restoreLastSession();
    Q_INVOKABLE void deleteChat(const QString &chatId);
    Q_INVOKABLE void deleteAllChats();
    Q_INVOKABLE QString getCurrentChatId();

    Q_INVOKABLE QString lastMessageId();
    Q_INVOKABLE bool deleteChatMessage(const QString &messageId);
    Q_INVOKABLE bool rewriteChatMessage(const QString &messageId);
    Q_INVOKABLE int editUserMessage(const QString &messageId, const QString &newText);
    Q_INVOKABLE void speakMessage(const QString &messageId);
    Q_INVOKABLE void copyToClipboard(const QString &text);

    QString speakingMessageId() const;

signals:
    // Стриминг ответа: пустой пузырь (streamStarted), затем дельты текста
    // (responseChunk). Полный ответ — responseReceived, он, как и раньше,
    // идёт в память и озвучку одним куском.
    void streamStarted();
    void responseChunk(const QString &delta);
    void responseReceived(const QString &response);
    void errorOccurred(const QString &message);
    void userVoiceRecognized(const QString &text);
      void processingFinished();
      void languageChanged();
      void perChatMemoryChanged();
      void speechLanguageAutoChanged();
      void chatListChanged();
      void speakingMessageIdChanged();


private:
    void processUserMessage(const QString &text, bool voiceEnabled);

    QNetworkAccessManager m_nam;

    // Subsystems (owned)
    // ПОРЯДОК ОБЪЯВЛЕНИЯ ВАЖЕН и должен совпадать с порядком в списке
    // инициализации конструктора: m_speechApi(m_audioEngine) получает ССЫЛКУ,
    // поэтому m_audioEngine обязан быть сконструирован раньше m_speechApi.
    // AudioEngine больше не эмитит сигналы из деструктора, поэтому порядок
    // разрушения больше не накладывает ограничений.
    ApiConfigManager m_config;
    MemoryManager m_memoryManager;
    AudioEngine m_audioEngine;
    ChatManager m_chatManager;
    LongTermMemory m_longTerm;

    // API clients (owned)
    GeminiApi m_geminiApi;
    SpeechApi m_speechApi;

    // Refactored components
    std::unique_ptr<MemoryOrchestrator> m_orchestrator;
    std::unique_ptr<MessageActions> m_actions;
    std::unique_ptr<ChatPersistence> m_persistence;

    // Voice mode state
    bool m_voiceMode = false;
};

#endif // NETWORKMANAGER_H
