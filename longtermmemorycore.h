#ifndef LONGTERMMEMORYCORE_H
#define LONGTERMMEMORYCORE_H

#include <QString>
#include <QVector>
#include <QList>
#include <QHash>
#include <QStringList>

#include "bm25index.h"

// Хранилище фактов долговременной памяти.
// Факты лежат в массиве слотов, векторы — в общей плоской арене. Свободные
// слоты переиспользуются, строки chatId/msgId интернируются, а для быстрого
// отбора векторных кандидатов хранится компактная сигнатура каждого вектора.
class LongTermMemoryCore
{
public:
    explicit LongTermMemoryCore(BM25Index &index);

    bool addFactInner(const QString &text, const QVector<float> &embedding,
                      const QString &chatId, const QString &msgId);
    bool addFact(const QString &text, const QVector<float> &embedding,
                 const QString &chatId, const QString &msgId);

    void deleteByChatId(const QString &chatId);
    void deleteByMsgId(const QString &chatId, const QString &msgId);
    void deleteAllMemory();

    bool hasMemory() const;
    int count() const;
    void ensureLoaded() const;

    void updateDayIfNeeded();
    bool canRunExtraction();
    void noteExtractionRun();
    int backfillIndex();
    QHash<QString, int> scannedChatMsgs();
    void saveBackfillCursor(int index, const QHash<QString, int> &scanned);

    static constexpr int MAX_TOTAL = 2000;
    static constexpr int MAX_EXTRACTIONS_PER_DAY = 80;

    // Состояние, сохраняемое отдельно от основного хранилища.
    bool loaded() const;
    void setLoaded(bool loaded);
    QString day() const;
    void setDay(const QString &day);
    int extractionsToday() const;
    void setExtractionsToday(int v);
    int backfillIndexValue() const;
    void setBackfillIndex(int v);
    QHash<QString, int> scannedChatMsgsValue() const;
    void setScannedChatMsgs(const QHash<QString, int> &v);

    // Доступ к слотам для сериализации и поиска.
    int slotCount() const;
    bool isAlive(int slot) const;
    int aliveCount() const;
    QList<int> aliveSlots() const;
    const QString &textAt(int slot) const;
    qint64 tsAt(int slot) const;
    quint32 seqAt(int slot) const;
    QString chatIdAt(int slot) const;
    QString msgIdAt(int slot) const;
    const float *embeddingData(int slot, int *dim) const;
    quint16 signatureAt(int slot) const;

    // Полный сброс фактов и состояния.
    void reset();

    // Восстанавливает факт из хранилища без дедупликации и без изменения
    // временной метки.
    void restoreFact(const QString &text, const QVector<float> &embedding,
                     qint64 ts, const QString &chatId, const QString &msgId);

    static float cosineSimilarity(const float *a, const float *b, int dim);
    static quint16 signatureOf(const QVector<float> &v);
    static int signatureDistance(quint16 a, quint16 b);

private:
    struct Entry {
        QString text;
        qint64 ts = 0;
        int chatKey = -1;
        int msgKey = -1;
        int vecOffset = -1;
        int vecDim = 0;
        quint16 signature = 0;
        quint32 seq = 0;
        bool alive = false;
    };

    int allocateSlot();
    void releaseSlot(int slot);
    void clearFacts();
    void evictOldest();
    void compactArenaIfNeeded();

    static int intern(const QString &value, QHash<QString, int> &map,
                      QVector<QString> &pool);
    static const QString &interned(int key, const QVector<QString> &pool);
    static QString textKey(const QString &text);

    BM25Index &m_index;
    QVector<Entry> m_entries;
    QVector<int> m_freeSlots;
    QVector<float> m_arena;
    qint64 m_deadFloats = 0;
    int m_alive = 0;
    quint32 m_seqCounter = 0;
    QHash<QString, int> m_textToSlot;
    QHash<QString, int> m_chatIds;
    QVector<QString> m_chatPool;
    QHash<QString, int> m_msgIds;
    QVector<QString> m_msgPool;
    bool m_loaded = false;
    QString m_day;
    int m_extractionsToday = 0;
    int m_backfillIndex = 0;
    QHash<QString, int> m_scannedChatMsgs;
};

#endif // LONGTERMMEMORYCORE_H
