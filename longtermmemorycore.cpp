#include "longtermmemorycore.h"
#include "constants.h"

#include <QDateTime>
#include <QDate>
#include <QtMath>
#include <QDebug>

namespace {

float projectionValue(int bit, int dim)
{
    quint64 x = quint64(bit + 1) * 0x9E3779B97F4A7C15ULL
              + quint64(dim + 1) * 0xD1B54A32D192ED03ULL;
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    const double u = double(x >> 11) / double(1ULL << 53);
    return float(u * 2.0 - 1.0);
}

} // namespace

LongTermMemoryCore::LongTermMemoryCore(BM25Index &index)
    : m_index(index)
{
}

int LongTermMemoryCore::intern(const QString &value, QHash<QString, int> &map,
                               QVector<QString> &pool)
{
    if (value.isEmpty()) return -1;
    const auto it = map.constFind(value);
    if (it != map.constEnd()) return it.value();

    const int key = pool.size();
    pool.append(value);
    map.insert(value, key);
    return key;
}

const QString &LongTermMemoryCore::interned(int key, const QVector<QString> &pool)
{
    static const QString empty;
    if (key < 0 || key >= pool.size()) return empty;
    return pool.at(key);
}

QString LongTermMemoryCore::textKey(const QString &text)
{
    return text.trimmed().toCaseFolded();
}

float LongTermMemoryCore::cosineSimilarity(const float *a, const float *b, int dim)
{
    if (!a || !b || dim <= 0) return 0.0f;

    double dot = 0.0, normA = 0.0, normB = 0.0;
    for (int i = 0; i < dim; ++i) {
        dot += double(a[i]) * double(b[i]);
        normA += double(a[i]) * double(a[i]);
        normB += double(b[i]) * double(b[i]);
    }
    if (normA == 0.0 || normB == 0.0) return 0.0f;
    return float(dot / (qSqrt(normA) * qSqrt(normB)));
}

quint16 LongTermMemoryCore::signatureOf(const QVector<float> &v)
{
    if (v.isEmpty()) return 0;

    quint16 signature = 0;
    for (int bit = 0; bit < 16; ++bit) {
        double dot = 0.0;
        for (int d = 0; d < v.size(); ++d) {
            dot += double(v[d]) * double(projectionValue(bit, d));
        }
        if (dot >= 0.0) signature |= quint16(1u << bit);
    }
    return signature;
}

int LongTermMemoryCore::signatureDistance(quint16 a, quint16 b)
{
    return qPopulationCount(static_cast<quint32>(a ^ b));
}

int LongTermMemoryCore::allocateSlot()
{
    if (!m_freeSlots.isEmpty()) {
        const int slot = m_freeSlots.takeLast();
        m_entries[slot] = Entry();
        return slot;
    }
    m_entries.append(Entry());
    return m_entries.size() - 1;
}

void LongTermMemoryCore::releaseSlot(int slot)
{
    if (slot < 0 || slot >= m_entries.size()) return;

    Entry &e = m_entries[slot];
    if (!e.alive) return;

    const QString key = textKey(e.text);
    auto textIt = m_textToSlot.find(key);
    if (textIt != m_textToSlot.end() && textIt.value() == slot) {
        m_textToSlot.erase(textIt);
    }

    m_index.removeDocument(slot);
    m_deadFloats += e.vecDim;
    e.alive = false;
    e.text.clear();
    e.chatKey = -1;
    e.msgKey = -1;
    e.signature = 0;
    m_freeSlots.append(slot);
    --m_alive;

    compactArenaIfNeeded();
}

void LongTermMemoryCore::compactArenaIfNeeded()
{
    if (m_deadFloats <= 0) return;

    const qint64 threshold = qMax<qint64>(4096, m_arena.size() / 2);
    if (m_deadFloats < threshold) return;

    QVector<float> compacted;
    compacted.reserve(m_arena.size() - m_deadFloats);

    for (Entry &e : m_entries) {
        if (!e.alive || e.vecDim <= 0) continue;
        const int oldOffset = e.vecOffset;
        e.vecOffset = compacted.size();
        for (int d = 0; d < e.vecDim; ++d) {
            compacted.append(m_arena.at(oldOffset + d));
        }
    }

    m_arena = compacted;
    m_deadFloats = 0;
}

void LongTermMemoryCore::evictOldest()
{
    int victim = -1;
    qint64 victimTs = 0;
    quint32 victimSeq = 0;

    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry &e = m_entries.at(i);
        if (!e.alive) continue;
        if (victim < 0 || e.ts < victimTs || (e.ts == victimTs && e.seq < victimSeq)) {
            victim = i;
            victimTs = e.ts;
            victimSeq = e.seq;
        }
    }

    if (victim >= 0) releaseSlot(victim);
}

