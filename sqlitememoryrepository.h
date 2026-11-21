#ifndef SQLITEMEMORYREPOSITORY_H
#define SQLITEMEMORYREPOSITORY_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>
#include <QDateTime>
#include <QSqlDatabase>
#include <QUuid>

#include "memorytypes.h"

class SqliteMemoryRepository : public QObject
{
    Q_OBJECT
public:
    explicit SqliteMemoryRepository(QObject *parent = nullptr);
    ~SqliteMemoryRepository() override;

    // Ініціалізація БД: відкрити підключення, виконати DDL схему
    bool initialize(const QString &dbPath = QString());

    // ---------- Чати ----------
    bool upsertChat(const QString &chatId, const QString &title,
                    qint64 createdAt, qint64 updatedAt, int messageCount);
    bool deleteChat(const QString &chatId);
    bool deleteAllChats();
    QJsonArray getChatList() const;          // новішіші першими
    QJsonArray getChatListOldestFirst() const;
    bool chatExists(const QString &chatId) const;
    QString getCurrentChatId() const;
    void setCurrentChatId(const QString &chatId);

    // ---------- Повідомлення ----------
    bool saveMessages(const QString &chatId, const QJsonArray &messages);
    QJsonArray loadMessages(const QString &chatId) const;
    QJsonArray peekMessages(const QString &chatId, int lastN,
                            int skipFromEnd = 0) const;
    bool messageExists(const QString &msgId) const;

    // ---------- Середньострокова пам'ять (свідка) ----------
    bool saveChatMemory(const QString &chatId, const QString &midTermSummary,
                        int summarizedCount);
    QJsonObject loadChatMemory(const QString &chatId) const;

    // ---------- Довгострокові факти ----------
    bool addFact(const QString &text, const QVector<float> &embedding,
                 const QString &chatId, const QString &msgId);
    bool addFacts(const QVector<QString> &texts,
                  const QVector<QVector<float>> &embeddings,
                  const QString &chatId, const QVector<QString> &msgIds,
                  int *addedCount = nullptr);
    QList<MemoryHit> queryLexical(const QString &queryText, int topK = 8,
                                  float threshold = 0.22f) const;
    QList<MemoryHit> queryHybrid(const QString &queryText,
                                 const QVector<float> &queryEmbedding,
                                 int topK = 8, float threshold = 0.18f,
                                 QList<MemoryHit> *lexicalOut = nullptr) const;
    QList<MemoryHit> recentFacts(int n = 8) const;
    bool deleteFactsByChatId(const QString &chatId);
    bool deleteFactsByMsgId(const QString &chatId, const QString &msgId);
    bool deleteAllFacts();
    int factCount() const;
    bool hasFacts() const;

    // ---------- Екстракція / бэкфілл ----------
    bool canRunExtraction() const;
    void noteExtractionRun();
    int backfillIndex() const;
    QHash<QString, int> scannedChatMsgs() const;
    void saveBackfillCursor(int index, const QHash<QString, int> &scanned);

    // ---------- Аудит ----------
    void writeAudit(const QString &action, const QString &tableName,
                    const QString &recordId, const QJsonObject &oldValues,
                    const QJsonObject &newValues);

    // ---------- Резервне копіювання / відновлення ----------
    bool backupDatabase(const QString &backupPath) const;
    bool restoreDatabase(const QString &backupPath);

    QSqlDatabase database() const { return m_db; }

private:
    QSqlDatabase m_db;
    QString m_currentChatId;

    bool execDDL();
    bool ensureOpen() const;
    void closeDatabase();

    static QByteArray serializeVector(const QVector<float> &v);
    static QVector<float> deserializeVector(const QByteArray &blob);
};

#endif // SQLITEMEMORYREPOSITORY_H
