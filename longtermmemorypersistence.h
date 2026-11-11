#ifndef LONGTERMMEMORYPERSISTENCE_H
#define LONGTERMMEMORYPERSISTENCE_H

#include <QObject>
#include <QString>

class LongTermMemoryCore;

// Бинарное хранилище долговременной памяти.
// Основной файл содержит факты и их векторы, отдельный компактный файл —
// счётчики извлечений и курсор бэкфилла. Оба файла защищены CRC32 и
// записываются атомарно через QSaveFile.
class LongTermMemoryPersistence : public QObject
{
    Q_OBJECT
public:
    explicit LongTermMemoryPersistence(QObject *parent = nullptr);

    QString storeFilePath() const;
    QString stateFilePath() const;
    void ensureStorageDir() const;

    void load(LongTermMemoryCore &core) const;
    void save(const LongTermMemoryCore &core) const;
    void saveState(const LongTermMemoryCore &core) const;

    static constexpr quint32 STORE_MAGIC = 0x52584D32;
    static constexpr quint32 STORE_FORMAT = 2;
    static constexpr quint32 STATE_MAGIC = 0x52585332;
    static constexpr quint32 STATE_FORMAT = 1;

private:
    bool loadStore(LongTermMemoryCore &core) const;
    bool loadState(LongTermMemoryCore &core) const;
    bool readJsonSnapshot(LongTermMemoryCore &core) const;
};

#endif // LONGTERMMEMORYPERSISTENCE_H
