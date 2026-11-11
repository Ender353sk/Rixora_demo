#include "longtermmemorypersistence.h"
#include "longtermmemorycore.h"

#include <QDataStream>
#include <QSaveFile>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <algorithm>

namespace {

quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (char ch : data) {
        crc ^= static_cast<unsigned char>(ch);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

bool writeBlob(const QString &path, quint32 magic, quint32 format, const QByteArray &payload)
{
    QByteArray blob;
    QDataStream out(&blob, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);
    out << magic << format << quint32(payload.size());
    out.writeRawData(payload.constData(), payload.size());
    out << crc32(payload);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(blob);
    return file.commit();
}

bool readBlob(const QString &path, quint32 magic, quint32 format, QByteArray &payload)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QByteArray blob = file.readAll();
    file.close();

    constexpr qint64 headerSize = 12;
    if (blob.size() < headerSize + 4) return false;

    QDataStream in(blob);
    in.setVersion(QDataStream::Qt_6_0);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 fileMagic = 0, fileFormat = 0, payloadSize = 0;
    in >> fileMagic >> fileFormat >> payloadSize;
    if (in.status() != QDataStream::Ok) return false;
    if (fileMagic != magic || fileFormat != format) return false;
    if (qint64(headerSize) + qint64(payloadSize) + 4 != blob.size()) return false;

    payload = blob.mid(headerSize, int(payloadSize));

    QDataStream crcIn(blob.mid(headerSize + int(payloadSize), 4));
    crcIn.setVersion(QDataStream::Qt_6_0);
    crcIn.setByteOrder(QDataStream::LittleEndian);
    quint32 storedCrc = 0;
    crcIn >> storedCrc;
    if (crcIn.status() != QDataStream::Ok) return false;

    return crc32(payload) == storedCrc;
}

QHash<QString, QVector<float>> readVectorSnapshot(const QString &path,
                                                  const QHash<QString, int> &vecIndexById)
{
    QHash<QString, QVector<float>> out;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return out;

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_0);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    quint32 magic = 0, format = 0;
    qint32 count = 0, dim = 0;
    in >> magic >> format >> count >> dim;
    if (magic != 0x52565831u || format != 1u
        || count < 0 || dim <= 0 || count > 100000 || dim > 100000) {
        file.close();
        return out;
    }

    QVector<float> flat(qint64(count) * dim);
    for (int i = 0; i < flat.size(); ++i) {
        float value = 0.0f;
        in >> value;
        flat[i] = value;
    }
    if (in.status() != QDataStream::Ok) {
        file.close();
        return out;
    }
    file.close();

    for (auto it = vecIndexById.constBegin(); it != vecIndexById.constEnd(); ++it) {
        const int slot = it.value();
        if (slot < 0 || slot >= count) continue;
        QVector<float> vec(dim);
        for (int d = 0; d < dim; ++d) {
            vec[d] = flat[qint64(slot) * dim + d];
        }
        out.insert(it.key(), vec);
    }
    return out;
}

} // namespace

LongTermMemoryPersistence::LongTermMemoryPersistence(QObject *parent)
    : QObject(parent)
{
    ensureStorageDir();
}

void LongTermMemoryPersistence::ensureStorageDir() const
{
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataPath.isEmpty()) return;

    const QString dir = dataPath + "/memory";
    if (!QDir().exists(dir)) {
        QDir().mkpath(dir);
    }
}

QString LongTermMemoryPersistence::storeFilePath() const
{
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataPath + "/memory/rixora_memory.bin";
}

QString LongTermMemoryPersistence::stateFilePath() const
{
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataPath + "/memory/rixora_memory_state.bin";
}

void LongTermMemoryPersistence::load(LongTermMemoryCore &core) const
{
    core.reset();

    if (loadStore(core)) {
        loadState(core);
        return;
    }

    if (!readJsonSnapshot(core)) {
        loadState(core);
        return;
    }

    save(core);
    saveState(core);
}

