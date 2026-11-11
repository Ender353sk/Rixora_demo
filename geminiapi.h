#ifndef GEMINIAPI_H
#define GEMINIAPI_H

#include <QObject>
#include <QVector>
#include <memory>

#include "geminiresponseparser.h"

class ApiConfigManager;
class MemoryManager;
class ChatManager;
class LongTermMemory;

class GeminiClient;
class MemoryContextBuilder;

class GeminiApi : public QObject
{
    Q_OBJECT

public:
    GeminiApi(ApiConfigManager &config,
              MemoryManager &memory,
              ChatManager &chat,
              LongTermMemory &longTerm,
              QObject *parent = nullptr);
    ~GeminiApi();  // Explicitly declared, defined in .cpp

    void callGeminiDirectly(const QString &systemContext, bool voiceEnabled);
    void embedQuery(const QString &text, bool voiceEnabled, bool allowRetry);
    QString buildMemoryContext(const QString &userText, const QVector<float> &queryVector);
    void abortCurrentReply();
    bool isBusy() const;

signals:
    // Дельты стриминга — только для UI; полный ответ уходит в responseReceived.
    void streamStarted();
    void responseChunk(const QString &delta);
    void responseReceived(const QString &response, bool voiceEnabled);
    void chatSwitchedDuringRequest(const QString &chatId, const QString &response);
    void errorReceived(const QString &message);

private:
    // Храним только memory-менеджер: он нужен для записи ответа в историю.
    // Остальные зависимости (config/chat/longTerm) нужны лишь на момент
    // конструирования — их получают GeminiClient и MemoryContextBuilder.
    MemoryManager &m_memoryManager;

    std::unique_ptr<GeminiClient> m_client;
    std::unique_ptr<MemoryContextBuilder> m_contextBuilder;
};

#endif // GEMINIAPI_H