#ifndef TEXTTOKENIZER_H
#define TEXTTOKENIZER_H

#include <QString>
#include <QStringList>
#include <QHash>

// Статический утилитный класс для токенизации и нормализации текста.
// Используется BM25-индексом и поиском LongTermMemory для приведения
// текста к «стьюILLE-форме»: стемминг окончаний + фильтрация стоп-слов
// + биграммы для CJK-SCRIPLT.
class TextTokenizer
{
public:
    // Разбивает текст на токены с учётом CJK-биграмм, стоп-слов и стемминга.
    // terms — маппинг токен → частота, order — порядок появления (опционально).
    static void tokenizeInto(const QString &text, QHash<QString, int> &terms,
                             QStringList *order = nullptr);

    // Обёртка: возвращает список токенов в порядке появления.
    static QStringList tokenize(const QString &text);

private:
    // Porter-подобный стеммер для русского/украинского/английского.
    // Сводит словоформу к основе («работаешь» → «работ», «running» → «run»).
    static QString stemWord(const QString &word);

    // Стоп-слова: общеупотребительные слова, не несущие смысловой нагрузки.
    static bool isStopword(const QString &word);

    // Проверка символа на принадлежность к CJK-блокам (Китай/Японский/Корейский).
    static bool isCjk(const QChar &ch);
};

#endif // TEXTTOKENIZER_H