bool LongTermMemoryCore::addFactInner(const QString &text, const QVector<float> &embedding,
                                      const QString &chatId, const QString &msgId)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.size() < 8) return false;

    const QString key = textKey(trimmed);
    if (m_textToSlot.contains(key)) return false;

    if (!embedding.isEmpty()) {
        for (const Entry &e : m_entries) {
            if (!e.alive || e.vecDim != embedding.size()) continue;
            const float sim = cosineSimilarity(embedding.constData(),
                                               m_arena.constData() + e.vecOffset,
                                               e.vecDim);
            if (sim > DEDUP_SIMILARITY_THRESHOLD) {
                qDebug() << "[LongTermMemoryCore] Дубликат пропущен:" << trimmed.left(40);
                return false;
            }
        }
    }

    if (m_alive >= MAX_TOTAL) evictOldest();

    const int slot = allocateSlot();
    Entry &e = m_entries[slot];
    e.text = trimmed;
    e.ts = QDateTime::currentMSecsSinceEpoch();
    e.chatKey = intern(chatId, m_chatIds, m_chatPool);
    e.msgKey = intern(msgId, m_msgIds, m_msgPool);
    e.seq = ++m_seqCounter;
    e.alive = true;

    if (!embedding.isEmpty()) {
        e.vecOffset = m_arena.size();
        e.vecDim = embedding.size();
        for (float value : embedding) m_arena.append(value);
        e.signature = signatureOf(embedding);
    } else {
        e.vecOffset = -1;
        e.vecDim = 0;
        e.signature = 0;
    }

    m_textToSlot.insert(key, slot);
    m_index.addDocument(slot, trimmed);
    ++m_alive;
    return true;
}

bool LongTermMemoryCore::addFact(const QString &text, const QVector<float> &embedding,
                                 const QString &chatId, const QString &msgId)
{
    ensureLoaded();
    return addFactInner(text, embedding, chatId, msgId);
}

void LongTermMemoryCore::deleteByChatId(const QString &chatId)
{
    ensureLoaded();
    if (chatId.isEmpty()) return;

    const int before = m_alive;
    for (int slot = 0; slot < m_entries.size(); ++slot) {
        if (!m_entries.at(slot).alive) continue;
        if (chatIdAt(slot) == chatId) releaseSlot(slot);
    }

    if (m_alive == before) return;
    qDebug() << "[LongTermMemoryCore] Память чата удалена: chatId=" << chatId
             << ", осталось фактов:" << m_alive;
}

void LongTermMemoryCore::deleteByMsgId(const QString &chatId, const QString &msgId)
{
    ensureLoaded();
    if (chatId.isEmpty() || msgId.isEmpty()) return;

    const int before = m_alive;
    for (int slot = 0; slot < m_entries.size(); ++slot) {
        if (!m_entries.at(slot).alive) continue;
        if (chatIdAt(slot) == chatId && msgIdAt(slot) == msgId) releaseSlot(slot);
    }

    if (m_alive == before) return;
    qDebug() << "[LongTermMemoryCore] Факты сообщения удалены: chatId=" << chatId
             << ", msgId=" << msgId << ", удалено:" << (before - m_alive);
}

void LongTermMemoryCore::deleteAllMemory()
{
    ensureLoaded();
    const bool hadFacts = m_alive > 0;
    clearFacts();
    if (hadFacts) {
        qDebug() << "[LongTermMemoryCore] Вся долговременная память очищена.";
    }
}

bool LongTermMemoryCore::hasMemory() const
{
    ensureLoaded();
    return m_alive > 0;
}

int LongTermMemoryCore::count() const
{
    ensureLoaded();
    return m_alive;
}

void LongTermMemoryCore::ensureLoaded() const
{
    // Загрузка выполняется вызывающим кодом через хранилище.
}

void LongTermMemoryCore::clearFacts()
{
    m_entries.clear();
    m_freeSlots.clear();
    m_arena.clear();
    m_deadFloats = 0;
    m_alive = 0;
    m_seqCounter = 0;
    m_textToSlot.clear();
    m_chatIds.clear();
    m_chatPool.clear();
    m_msgIds.clear();
    m_msgPool.clear();
    m_index.clear();
}

void LongTermMemoryCore::reset()
{
    clearFacts();
    m_loaded = false;
    m_day.clear();
    m_extractionsToday = 0;
    m_backfillIndex = 0;
    m_scannedChatMsgs.clear();
}

