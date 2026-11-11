#include "chatutils.h"
#include <QJsonObject>

QStringList messageIdsFrom(const QJsonArray &hist, int fromIndex) {
    QStringList ids;
    for (int i = qMax(0, fromIndex); i < hist.size(); ++i) {
        const QString id = hist[i].toObject()["id"].toString();
        if (!id.isEmpty()) ids << id;
    }
    return ids;
}
