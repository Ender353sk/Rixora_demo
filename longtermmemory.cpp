#include "longtermmemory.h"
#include "sqlitememoryrepository.h"
#include "memorysearch.h"

#include <QStandardPaths>
#include <QDebug>

LongTermMemory::LongTermMemory(SqliteMemoryRepository *repository, QObject *parent)
    : QObject(parent),
      m_index(),
      m_core(std::make_unique<LongTermMemoryCore>(m_index)),
      m_search(std::make_unique<MemorySearch>(m_index, *m_core, this)),
      m_repository(repository),
      m_ownsRepository(false)
{
    if (!m_repository) {
        m_repository = new SqliteMemoryRepository(this);
        m_ownsRepository = true;
        QStandardPaths::setTestModeEnabled(true);
        QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/test_ltm.db";
        m_repository->initialize(path);
    }
}

LongTermMemory::~LongTermMemory() = default;

void LongTermMemory::ensureLoaded() const
{
    if (m_core->loaded()) return;

    // Завантажуємо факти з SQLite в LongTermMemoryCore для пошуку
    // Зараз завантаження здійснюється лениво при першому доступі.
    // Повний перенос всіх фактів з БД в core відбувається за потреби.
    m_core->setLoaded(true);
}

bool LongTermMemory::addFact(const QString &text, const QVector<float> &embedding,
                             const QString &chatId, const QString &msgId)
{
    ensureLoaded();
    if (!m_core->addFact(text, embedding, chatId, msgId)) return false;

    // Персистентність через SQLite
    if (m_repository) {
        m_repository->addFact(text, embedding, chatId, msgId);
    }

    qDebug() << "[LongTermMemory] Запомнено факт:" << text.trimmed().left(50);
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
        if (m_repository) {
            m_repository->addFacts(texts, embeddings, chatId, msgIds, &added);
        }
    }

    if (addedCount) *addedCount = added;
}

QList<MemoryHit> LongTermMemory::queryLexical(const QString &queryText,
                                              int topK, float threshold) const
{
    ensureLoaded();
    if (m_repository) {
        return m_repository->queryLexical(queryText, topK, threshold);
    }
    return m_search->queryLexical(queryText, topK, threshold);
}

QList<MemoryHit> LongTermMemory::queryHybrid(const QString &queryText,
                                             const QVector<float> &queryEmbedding,
                                             int topK, float threshold,
                                             QList<MemoryHit> *lexicalOut) const
{
    ensureLoaded();
    if (m_repository) {
        return m_repository->queryHybrid(queryText, queryEmbedding, topK, threshold, lexicalOut);
    }
    return m_search->queryHybrid(queryText, queryEmbedding, topK, threshold, lexicalOut);
}

QList<MemoryHit> LongTermMemory::recentFacts(int n) const
{
    ensureLoaded();
    if (m_repository) {
        return m_repository->recentFacts(n);
    }
    return m_search->recentFacts(n);
}

bool LongTermMemory::hasMemory() const
{
    ensureLoaded();
    if (m_repository) {
        return m_repository->hasFacts();
    }
    return m_core->hasMemory();
}

int LongTermMemory::count() const
{
    ensureLoaded();
    if (m_repository) {
        return m_repository->factCount();
    }
    return m_core->count();
}

void LongTermMemory::deleteByChatId(const QString &chatId)
{
    ensureLoaded();
    m_core->deleteByChatId(chatId);
    if (m_repository) {
        m_repository->deleteFactsByChatId(chatId);
    }
}

void LongTermMemory::deleteByMsgId(const QString &chatId, const QString &msgId)
{
    ensureLoaded();
    m_core->deleteByMsgId(chatId, msgId);
    if (m_repository) {
        m_repository->deleteFactsByMsgId(chatId, msgId);
    }
}

void LongTermMemory::deleteAllMemory()
{
    ensureLoaded();
    m_core->deleteAllMemory();
    if (m_repository) {
        m_repository->deleteAllFacts();
    }
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
    if (m_repository) {
        m_repository->noteExtractionRun();
    }
}

int LongTermMemory::backfillIndex() const
{
    ensureLoaded();
    if (m_repository) {
        return m_repository->backfillIndex();
    }
    return m_core->backfillIndex();
}

QHash<QString, int> LongTermMemory::scannedChatMsgs() const
{
    ensureLoaded();
    if (m_repository) {
        return m_repository->scannedChatMsgs();
    }
    return m_core->scannedChatMsgs();
}

void LongTermMemory::saveBackfillCursor(int index, const QHash<QString, int> &scanned)
{
    ensureLoaded();
    m_core->saveBackfillCursor(index, scanned);
    if (m_repository) {
        m_repository->saveBackfillCursor(index, scanned);
    }
}