void LongTermMemoryCore::restoreFact(const QString &text, const QVector<float> &embedding,
                                     qint64 ts, const QString &chatId, const QString &msgId)
{
    if (m_alive >= MAX_TOTAL) return;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return;

    const int slot = allocateSlot();
    Entry &e = m_entries[slot];
    e.text = trimmed;
    e.ts = ts;
    e.chatKey = intern(chatId, m_chatIds, m_chatPool);
    e.msgKey = intern(msgId, m_msgIds, m_msgPool);
    e.seq = ++m_seqCounter;
    e.alive = true;

    if (!embedding.isEmpty()) {
        e.vecOffset = m_arena.size();
        e.vecDim = embedding.size();
        for (float value : embedding) m_arena.append(value);
        e.signature = signatureOf(embedding);
    } else {
        e.vecOffset = -1;
        e.vecDim = 0;
        e.signature = 0;
    }

    const QString key = textKey(trimmed);
    if (!m_textToSlot.contains(key)) {
        m_textToSlot.insert(key, slot);
    }

    m_index.addDocument(slot, trimmed);
    ++m_alive;
}

void LongTermMemoryCore::updateDayIfNeeded()
{
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    if (m_day != today) {
        m_day = today;
        m_extractionsToday = 0;
    }
}

bool LongTermMemoryCore::canRunExtraction()
{
    ensureLoaded();
    updateDayIfNeeded();
    return m_extractionsToday < MAX_EXTRACTIONS_PER_DAY;
}

void LongTermMemoryCore::noteExtractionRun()
{
    ensureLoaded();
    updateDayIfNeeded();
    ++m_extractionsToday;
}

int LongTermMemoryCore::backfillIndex()
{
    ensureLoaded();
    return m_backfillIndex;
}

QHash<QString, int> LongTermMemoryCore::scannedChatMsgs()
{
    ensureLoaded();
    return m_scannedChatMsgs;
}

void LongTermMemoryCore::saveBackfillCursor(int index, const QHash<QString, int> &scanned)
{
    ensureLoaded();
    m_backfillIndex = index;
    m_scannedChatMsgs = scanned;
}

int LongTermMemoryCore::slotCount() const
{
    return m_entries.size();
}

bool LongTermMemoryCore::isAlive(int slot) const
{
    return slot >= 0 && slot < m_entries.size() && m_entries.at(slot).alive;
}

int LongTermMemoryCore::aliveCount() const
{
    return m_alive;
}

QList<int> LongTermMemoryCore::aliveSlots() const
{
    QList<int> result;
    result.reserve(m_alive);
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).alive) result.append(i);
    }
    return result;
}

const QString &LongTermMemoryCore::textAt(int slot) const
{
    static const QString empty;
    if (slot < 0 || slot >= m_entries.size()) return empty;
    return m_entries.at(slot).text;
}

qint64 LongTermMemoryCore::tsAt(int slot) const
{
    if (slot < 0 || slot >= m_entries.size()) return 0;
    return m_entries.at(slot).ts;
}

quint32 LongTermMemoryCore::seqAt(int slot) const
{
    if (slot < 0 || slot >= m_entries.size()) return 0;
    return m_entries.at(slot).seq;
}

QString LongTermMemoryCore::chatIdAt(int slot) const
{
    if (slot < 0 || slot >= m_entries.size()) return QString();
    return interned(m_entries.at(slot).chatKey, m_chatPool);
}

QString LongTermMemoryCore::msgIdAt(int slot) const
{
    if (slot < 0 || slot >= m_entries.size()) return QString();
    return interned(m_entries.at(slot).msgKey, m_msgPool);
}

const float *LongTermMemoryCore::embeddingData(int slot, int *dim) const
{
    if (dim) *dim = 0;
    if (slot < 0 || slot >= m_entries.size()) return nullptr;

    const Entry &e = m_entries.at(slot);
    if (!e.alive || e.vecDim <= 0 || e.vecOffset < 0) return nullptr;

    if (dim) *dim = e.vecDim;
    return m_arena.constData() + e.vecOffset;
}

quint16 LongTermMemoryCore::signatureAt(int slot) const
{
    if (slot < 0 || slot >= m_entries.size()) return 0;
    return m_entries.at(slot).signature;
}

bool LongTermMemoryCore::loaded() const
{
    return m_loaded;
}

void LongTermMemoryCore::setLoaded(bool loaded)
{
    m_loaded = loaded;
}

QString LongTermMemoryCore::day() const
{
    return m_day;
}

void LongTermMemoryCore::setDay(const QString &day)
{
    m_day = day;
}

int LongTermMemoryCore::extractionsToday() const
{
    return m_extractionsToday;
}

void LongTermMemoryCore::setExtractionsToday(int v)
{
    m_extractionsToday = v;
}

int LongTermMemoryCore::backfillIndexValue() const
{
    return m_backfillIndex;
}

void LongTermMemoryCore::setBackfillIndex(int v)
{
    m_backfillIndex = v;
}

QHash<QString, int> LongTermMemoryCore::scannedChatMsgsValue() const
{
    return m_scannedChatMsgs;
}

void LongTermMemoryCore::setScannedChatMsgs(const QHash<QString, int> &v)
{
    m_scannedChatMsgs = v;
}
