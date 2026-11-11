#ifndef TEXTUTILS_H
#define TEXTUTILS_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>

#include "constants.h"

// Утилиты для подготовки текста к TTS-синтезу.
class TextUtils
{
public:
    // Разбивает текст на куски комфортного для TTS размера:
    // - убирает Markdown-разметку
    // - схлопывает переносы строк
    // - режет по границам предложений
    // - склеивает короткие обрывки
    // - упаковывает в чанки заданного размера
    // Единый источник истины по длине куска — constants.h, а не литералы
    // здесь: раньше значения 200/25 дублировались в трёх местах, и параметр
    // minChunkChars молча игнорировался (в коде стояло жёсткое 25).
    static QStringList splitIntoTtsChunks(const QString &text,
                                          int maxChunkChars = TTS_MAX_CHUNK_CHARS,
                                          int minChunkChars = TTS_MIN_CHUNK_CHARS);

private:
    static bool isAllDigits(const QString &s);
    static QString stripLeadingNoise(const QString &s);
    static bool isSentenceEnd(const QChar &c);
};

#endif // TEXTUTILS_H