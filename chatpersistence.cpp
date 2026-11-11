#include "chatpersistence.h"
#include "chatmanager.h"
#include "memorymanager.h"
#include "longtermmemory.h"
#include "apiconfigmanager.h"
#include <QSettings>
#include <QDebug>

ChatPersistence::ChatPersistence(ChatManager &chatManager,
                                 MemoryManager &memoryManager,
                                 LongTermMemory &longTerm,
                                 QObject *parent)
    : QObject(parent),
      m_chatManager(chatManager),
      m_memoryManager(memoryManager),
      m_longTerm(longTerm)
{
}

void ChatPersistence::saveCurrentChat()
{
    QString chatId = m_chatManager.getCurrentChatId();
    if (chatId.isEmpty()) {
        chatId = m_chatManager.createNewChat();
    }
    m_chatManager.saveChat(chatId, m_memoryManager.exportMessages(),
                           m_memoryManager.getMidTermSummary(),
                           m_memoryManager.getCommittedSummarizedCount());
    QSettings settings;
    settings.setValue("app/lastChatId", chatId);
    qDebug() << "[ChatPersistence] Текущий чат сохранен:" << chatId;
}

QJsonArray ChatPersistence::loadChat(const QString &chatId)
{
    const QJsonArray raw = m_chatManager.loadChat(chatId);

    m_memoryManager.clear();
    m_memoryManager.importMessages(raw);

    QJsonObject mem = m_chatManager.loadChatMemory(chatId);
    m_memoryManager.setMidTermSummary(mem["midTermSummary"].toString());
    m_memoryManager.setSummarizedCount(mem["summarizedCount"].toInt());

    QSettings settings;
    settings.setValue("app/lastChatId", chatId);
    const QJsonArray messages = m_memoryManager.exportMessages();
    qDebug() << "[ChatPersistence] Чат загружен:" << chatId << "сообщений:" << messages.size();
    return messages;
}

QJsonArray ChatPersistence::restoreLastSession()
{
    QSettings settings;
    QString lastChatId = settings.value("app/lastChatId", "").toString();

    if (!lastChatId.isEmpty()) {
        const QJsonArray raw = m_chatManager.loadChat(lastChatId);
        if (!raw.isEmpty()) {
            m_memoryManager.clear();
            m_memoryManager.importMessages(raw);
            QJsonObject mem = m_chatManager.loadChatMemory(lastChatId);
            m_memoryManager.setMidTermSummary(mem["midTermSummary"].toString());
            m_memoryManager.setSummarizedCount(mem["summarizedCount"].toInt());
            const QJsonArray messages = m_memoryManager.exportMessages();
            qDebug() << "[ChatPersistence] Восстановлена последняя сессия:" << lastChatId << "сообщений:" << messages.size();
            return messages;
        }
    }

    m_chatManager.createNewChat();
    return QJsonArray();
}

void ChatPersistence::deleteChat(const QString &chatId)
{
    m_chatManager.deleteChat(chatId);
    m_longTerm.deleteByChatId(chatId);
    if (m_chatManager.getCurrentChatId().isEmpty()) {
        m_memoryManager.clear();
    }
    QSettings settings;
    if (settings.value("app/lastChatId").toString() == chatId) {
        settings.remove("app/lastChatId");
    }
    m_longTerm.saveBackfillCursor(0, QHash<QString, int>());
    qDebug() << "[ChatPersistence] Чат удалён полностью:" << chatId;
    emit chatListChanged();
}

void ChatPersistence::deleteAllChats()
{
    m_chatManager.deleteAllChats();
    m_longTerm.deleteAllMemory();
    m_memoryManager.clear();
    m_longTerm.saveBackfillCursor(0, QHash<QString, int>());
    qDebug() << "[ChatPersistence] Все чаты и вся долгосрочная память удалены.";
    emit chatListChanged();
}

QString ChatPersistence::getCurrentChatId() const
{
    return m_chatManager.getCurrentChatId();
}

QString ChatPersistence::createNewChat()
{
    return m_chatManager.createNewChat();
}

QJsonArray ChatPersistence::getChatList()
{
    return m_chatManager.getChatList();
}

void ChatPersistence::saveResponseToOriginalChat(const QString &chatId, const QString &response)
{
    if (response.isEmpty() || chatId.isEmpty()) return;

    if (!m_chatManager.chatExists(chatId)) return;

    QJsonArray msgs = m_chatManager.peekChatMessages(chatId, 100000, 0);
    QJsonObject replyMsg;
    replyMsg["role"] = "model";
    replyMsg["parts"] = QJsonArray{QJsonObject{{"text", response}}};
    msgs.append(replyMsg);
    const QJsonObject mem = m_chatManager.loadChatMemory(chatId);
    m_chatManager.saveChat(chatId, msgs,
                           mem["midTermSummary"].toString(),
                           mem["summarizedCount"].toInt());
    qDebug() << "[ChatPersistence] Ответ сохранён в исходный чат после переключения:" << chatId;
}