void LongTermMemoryPersistence::save(const LongTermMemoryCore &core) const
{
    ensureStorageDir();

    QList<int> orderedSlots;
    orderedSlots.reserve(core.aliveCount());
    for (int slot = 0; slot < core.slotCount(); ++slot) {
        if (core.isAlive(slot)) orderedSlots.append(slot);
    }
    std::sort(orderedSlots.begin(), orderedSlots.end(), [&core](int a, int b) {
        return core.seqAt(a) < core.seqAt(b);
    });

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setFloatingPointPrecision(QDataStream::SinglePrecision);

    out << quint32(orderedSlots.size());
    for (int slot : orderedSlots) {
        out << core.textAt(slot)
            << qint64(core.tsAt(slot))
            << core.chatIdAt(slot)
            << core.msgIdAt(slot);

        int dim = 0;
        const float *vec = core.embeddingData(slot, &dim);
        const quint32 storedDim = (vec && dim > 0) ? quint32(dim) : 0u;
        out << storedDim;
        if (storedDim > 0) {
            for (int d = 0; d < dim; ++d) out << vec[d];
        }
    }

    if (!writeBlob(storeFilePath(), STORE_MAGIC, STORE_FORMAT, payload)) {
        qDebug() << "[LongTermMemoryPersistence] Не удалось сохранить хранилище:"
                 << storeFilePath();
    }
}

void LongTermMemoryPersistence::saveState(const LongTermMemoryCore &core) const
{
    ensureStorageDir();

    const QHash<QString, int> scanned = core.scannedChatMsgsValue();

    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    out.setByteOrder(QDataStream::LittleEndian);

    out << core.day()
        << qint32(core.extractionsToday())
        << qint32(core.backfillIndexValue())
        << quint32(scanned.size());

    for (auto it = scanned.constBegin(); it != scanned.constEnd(); ++it) {
        out << it.key() << qint32(it.value());
    }

    if (!writeBlob(stateFilePath(), STATE_MAGIC, STATE_FORMAT, payload)) {
        qDebug() << "[LongTermMemoryPersistence] Не удалось сохранить состояние:"
                 << stateFilePath();
    }
}

bool LongTermMemoryPersistence::loadStore(LongTermMemoryCore &core) const
{
    QByteArray payload;
    if (!readBlob(storeFilePath(), STORE_MAGIC, STORE_FORMAT, payload)) return false;

    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    struct StoredFact {
        QString text;
        qint64 ts = 0;
        QString chatId;
        QString msgId;
        QVector<float> embedding;
    };

    quint32 count = 0;
    in >> count;
    if (in.status() != QDataStream::Ok || count > 100000) return false;

    QVector<StoredFact> facts;
    facts.reserve(int(qMin<quint32>(count, 100000u)));

    for (quint32 i = 0; i < count; ++i) {
        StoredFact fact;
        quint32 dim = 0;
        in >> fact.text >> fact.ts >> fact.chatId >> fact.msgId >> dim;
        if (in.status() != QDataStream::Ok || dim > 100000) return false;

        fact.embedding.resize(int(dim));
        for (quint32 d = 0; d < dim; ++d) {
            float value = 0.0f;
            in >> value;
            fact.embedding[int(d)] = value;
        }
        if (in.status() != QDataStream::Ok) return false;
        facts.append(fact);
    }

    for (const StoredFact &fact : facts) {
        core.restoreFact(fact.text, fact.embedding, fact.ts, fact.chatId, fact.msgId);
    }
    return true;
}

