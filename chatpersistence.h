#ifndef CHATPERSISTENCE_H
#define CHATPERSISTENCE_H

#include <QObject>
#include <QString>
#include <QJsonArray>

class ChatManager;
class MemoryManager;
class LongTermMemory;
class ApiConfigManager;

class ChatPersistence : public QObject
{
    Q_OBJECT
public:
    explicit ChatPersistence(ChatManager &chatManager,
                             MemoryManager &memoryManager,
                             LongTermMemory &longTerm,
                             QObject *parent = nullptr);

    void saveCurrentChat();
    QJsonArray loadChat(const QString &chatId);
    QJsonArray restoreLastSession();
    void deleteChat(const QString &chatId);
    void deleteAllChats();
    QString getCurrentChatId() const;
    QString createNewChat();
    QJsonArray getChatList();
    void saveResponseToOriginalChat(const QString &chatId, const QString &response);

signals:
      void chatListChanged();
  

private:
    ChatManager &m_chatManager;
    MemoryManager &m_memoryManager;
    LongTermMemory &m_longTerm;
};

#endif // CHATPERSISTENCE_H
