#include "texttokenizer.h"
#include <QSet>

// ---------- Стоп-слова ----------

bool TextTokenizer::isStopword(const QString &word) {
    static const QSet<QString> stop = {
        // Русский
        "и", "в", "во", "на", "не", "что", "с", "со", "по", "это", "за", "из", "от",
        "мы", "вы", "ты", "он", "она", "оно", "они", "как", "так", "при", "к", "ко",
        "до", "а", "но", "у", "же", "бы", "ли", "или", "то", "да", "все", "всё", "уже",
        "ещё", "еще", "этот", "эта", "это", "его", "её", "ее", "их", "ни", "без", "для",
        "чем", "где", "там", "тут", "был", "была", "были", "было", "если", "потому",
        "что", "меня", "мне", "тебе", "тебя", "себя", "нас", "вас", "про", "просто",
        "очень", "теперь", "сейчас", "тоже", "только", "можно", "надо", "нужно", "будет",
        "люблю", "знаешь", "знаете", "хочу", "хочешь", "когда", "тогда", "всегда", "здесь",
        "привет", "здравствуй", "пока", "спасибо", "пожалуйста", "аа", "мм", "хм",
        // Английский
        "the", "a", "an", "and", "or", "for", "to", "of", "in", "on", "with", "is",
        "are", "was", "were", "it", "i", "you", "he", "she", "they", "we", "my", "your",
        "this", "that", "what", "how", "not", "but", "as", "at", "be", "do", "does", "did",
        "have", "has", "had", "just", "like", "want", "know", "please", "me", "him", "her",
        "us", "them", "there", "here", "then", "when", "why", "who", "which", "also"
    };
    return stop.contains(word);
}

// ---------- CJK ----------

bool TextTokenizer::isCjk(const QChar &ch) {
    uint u = ch.unicode();
    return (u >= 0x3400 && u <= 0x4DBF)   // CJK Extension A
        || (u >= 0x4E00 && u <= 0x9FFF)   // CJK Unified Ideographs
        || (u >= 0x3040 && u <= 0x30FF)   // Hiragana / Katakana
        || (u >= 0xAC00 && u <= 0xD7AF);  // Hangul
}

// ---------- Стемминг ----------

// Лёгкий стеммер (Porter-подход, упрощённый). Русские слова сводятся к основе,
// английские — тоже. Не толмачит смысл, но резко повышает лексическое совпадение
// между разными словоформами ("работаешь" -> "работ", "работает" -> "работ").
QString TextTokenizer::stemWord(const QString &word) {
    if (word.size() < 4) return word;   // короткие (в т.ч. Cjk-биграммы) не трогаем

    QString w = word;

    // Русские/украинские окончания (от длинных к коротким).
    static const QStringList ru = {
        "ющейся", "ающейся", "ующийся", "ющийся", "ившийся", "ывшийся",
        "авшийся", "явшийся", "евшийся",
        "оваться", "еваться", "аваться", "иваться",
        "овавши", "ивавши",
        "овше", "авше", "явше", "ившие", "ывшие", "авшие", "явшие",
        "ование", "ивание", "ание", "ение", "ение",
        "ешься", "етесь", "емся", "имся", "аете", "уют",
        "овать", "евать", "авать", "ивать",
        "ешь", "ете", "ите", "ает", "яет", "ит", "ат", "ят",
        "ого", "ему", "ими", "ыми", "ому",
        "ала", "яла", "ела", "ила", "ыла",
        "ой", "ый", "ий", "ая", "яя", "ое", "ее", "ые", "ие",
        "ах", "ях", "ам", "ям", "ов", "ев", "ей",
        "а", "я", "о", "е", "у", "ю", "ы", "и", "ь"
    };

    // До двух проходов, всегда оставляем корень не короче 3 букв.
    for (int pass = 0; pass < 2; ++pass) {
        bool removed = false;
        for (const QString &s : ru) {
            if (w.size() - s.size() >= 3 && w.endsWith(s)) {
                w.chop(s.size());
                removed = true;
                break;
            }
        }
        if (!removed) break;
    }

    // Английские окончания.
    if (w.endsWith("ies") && w.size() > 4) {
        w.chop(2); w.append('y');
    } else {
        static const QStringList en = { "ing", "ed", "es" };
        for (const QString &s : en) {
            if (w.size() - s.size() >= 3 && w.endsWith(s)) { w.chop(s.size()); break; }
        }
        if (w.endsWith('s') && w.size() > 4) w.chop(1);
    }

    if (w.size() < 3) return word;
    return w;
}

// ---------- Токенизация ----------

void TextTokenizer::tokenizeInto(const QString &text, QHash<QString, int> &terms,
                                  QStringList *order) {
    if (text.isEmpty()) return;
    const QString lower = text.toLower();
    const int n = lower.size();

    auto addTerm = [&](const QString &t) {
        if (t.isEmpty()) return;
        // Лёгкий стемчинг (русские/английские окончания) — чтобы "проекте",
        // "проектом" и "проект" совпадали в индексе, даже когда эмбеддинги недоступны.
        QString term = stemWord(t);
        if (term.size() < 2) return;
        terms[term] = terms.value(term) + 1;
        if (order) order->append(term);
    };

    auto flushRun = [&](int from, int to, bool cjk) {
        if (from >= to) return;
        if (cjk) {
            int len = to - from;
            if (len == 1) {
                // Одиночный иероглиф/слог — оставляем как есть (в корейском это целое слово).
                QString s = lower.mid(from, 1);
                if (!isStopword(s)) addTerm(s);
                return;
            }
            // Для китайского/японского используем биграммы.
            for (int i = from; i + 1 < to; ++i) {
                QString bg = lower.mid(i, 2);
                if (!isStopword(bg)) addTerm(bg);
            }
            return;
        }
        // Латиница / кириллица / цифры.
        bool allDigits = true;
        for (int i = from; i < to; ++i) {
            if (!lower[i].isDigit()) { allDigits = false; break; }
        }
        if (allDigits) return;              // чистые номера/цифры не несём в индекс
        QString w = lower.mid(from, to - from);
        if (w.size() < 2) return;
        if (isStopword(w)) return;
        addTerm(w);
    };

    int runStart = -1;
    bool runIsCjk = false;
    for (int i = 0; i <= n; ++i) {
        bool boundary = (i == n);
        bool isLetter = !boundary && lower[i].isLetterOrNumber();
        bool cjk = !boundary && isLetter && isCjk(lower[i]);
        if (isLetter) {
            if (runStart < 0) {
                runStart = i;
                runIsCjk = cjk;
            } else if (runIsCjk != cjk) {
                flushRun(runStart, i, runIsCjk);
                runStart = i;
                runIsCjk = cjk;
            }
        } else {
            if (runStart >= 0) {
                flushRun(runStart, i, runIsCjk);
                runStart = -1;
            }
        }
    }
}

QStringList TextTokenizer::tokenize(const QString &text) {
    QHash<QString, int> terms;
    QStringList order;
    tokenizeInto(text, terms, &order);
    return order;
}
