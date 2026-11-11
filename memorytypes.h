#ifndef MEMORYTYPES_H
#define MEMORYTYPES_H

#include <QString>

// Результат поиска по долговременной памяти.
struct MemoryHit {
    QString text;
    QString kind;        // тип записи; для фактов — "fact"
    QString chatId;
    QString msgId;       // сообщение-источник (может быть пустым)
    qint64 ts = 0;
    float score = 0.0f;          // итоговый скор после переранжировки
    float vectorScore = 0.0f;    // косинусная близость (0..1)
    float lexicalScore = 0.0f;   // лексический скор (0..1)
};

#endif // MEMORYTYPES_H
