#ifndef MEMORYORCHESTRATOR_H
#define MEMORYORCHESTRATOR_H

#include <QObject>
#include <QDateTime>
#include <QStringList>
#include <QHash>
#include <QJsonArray>

class LongTermMemory;
class MemoryManager;
class ChatManager;
class ApiConfigManager;
class QNetworkAccessManager;

class MemoryOrchestrator : public QObject
{
    Q_OBJECT
public:
    explicit MemoryOrchestrator(LongTermMemory &longTerm,
                                MemoryManager &memoryManager,
                                ChatManager &chatManager,
                                ApiConfigManager &config,
                                QNetworkAccessManager &nam,
                                QObject *parent = nullptr);

    void checkAndRunBackgroundMemoryTasks();
    void maybeRunFactExtraction();
    void backfillLongTermMemory(const QDateTime &now, int maxChats = 1);
    void extractFactsFromChunk(const QString &chunk, const QString &chatId,
                               const QDateTime &now, const QStringList &msgIds);
    QStringList extractFactsList(const QString &raw) const;

signals:
    void errorOccurred(const QString &message);

private:
    LongTermMemory &m_longTerm;
    MemoryManager &m_memoryManager;
    ChatManager &m_chatManager;
    ApiConfigManager &m_config;
    QNetworkAccessManager &m_nam;

    int m_messagesSinceExtraction = 0;
    bool m_backfillDone = false;
    int m_backfillIndex = 0;
    QHash<QString, int> m_scannedChatMsgs;
    QDateTime m_lastExtractionRun;
};

#endif // MEMORYORCHESTRATOR_H