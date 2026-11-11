#include "geminiresponseparser.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

bool parseGeminiText(const QByteArray &payload, QString *out, QString *blockReason)
{
    if (out) out->clear();
    if (blockReason) blockReason->clear();

    QJsonParseError parseError{};
    const QJsonObject root = QJsonDocument::fromJson(payload, &parseError).object();
    if (parseError.error != QJsonParseError::NoError) {
        if (blockReason) *blockReason = "Некорректный ответ сервиса";
        return false;
    }

    const QString blocked = root["promptFeedback"].toObject()["blockReason"].toString();
    if (!blocked.isEmpty()) {
        if (blockReason) *blockReason = blocked;
        return false;
    }

    const QJsonArray candidates = root["candidates"].toArray();
    if (candidates.isEmpty()) {
        if (blockReason) *blockReason = "Пустой ответ";
        return false;
    }

    const QJsonObject candidate = candidates.at(0).toObject();

    const QString finish = candidate["finishReason"].toString();
    if (!finish.isEmpty() && finish != "STOP" && finish != "MAX_TOKENS") {
        if (blockReason) *blockReason = finish;
        return false;
    }

    const QJsonArray parts = candidate["content"].toObject()["parts"].toArray();
    if (parts.isEmpty()) {
        if (blockReason) *blockReason = finish.isEmpty() ? QString("Пустой ответ") : finish;
        return false;
    }

    const QString text = parts.at(0).toObject()["text"].toString();
    if (text.trimmed().isEmpty()) {
        if (blockReason) *blockReason = finish.isEmpty() ? QString("Пустой ответ") : finish;
        return false;
    }

    if (out) *out = text;
    return true;
}

// Поддерживаются оба формата ответа:
//   embedContent        -> { "embedding":  { "values": [...] } }
//   batchEmbedContents  -> { "embeddings": [ { "values": [...] }, ... ] }
bool parseEmbeddingResponse(const QByteArray &payload, QVector<QVector<float>> *embeddings)
{
    if (embeddings) embeddings->clear();

    QJsonParseError parseError{};
    const QJsonObject root = QJsonDocument::fromJson(payload, &parseError).object();
    if (parseError.error != QJsonParseError::NoError) {
        return false;
    }

    QJsonArray embeddingsArray = root["embeddings"].toArray();
    if (embeddingsArray.isEmpty()) {
        const QJsonObject single = root["embedding"].toObject();
        if (single.isEmpty()) {
            return false;
        }
        embeddingsArray.append(single);
    }

    if (embeddings) {
        embeddings->reserve(embeddingsArray.size());
        // Через const-ссылку: неконстантный begin() у QJsonArray снимает
        // implicit sharing и делает detach на каждой итерации.
        const QJsonArray &batch = embeddingsArray;
        for (const QJsonValue &ev : batch) {
            QVector<float> v;
            // operator[] вызываем не на временном объекте, а на именованном.
            const QJsonObject obj = ev.toObject();
            const QJsonArray values = obj["values"].toArray();
            v.reserve(values.size());
            for (const QJsonValue &fv : values) v.append(static_cast<float>(fv.toDouble()));
            embeddings->append(std::move(v));
        }
    }
    return true;
}

QString describeGeminiError(const QByteArray &payload)
{
    QJsonParseError parseError{};
    const QJsonObject root = QJsonDocument::fromJson(payload, &parseError).object();
    const QJsonObject err = root["error"].toObject();
    if (err.isEmpty()) {
        const QString body = QString::fromUtf8(payload).simplified();
        return body.isEmpty() ? QString("empty response body") : body.left(300);
    }

    QString message = err["message"].toString();
    const int code = err["code"].toInt();
    if (message.isEmpty()) message = err["status"].toString();
    if (code > 0) return QString("HTTP %1: %2").arg(code).arg(message);
    return message;
}
