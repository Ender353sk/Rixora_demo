#include "textutils.h"
#include <QRegularExpression>
#include <QLatin1String>
#include <QLatin1Char>

QStringList TextUtils::splitIntoTtsChunks(const QString &text,
                                          int maxChunkChars,
                                          int minChunkChars)
{
    QString cleaned = text;

    // Markdown-ссылки [текст](url) -> просто текст
    cleaned.replace(QRegularExpression(QLatin1String("!?\\[[^\\]]*\\]\\([^)]*\\)")),
                    QLatin1String(" "));

    // Любые переносы строк заменяем пробелами
    cleaned.replace(QLatin1Char('\r'), QLatin1Char(' '));
    cleaned.replace(QLatin1Char('\n'), QLatin1Char(' '));

    // Непроизносимые символы разметки
    cleaned.remove(QLatin1Char('*'));
    cleaned.remove(QLatin1Char('_'));
    cleaned.remove(QLatin1Char('#'));
    cleaned.remove(QLatin1Char('`'));
    cleaned.remove(QLatin1Char('>'));

    cleaned = cleaned.simplified();
    if (cleaned.isEmpty()) return QStringList();

    // 1) Режем на предложения по знакам . ! ? … (с учётом "..." и "?!")
    QStringList sentences;
    {
        int pos = 0;
        const int n = cleaned.size();
        while (pos < n) {
            int end = -1;
            for (int i = pos; i < n; ++i) {
                const QChar c = cleaned.at(i);
                if (!isSentenceEnd(c)) continue;

                int j = i;
                while (j < n) {
                    const QChar q = cleaned.at(j);
                    if (!isSentenceEnd(q)) break;
                    ++j;
                }

                if (j >= n) { end = n; break; }
                if (!cleaned.at(j).isSpace()) { i = j - 1; continue; }

                int k = j;
                while (k < n && cleaned.at(k).isSpace()) ++k;

                // "1. Пункт" — точка после одних лишь цифр это маркер списка,
                // а не конец предложения (иначе получается обрывок "1").
                if (j - i == 1 && cleaned.at(i) == QLatin1Char('.') &&
                    isAllDigits(cleaned.mid(pos, i - pos).trimmed())) {
                    i = j - 1;
                    continue;
                }

                end = k;
                break;
            }

            if (end < 0) {
                const QString rest = cleaned.mid(pos).trimmed();
                if (!rest.isEmpty()) sentences << rest;
                break;
            }

            const QString piece = cleaned.mid(pos, end - pos).trimmed();
            if (!piece.isEmpty()) sentences << piece;
            pos = end;
        }
    }

    // 2) Склеиваем слишком короткие обрывки ("г.", "т.д.", "1.") со следующим
    QStringList merged;
    for (const QString &s : sentences) {
        if (merged.isEmpty() || merged.last().size() >= minChunkChars) {
            merged << s;
        } else {
            merged.last() += QLatin1Char(' ');
            merged.last() += s;
        }
    }

    // 3) Упаковываем в куски комфортного для модели размера
    QStringList chunks;
    QString buffer;
    for (const QString &s : merged) {
        if (s.size() > maxChunkChars) {
            // Аномально длинное предложение — режем по границам слов.
            if (!buffer.isEmpty()) {
                chunks << stripLeadingNoise(buffer);
                buffer.clear();
            }
            QString part = s;
            while (part.size() > maxChunkChars) {
                int cut = part.lastIndexOf(QLatin1Char(' '), maxChunkChars);
                if (cut <= 0) cut = maxChunkChars;
                chunks << stripLeadingNoise(part.left(cut));
                part = part.mid(cut).simplified();
            }
            buffer = part;
            continue;
        }

        if (buffer.isEmpty()) {
            buffer = s;
        } else if (buffer.size() + 1 + s.size() <= maxChunkChars) {
            buffer += QLatin1Char(' ');
            buffer += s;
        } else {
            chunks << stripLeadingNoise(buffer);
            buffer = s;
        }
    }
    if (!buffer.isEmpty()) chunks << stripLeadingNoise(buffer);

    return chunks;
}

bool TextUtils::isAllDigits(const QString &s)
{
    if (s.isEmpty()) return false;
    for (const QChar &c : s) {
        if (!c.isDigit()) return false;
    }
    return true;
}

QString TextUtils::stripLeadingNoise(const QString &s)
{
    static const QString noise = QStringLiteral(".,;:!?…\"'«»“”‘’()[]{}•-–—");
    int i = 0;
    const int n = s.size();
    while (i < n && noise.contains(s.at(i))) ++i;
    return s.mid(i).simplified();
}

bool TextUtils::isSentenceEnd(const QChar &c)
{
    return c == QLatin1Char('.') || c == QLatin1Char('!') ||
           c == QLatin1Char('?') || c.unicode() == 0x2026;  // …
}