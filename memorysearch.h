#ifndef MEMORYSEARCH_H
#define MEMORYSEARCH_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QList>

#include "memorytypes.h"
#include "bm25index.h"

class LongTermMemoryCore;

// Гибридный поиск по долговременной памяти.
// Лексические кандидаты берутся из posting-таблиц, векторные — из короткого
// списка по сигнатурам; затем кандидаты переранжируются точным косинусом,
// свежестью и разнообразием (MMR).
class MemorySearch : public QObject
{
    Q_OBJECT
public:
    explicit MemorySearch(BM25Index &index, const LongTermMemoryCore &core,
                          QObject *parent = nullptr);

    QList<MemoryHit> queryLexical(const QString &queryText, int topK, float threshold) const;
    QList<MemoryHit> queryHybrid(const QString &queryText, const QVector<float> &queryEmbedding,
                                 int topK, float threshold, QList<MemoryHit> *lexicalOut) const;
    QList<MemoryHit> recentFacts(int n) const;

    static float cosineSimilarity(const QVector<float> &vecA, const QVector<float> &vecB);
    static double recencyScore(qint64 ts);

private:
    BM25Index &m_index;
    const LongTermMemoryCore &m_core;
};

#endif // MEMORYSEARCH_H
