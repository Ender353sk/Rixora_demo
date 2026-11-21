#ifndef CHATMANAGER_H
#define CHATMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfoList>
#include <QUuid>
#include <QDateTime>
#include <QDebug>

class SqliteMemoryRepository;

class ChatManager : public QObject
{
    Q_OBJECT

public:
    explicit ChatManager(SqliteMemoryRepository *repository = nullptr, QObject *parent = nullptr);

    Q_INVOKABLE QString createNewChat();
    Q_INVOKABLE void saveChat(const QString &chatId, const QJsonArray &messages,
                              const QString &midTermSummary = QString(),
                              int summarizedCount = 0);
    Q_INVOKABLE QJsonArray loadChat(const QString &chatId);
    Q_INVOKABLE void deleteChat(const QString &chatId);
    Q_INVOKABLE void deleteAllChats();
    Q_INVOKABLE QJsonArray getChatList();
    Q_INVOKABLE QJsonArray getChatListOldestFirst();
    Q_INVOKABLE QString getCurrentChatId() const;
    Q_INVOKABLE void setCurrentChatId(const QString &chatId);
    Q_INVOKABLE QString generateChatTitle(const QJsonArray &messages);
    Q_INVOKABLE QJsonArray peekChatMessages(const QString &chatId, int lastN,
                                            int skipFromEnd = 0) const;
    Q_INVOKABLE QJsonObject loadChatMemory(const QString &chatId) const;
    bool chatExists(const QString &chatId) const;

    SqliteMemoryRepository *repository() const { return m_repository; }

private:
    QString m_currentChatId;
    SqliteMemoryRepository *m_repository;
    bool m_ownsRepository = false;
};

#endif // CHATMANAGER_H
