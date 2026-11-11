#ifndef MESSAGEACTIONS_H
#define MESSAGEACTIONS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QClipboard>

class MemoryManager;
class LongTermMemory;
class ChatManager;
class GeminiApi;
class AudioEngine;
class ApiConfigManager;

class MessageActions : public QObject
{
    Q_OBJECT
public:
    explicit MessageActions(MemoryManager &memoryManager,
                            LongTermMemory &longTerm,
                            ChatManager &chatManager,
                            GeminiApi &geminiApi,
                            AudioEngine &audioEngine,
                            QObject *parent = nullptr);

    bool deleteChatMessage(const QString &messageId);
    bool rewriteChatMessage(const QString &messageId);
    int editUserMessage(const QString &messageId, const QString &newText);
    void speakMessage(const QString &messageId);
    void copyToClipboard(const QString &text);
    QString lastMessageId() const;

    // Сброс активной озвучки (ответ ассистента начинает звучать сам).
    void clearSpeakingMessage();
    QString speakingMessageId() const;

signals:
    void speakingMessageIdChanged();

private:
    MemoryManager &m_memoryManager;
    LongTermMemory &m_longTerm;
    ChatManager &m_chatManager;
    GeminiApi &m_geminiApi;
    AudioEngine &m_audioEngine;
    QString m_speakingMessageId;
};

#endif // MESSAGEACTIONS_H
