#ifndef CHATUTILS_H
#define CHATUTILS_H

#include <QStringList>
#include <QJsonArray>

// Собирает id сообщений из истории, начиная с fromIndex.
// Используется перед мутацией истории (truncate/remove), чтобы знать,
// какие факты удалить из долговременной памяти.
QStringList messageIdsFrom(const QJsonArray &hist, int fromIndex);

#endif // CHATUTILS_H