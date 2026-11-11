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

class LongTermMemoryPersistence;
class MemorySearch;

// Глобальная (кросс-чатовая) долгосрочная память Риксоры.
//
// Память хранит сырые факты, каждый из которых жёстко привязан к chatId и
// сообщению-источнику msgId. Удаление чата или сообщения вырезает только
// относящиеся к ним факты, не затрагивая остальные.
//
// Рост памяти ограничен: дедупликация при добавлении, жёсткий потолок
// MAX_TOTAL фактов и дневной лимит API-вызовов на извлечение.
//
// Поиск гибридный: векторная близость, лексический индекс, свежесть и
// переранжировка с разнообразием (MMR).
class LongTermMemory : public QObject
{
    Q_OBJECT

public:
    explicit LongTermMemory(QObject *parent = nullptr);
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

private:
    BM25Index m_index;
    std::unique_ptr<LongTermMemoryCore> m_core;
    std::unique_ptr<LongTermMemoryPersistence> m_persistence;
    std::unique_ptr<MemorySearch> m_search;
};

#endif // LONGTERMMEMORY_H
