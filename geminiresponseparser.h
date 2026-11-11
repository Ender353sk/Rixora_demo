#ifndef GEMINIRESPONSEPARSER_H
#define GEMINIRESPONSEPARSER_H

#include <QByteArray>
#include <QString>
#include <QVector>

// Парсинг ответа Gemini API generateContent.
// Возвращает true при успехе, false при ошибке/блокировке.
// При успехе: *out = текст ответа, *blockReason = пусто.
// При ошибке: *out = пусто, *blockReason = причина.
bool parseGeminiText(const QByteArray &payload, QString *out, QString *blockReason);

// Парсинг ответа эмбеддинга.
// Возвращает true при успехе, *embeddings заполняется векторами.
bool parseEmbeddingResponse(const QByteArray &payload, QVector<QVector<float>> *embeddings);

// Достаёт человекочитаемую причину из ошибочного ответа Gemini:
//   { "error": { "code": 404, "message": "...", "status": "NOT_FOUND" } }
// Без этого текста сетевой сбой неотличим от «пустого» ответа.
QString describeGeminiError(const QByteArray &payload);

#endif // GEMINIRESPONSEPARSER_H
