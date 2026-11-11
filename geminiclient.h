#ifndef GEMINICLIENT_H
#define GEMINICLIENT_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ApiConfigManager;
class ChatManager;

// Сетевой слой Gemini: стриминговый generateContent (SSE) / embedContent,
// отмена запросов, учёт поколений ответов и определение переключения чата
// во время запроса.
class GeminiClient : public QObject
{
    Q_OBJECT
public:
    GeminiClient(ApiConfigManager &config, ChatManager &chatManager, QObject *parent = nullptr);

    void callGeminiDirectly(const QString &systemContext, const QJsonArray &history, bool voiceEnabled);
    void embedQuery(const QString &text, bool voiceEnabled, bool allowRetry);
    void abortCurrentReply();
    bool isBusy() const;

signals:
    // Стриминг: первый чанк (создать пузырь в UI), затем дельты текста.
    // Полный ответ уходит в responseReceived — память и озвучка получают
    // его без изменений, дельты видит только интерфейс.
    void streamStarted();
    void responseChunk(const QString &delta);
    void responseReceived(const QString &response, bool voiceEnabled);
    void chatSwitchedDuringRequest(const QString &chatId, const QString &response);
    void errorReceived(const QString &message);
    void embedCompleted(const QString &text, const QVector<float> &embedding, bool voiceEnabled);

private:
    ApiConfigManager &m_config;
    ChatManager &m_chatManager;
    QNetworkAccessManager m_networkAccessManager;
    QNetworkReply *m_currentReply = nullptr;
    QNetworkReply *m_embedReply = nullptr;
    quint64 m_generation = 0;

    // Разбор SSE-потока: неполные байты, сырой ответ (для fallback),
    // накопленный полный текст, причина блокировки промпта.
    QByteArray m_streamBuffer;
    QByteArray m_rawBody;
    QString m_streamText;
    QString m_blockReason;
    bool m_streamStarted = false;

    void consumeStreamBytes(const QByteArray &bytes, bool flush);
    void handleStreamLine(const QByteArray &line);
    void handleStreamPayload(const QByteArray &payload);
    void resetStreamState();

    QString languageCodeToName(const QString &code) const;
};

#endif // GEMINICLIENT_H
