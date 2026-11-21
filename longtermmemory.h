#ifndef LONGTERMMEMORY_H
#define LONGTERMMEMORY_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QList>
#include <QHash>
#include <memory>

#include "longtermmemorycore.h"
#include "memorytypes.h"

class MemorySearch;
class SqliteMemoryRepository;

// Глобальна (кросс-чатова) довгострокова пам'ять Риксори.
//
// Память хранит сирі факти, кожен з яких жорстко привязаний до chatId і
// повідомлення-джерела msgId. Видалення чату або повідомлення вириває лише
// відносяться до них факти, не зачікаючи решта.
//
// Зростання пам'яті обмежене: дедукція при додаванні, жорсткий потолок
// MAX_TOTAL фактів і денний ліміт API-викликів на вилучення.
//
// Пошук гібридний: векторна близость, лексичний індекс, свіжість і
// переранжування з різноманіттям (MMR).
//
// Персистентність тепер через SQLite (SqliteMemoryRepository), а не через
// двійкові файли LongTermMemoryPersistence.
class LongTermMemory : public QObject
{
    Q_OBJECT
public:
    explicit LongTermMemory(SqliteMemoryRepository *repository = nullptr, QObject *parent = nullptr);
    ~LongTermMemory() override;

    static constexpr int MAX_TOTAL = LongTermMemoryCore::MAX_TOTAL;

    void ensureLoaded() const;

    bool addFact(const QString &text, const QVector<float> &embedding,
                 const QString &chatId = QString(),
                 const QString &msgId = QString());

    void addFacts(const QVector<QString> &texts,
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

    bool hasMemory() const;
    int count() const;

    void deleteByChatId(const QString &chatId);
    void deleteByMsgId(const QString &chatId, const QString &msgId);
    void deleteAllMemory();

    bool canRunExtraction() const;
    void noteExtractionRun();

    int backfillIndex() const;
    QHash<QString, int> scannedChatMsgs() const;
    void saveBackfillCursor(int index, const QHash<QString, int> &scanned);

    SqliteMemoryRepository *repository() const { return m_repository; }

private:
    BM25Index m_index;
    std::unique_ptr<LongTermMemoryCore> m_core;
    std::unique_ptr<MemorySearch> m_search;
    SqliteMemoryRepository *m_repository;
    bool m_ownsRepository = false;
};

#endif // LONGTERMMEMORY_H
