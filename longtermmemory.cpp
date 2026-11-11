#include "longtermmemory.h"
#include "longtermmemorypersistence.h"
#include "memorysearch.h"

#include <QDebug>

LongTermMemory::LongTermMemory(QObject *parent)
    : QObject(parent),
      m_index(),
      m_core(std::make_unique<LongTermMemoryCore>(m_index)),
      m_persistence(std::make_unique<LongTermMemoryPersistence>()),
      m_search(std::make_unique<MemorySearch>(m_index, *m_core, this))
{
}

LongTermMemory::~LongTermMemory() = default;

void LongTermMemory::ensureLoaded() const
{
    if (m_core->loaded()) return;

    m_persistence->load(*m_core);
    m_core->setLoaded(true);
}

bool LongTermMemory::addFact(const QString &text, const QVector<float> &embedding,
                             const QString &chatId, const QString &msgId)
{
    ensureLoaded();
    if (!m_core->addFact(text, embedding, chatId, msgId)) return false;

    m_persistence->save(*m_core);
    m_persistence->saveState(*m_core);

    qDebug() << "[LongTermMemory] Запомнен факт:" << text.trimmed().left(50);
    return true;
}

void LongTermMemory::addFacts(const QVector<QString> &texts,
                              const QVector<QVector<float>> &embeddings,
                              const QString &chatId, const QVector<QString> &msgIds,
                              int *addedCount)
{
    ensureLoaded();

    int added = 0;
    for (int i = 0; i < texts.size(); ++i) {
        const QVector<float> embedding = i < embeddings.size()
                                             ? embeddings.at(i)
                                             : QVector<float>();
        const QString msgId = i < msgIds.size() ? msgIds.at(i) : QString();
        if (m_core->addFactInner(texts.at(i), embedding, chatId, msgId)) ++added;
    }

    if (added > 0) {
        m_persistence->save(*m_core);
        m_persistence->saveState(*m_core);
    }

    if (addedCount) *addedCount = added;
}

QList<MemoryHit> LongTermMemory::queryLexical(const QString &queryText,
                                              int topK, float threshold) const
{
    ensureLoaded();
    return m_search->queryLexical(queryText, topK, threshold);
}

QList<MemoryHit> LongTermMemory::queryHybrid(const QString &queryText,
                                             const QVector<float> &queryEmbedding,
                                             int topK, float threshold,
                                             QList<MemoryHit> *lexicalOut) const
{
    ensureLoaded();
    return m_search->queryHybrid(queryText, queryEmbedding, topK, threshold, lexicalOut);
}

QList<MemoryHit> LongTermMemory::recentFacts(int n) const
{
    ensureLoaded();
    return m_search->recentFacts(n);
}

bool LongTermMemory::hasMemory() const
{
    ensureLoaded();
    return m_core->hasMemory();
}

int LongTermMemory::count() const
{
    ensureLoaded();
    return m_core->count();
}

void LongTermMemory::deleteByChatId(const QString &chatId)
{
    ensureLoaded();
    m_core->deleteByChatId(chatId);
    m_persistence->save(*m_core);
    m_persistence->saveState(*m_core);
}

void LongTermMemory::deleteByMsgId(const QString &chatId, const QString &msgId)
{
    ensureLoaded();
    m_core->deleteByMsgId(chatId, msgId);
    m_persistence->save(*m_core);
    m_persistence->saveState(*m_core);
}

void LongTermMemory::deleteAllMemory()
{
    ensureLoaded();
    m_core->deleteAllMemory();
    m_persistence->save(*m_core);
    m_persistence->saveState(*m_core);
}

bool LongTermMemory::canRunExtraction() const
{
    ensureLoaded();
    return m_core->canRunExtraction();
}

void LongTermMemory::noteExtractionRun()
{
    ensureLoaded();
    m_core->noteExtractionRun();
    m_persistence->saveState(*m_core);
}

int LongTermMemory::backfillIndex() const
{
    ensureLoaded();
    return m_core->backfillIndex();
}

QHash<QString, int> LongTermMemory::scannedChatMsgs() const
{
    ensureLoaded();
    return m_core->scannedChatMsgs();
}

void LongTermMemory::saveBackfillCursor(int index, const QHash<QString, int> &scanned)
{
    ensureLoaded();
    m_core->saveBackfillCursor(index, scanned);
    m_persistence->saveState(*m_core);
}
