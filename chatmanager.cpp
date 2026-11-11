#include "chatmanager.h"
#include <QSettings>
#include <QSaveFile>
#include <QFile>
ChatManager::ChatManager(QObject *parent)
    : QObject(parent)
{
    QString dir = chatsDirectory();
    if (!QDir().exists(dir)) {
        QDir().mkpath(dir);
    }
}

QString ChatManager::chatsDirectory() const {
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dataPath + "/chats";
}

QString ChatManager::chatFilePath(const QString &chatId) const {
    return chatsDirectory() + "/" + chatId + ".json";
}

// Индекс лежит рядом с чатами, но под расширением без ".json", чтобы никогда
// не попасть в QDir("*.json") — ни в getChatList(), ни в deleteAllChats(),
// который удаляет все чаты разом.
QString ChatManager::indexFilePath() const {
    return chatsDirectory() + "/.chats_index";
}

bool ChatManager::chatExists(const QString &chatId) const {
    return !chatId.isEmpty() && QFile::exists(chatFilePath(chatId));
}

QJsonObject ChatManager::readIndex() const {
    QFile file(indexFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    return obj;
}

void ChatManager::writeIndex(const QJsonObject &index) const {
    QSaveFile file(indexFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(index).toJson(QJsonDocument::Compact));
    file.commit();
}

void ChatManager::indexUpsert(const QString &chatId, const QJsonObject &meta) {
    QJsonObject index = readIndex();
    index.insert(chatId, meta);
    writeIndex(index);
}

void ChatManager::indexRemove(const QString &chatId) {
    QJsonObject index = readIndex();
    if (index.contains(chatId)) {
        index.remove(chatId);
        writeIndex(index);
    }
}

QJsonObject ChatManager::readChatFile(const QString &chatId) const {
    QFile file(chatFilePath(chatId));
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    return doc.object();
}

void ChatManager::writeChatFile(const QString &chatId, const QJsonObject &data) {
    QFile file(chatFilePath(chatId));
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "[ChatManager] Ошибка записи файла:" << chatFilePath(chatId);
        return;
    }
    file.write(QJsonDocument(data).toJson(QJsonDocument::Indented));
    file.close();
}

QString ChatManager::createNewChat() {
    QString chatId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_currentChatId = chatId;
    return chatId;
}

void ChatManager::saveChat(const QString &chatId, const QJsonArray &messages,
                           const QString &midTermSummary, int summarizedCount) {
    if (chatId.isEmpty()) return;
    QJsonObject chatData = readChatFile(chatId);
    if (chatData.isEmpty()) {
        chatData["id"] = chatId;
        chatData["createdAt"] = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm");
    }

    chatData["messages"] = messages;
    chatData["updatedAt"] = QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm");
    chatData["midTermSummary"] = midTermSummary;
    chatData["summarizedCount"] = summarizedCount;
    QString title = generateChatTitle(messages);
    if (!title.isEmpty()) {
        chatData["title"] = title;
    }

    writeChatFile(chatId, chatData);
    QJsonObject meta;
    meta["id"] = chatData["id"].toString();
    meta["title"] = chatData["title"].toString();
    meta["createdAt"] = chatData["createdAt"].toString();
    meta["updatedAt"] = chatData["updatedAt"].toString();
    meta["messageCount"] = messages.size();
    indexUpsert(chatId, meta);

}

QJsonArray ChatManager::loadChat(const QString &chatId) {
    QJsonObject chatData = readChatFile(chatId);
    if (chatData.isEmpty()) {
        return QJsonArray();
    }
    m_currentChatId = chatId;
    return chatData["messages"].toArray();
}

void ChatManager::deleteChat(const QString &chatId) {
    QFile file(chatFilePath(chatId));
    if (file.exists()) {
        file.remove();
        if (m_currentChatId == chatId) {
            m_currentChatId.clear();
        }
        indexRemove(chatId);

    }
}

void ChatManager::deleteAllChats() {
    QDir dir(chatsDirectory());
    const QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files);
    for (const QString &f : files) {
        QFile::remove(dir.absoluteFilePath(f));
    }
    m_currentChatId.clear();
    // Забытый lastChatId = битый чат при следующем старте: приложение пытается
    // открыть несуществующий id. deleteChat() это уже делал, а полная очистка —
    // нет.
    QSettings().remove("app/lastChatId");
    QFile::remove(indexFilePath());

}

