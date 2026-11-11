#include "geminiadapter.h"
#include <QJsonObject>

QJsonArray GeminiHistoryAdapter::normalizeHistoryForGemini(QJsonArray history) {
    if (history.isEmpty()) return history;

    // Gemini требует: contents должен начинаться с user, строго чередоваться и
    // заканчиваться user. История строится с user на чётных индексах, поэтому при
    // размере > maxMessages срез начинается с model (нечётный индекс) — такой запрос
    // Gemini отклоняет (400). Отбрасываем ведущую model-реплику.
    while (history.size() > 1
           && history.first().toObject()["role"].toString() == "model") {
        history.removeFirst();
    }
    if (history.first().toObject()["role"].toString() != "user") {
        return QJsonArray();
    }

    // После сетевой ошибки или «стопа» модель-ответ может не добавиться в историю,
    // и user-реплики окажутся подряд — Gemini такую последовательность тоже
    // отклоняет. Склеиваем соседние реплики одной роли в одну (несколько parts),
    // восстанавливая чередование без потери текста.
    QJsonArray normalized;
    for (const QJsonValue &val : qAsConst(history)) {
        const QJsonObject raw = val.toObject();
        // Во внутреннем хранилище у сообщений есть служебное поле "id" — его нельзя
        // отправлять на API: Gemini отклоняет незнакомые поля в content (ошибка 400).
        QJsonObject msg;
        msg["role"] = raw["role"];
        msg["parts"] = raw["parts"];
        if (!normalized.isEmpty()
            && normalized.last().toObject()["role"].toString() == msg["role"].toString()) {
            QJsonObject last = normalized.last().toObject();
            QJsonArray parts = last["parts"].toArray();
            const QJsonArray extraParts = msg["parts"].toArray();
            for (const QJsonValue &p : extraParts) parts.append(p);
            last["parts"] = parts;
            normalized[normalized.size() - 1] = last;
        } else {
            normalized.append(msg);
        }
    }

    // Страховка: последней репликой должен быть user (на момент вызова так и есть,
    // т.к. текущий промпт уже добавлен), хвостовые model-реплики не отдаём.
    while (!normalized.isEmpty()
           && normalized.last().toObject()["role"].toString() == "model") {
        normalized.removeLast();
    }

    return normalized;
}