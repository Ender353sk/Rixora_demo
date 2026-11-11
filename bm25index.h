#ifndef BM25INDEX_H
#define BM25INDEX_H

#include <QString>
#include <QStringList>
#include <QHash>
#include <QSet>

// Инвертированный индекс для лексической части гибридного поиска.
// Хранит posting-таблицы «терм -> слот документа -> частота» и обновляется
// инкрементально при добавлении/удалении факта: полная перестройка не нужна.
class BM25Index
{
public:
    // Регистрирует текст факта в слоте. Повторный вызов для того же слота
    // заменяет прежнюю запись.
    void addDocument(int slot, const QString &text);

    // Убирает слот из индекса. Для незарегистрированного слота — no-op.
    void removeDocument(int slot);

    // Полностью очищает индекс.
    void clear();

    // BM25-скор документа (slot) по списку термов запроса.
    float lexicalScore(const QStringList &queryTerms, int slot) const;

    // Сколько документов содержит данный терм.
    int docFreq(const QString &term) const;

    // Доля редких термов запроса, найденных в документе.
    float coverageScore(const QStringList &queryTerms, int rareQueryTerms, int slot) const;

    int documentCount() const { return m_docCount; }
    bool isEmpty() const { return m_docCount == 0; }

    // Все слоты, содержащие хотя бы один из термов запроса.
    QSet<int> slotsForTerms(const QStringList &terms) const;

private:
    QHash<QString, QHash<int, quint16>> m_postings; // терм -> слот -> tf
    QHash<int, QSet<QString>> m_docTerms;           // слот -> термы
    QHash<int, int> m_docLen;                       // слот -> длина в токенах
    int m_docCount = 0;
    qint64 m_sumDocLen = 0;
};

#endif // BM25INDEX_H