bool LongTermMemoryPersistence::loadState(LongTermMemoryCore &core) const
{
    QByteArray payload;
    if (!readBlob(stateFilePath(), STATE_MAGIC, STATE_FORMAT, payload)) return false;

    QDataStream in(payload);
    in.setVersion(QDataStream::Qt_6_0);
    in.setByteOrder(QDataStream::LittleEndian);

    QString day;
    qint32 extractions = 0, backfillIndex = 0;
    quint32 scannedCount = 0;
    in >> day >> extractions >> backfillIndex >> scannedCount;
    if (in.status() != QDataStream::Ok || scannedCount > 100000) return false;

    QHash<QString, int> scanned;
    for (quint32 i = 0; i < scannedCount; ++i) {
        QString chatId;
        qint32 value = 0;
        in >> chatId >> value;
        if (in.status() != QDataStream::Ok) return false;
        scanned.insert(chatId, value);
    }

    core.setDay(day);
    core.setExtractionsToday(extractions);
    core.setBackfillIndex(backfillIndex);
    core.setScannedChatMsgs(scanned);
    return true;
}

bool LongTermMemoryPersistence::readJsonSnapshot(LongTermMemoryCore &core) const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                         + "/memory";

    QFile metaFile(base + "/bot_memory_meta.json");
    bool metaRead = false;
    if (metaFile.open(QIODevice::ReadOnly)) {
        const QJsonObject meta = QJsonDocument::fromJson(metaFile.readAll()).object();
        metaFile.close();
        metaRead = true;

        core.setDay(meta["day"].toString());
        core.setExtractionsToday(meta["extractions"].toInt());
        core.setBackfillIndex(meta["backfillIndex"].toInt());

        QHash<QString, int> scanned;
        const QJsonObject scannedObj = meta["scannedChatMsgs"].toObject();
        for (auto it = scannedObj.constBegin(); it != scannedObj.constEnd(); ++it) {
            scanned.insert(it.key(), it.value().toInt());
        }
        core.setScannedChatMsgs(scanned);
    }

    QFile file(base + "/bot_memory.json");
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    const int version = root["version"].toInt();
    if (!metaRead) {
        core.setDay(root["day"].toString());
        core.setExtractionsToday(root["extractions"].toInt());
    }

    struct SnapshotFact {
        QString id;
        QString text;
        qint64 ts = 0;
        QString chatId;
        QString msgId;
        QVector<float> embedding;
    };

    QVector<SnapshotFact> facts;
    QHash<QString, int> vecIndexById;

    const QJsonArray arr = root["items"].toArray();
    facts.reserve(arr.size());

    for (const QJsonValue &value : arr) {
        const QJsonObject object = value.toObject();
        if (version < 4 && object["kind"].toString() != "fact") continue;

        SnapshotFact fact;
        fact.id = object["id"].toString();
        fact.text = object["text"].toString();
        if (fact.text.trimmed().isEmpty()) continue;
        fact.ts = qint64(object["ts"].toDouble());
        fact.chatId = object["chatId"].toString();
        fact.msgId = object["msgId"].toString();

        if (version < 6) {
            const QJsonArray embedding = object["embedding"].toArray();
            fact.embedding.reserve(embedding.size());
            for (const QJsonValue &component : embedding) {
                fact.embedding.append(float(component.toDouble()));
            }
        } else {
            const int vecIndex = object["vecIndex"].toInt(-1);
            if (vecIndex >= 0) vecIndexById.insert(fact.id, vecIndex);
        }

        facts.append(fact);
    }

    if (!vecIndexById.isEmpty()) {
        const QHash<QString, QVector<float>> vectors =
            readVectorSnapshot(base + "/bot_memory_vectors.bin", vecIndexById);
        for (SnapshotFact &fact : facts) {
            if (!fact.embedding.isEmpty()) continue;
            const auto it = vectors.constFind(fact.id);
            if (it != vectors.constEnd()) fact.embedding = it.value();
        }
    }

    for (const SnapshotFact &fact : facts) {
        core.restoreFact(fact.text, fact.embedding, fact.ts, fact.chatId, fact.msgId);
    }

    qDebug() << "[LongTermMemoryPersistence] Загружена память: фактов =" << core.count();
    return true;
}