QJsonArray ChatManager::getChatList() {
    QDir dir(chatsDirectory());
    const QFileInfoList fileList = dir.entryInfoList(QStringList() << "*.json", QDir::Files, QDir::Time);
    QStringList ids;
    ids.reserve(fileList.size());
    for (qsizetype i = 0; i < fileList.size(); ++i) {
        ids.append(fileList.at(i).baseName());
    }

    // Индекс самопроверяется по набору имён файлов: список файлов мы всё равно
    // получили (это дешёвый листинг каталога), а вот читать их содержимое
    // больше не нужно. Если файлы добавили/удалили мимо нас, индекс не совпадёт
    // и будет пересобран — рассинхронизация невозможна.
    QJsonObject index = readIndex();
    bool consistent = (index.size() == ids.size());
    if (consistent) {
        for (const QString &id : ids) {
            if (!index.contains(id)) {
                consistent = false;
                break;
            }
        }
    }

    QJsonArray chatList;
    if (consistent) {
        for (const QString &id : ids) {
            chatList.append(index.value(id).toObject());
        }
        return chatList;
    }

    // Холодный старт / рассинхрон: собираем индекс, читая чаты один раз.
    index = QJsonObject();
    for (const QString &id : ids) {
        const QJsonObject chatData = readChatFile(id);
        if (chatData.isEmpty()) {
            continue;
        }
        QJsonObject meta;
        meta["id"] = chatData["id"].toString();
        meta["title"] = chatData["title"].toString();
        meta["createdAt"] = chatData["createdAt"].toString();
        meta["updatedAt"] = chatData["updatedAt"].toString();
        meta["messageCount"] = chatData["messages"].toArray().size();
        index.insert(id, meta);
        chatList.append(meta);
    }
    writeIndex(index);
    return chatList;
}

// Сортировка по РАЗОБРАННОЙ дате: updatedAt хранится как "dd.MM.yyyy HH:mm",
// и такая строка не упорядочена лексикографически ("31.12.2025 23:00" >
// "01.01.2026 00:00" текстом, хотя хронологически наоборот). Наивная сортировка
// строк здесь дала бы случайный порядок бэкафилла.
QJsonArray ChatManager::getChatListOldestFirst() {
    const QString dtFormat = "dd.MM.yyyy HH:mm";
    const auto stamp = [&dtFormat](const QJsonObject &meta) -> QDateTime {
        const QDateTime dt = QDateTime::fromString(meta["updatedAt"].toString(), dtFormat);
        return dt.isValid() ? dt : QDateTime();
    };
    const QJsonArray newestFirst = getChatList();
    QList<QPair<QDateTime, QJsonObject>> sorted;
    sorted.reserve(newestFirst.size());
    for (const QJsonValue &v : newestFirst) {
        const QJsonObject meta = v.toObject();
        sorted.append({stamp(meta), meta});
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const QPair<QDateTime, QJsonObject> &a, const QPair<QDateTime, QJsonObject> &b) {
                  return a.first < b.first;   // невалидные (QDateTime()) уходят в конец
              });
    QJsonArray out;
    for (const auto &pair : sorted) out.append(pair.second);
    return out;
}

QString ChatManager::getCurrentChatId() const {
    return m_currentChatId;
}

void ChatManager::setCurrentChatId(const QString &chatId) {
    if (m_currentChatId != chatId) {
        m_currentChatId = chatId;

    }
}

QJsonArray ChatManager::peekChatMessages(const QString &chatId, int lastN, int skipFromEnd) const {
    const QJsonObject data = readChatFile(chatId);
    const QJsonArray msgs = data["messages"].toArray();
    const int end = msgs.size() - qMax(0, skipFromEnd);
    const int start = qMax(0, end - lastN);
    QJsonArray out;
    for (int i = start; i < end; ++i) out.append(msgs[i]);
    return out;
}

QJsonObject ChatManager::loadChatMemory(const QString &chatId) const {
    QJsonObject out;
    const QJsonObject data = readChatFile(chatId);
    out["midTermSummary"] = data["midTermSummary"].toString();
    out["summarizedCount"] = data["summarizedCount"].toInt();
    return out;
}

QString ChatManager::generateChatTitle(const QJsonArray &messages) {
    for (const QJsonValue &val : messages) {
        QJsonObject msg = val.toObject();
        if (msg["role"].toString() == "user") {
            const QJsonArray parts = msg["parts"].toArray();
            // Пустой parts раньше приводил к выходу за границы массива.
            // QJsonObject нужно материализовать в переменную: operator[]
            // на временном объекте от toObject() — лишнее разделение данных.
            QString text;
            if (!parts.isEmpty()) {
                const QJsonObject part = parts.at(0).toObject();
                text = part["text"].toString();
            }
            if (text.length() > 40) {
                return text.left(40) + "...";
            }
            return text;
        }
    }
    return "Новый чат";
}
