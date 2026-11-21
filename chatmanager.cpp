#include "chatmanager.h"
#include "sqlitememoryrepository.h"
#include <QSettings>
#include <QStandardPaths>

ChatManager::ChatManager(SqliteMemoryRepository *repository, QObject *parent)
    : QObject(parent), m_repository(repository), m_ownsRepository(false)
{
    if (!m_repository) {
        m_repository = new SqliteMemoryRepository(this);
        m_ownsRepository = true;
        // Тестова база у тимчасовому каталізні
        QStandardPaths::setTestModeEnabled(true);
        QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/test_memory.db";
        m_repository->initialize(path);
    }
}

QString ChatManager::createNewChat()
{
    QString chatId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_currentChatId = chatId;

    // Створюємо запис чату в БД
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_repository->upsertChat(chatId, "Новий чат", now, now, 0);

    return chatId;
}

void ChatManager::saveChat(const QString &chatId, const QJsonArray &messages,
                           const QString &midTermSummary, int summarizedCount)
{
    if (chatId.isEmpty() || !m_repository) return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // Отримуємо заголовок (створюємо якщо нового чату ще немає в БД)
    QString title;
    if (!m_repository->chatExists(chatId)) {
        title = generateChatTitle(messages);
        m_repository->upsertChat(chatId, title, now, now, messages.size());
    } else {
        title = generateChatTitle(messages);
    }

    // Зберігаємо повідомлення
    m_repository->saveMessages(chatId, messages);

    // Зберігаємо свідку
    m_repository->saveChatMemory(chatId, midTermSummary, summarizedCount);

    // Оновлюємо метадані чату
    m_repository->upsertChat(chatId, title, now, now, messages.size());
}

QJsonArray ChatManager::loadChat(const QString &chatId)
{
    if (!m_repository) return QJsonArray();

    m_currentChatId = chatId;
    return m_repository->loadMessages(chatId);
}

void ChatManager::deleteChat(const QString &chatId)
{
    if (!m_repository) return;

    m_repository->deleteChat(chatId);
    if (m_currentChatId == chatId) {
        m_currentChatId.clear();
    }
}

void ChatManager::deleteAllChats()
{
    if (!m_repository) return;

    m_repository->deleteAllChats();
    m_currentChatId.clear();
}

QJsonArray ChatManager::getChatList()
{
    if (!m_repository) return QJsonArray();
    return m_repository->getChatList();
}

QJsonArray ChatManager::getChatListOldestFirst()
{
    if (!m_repository) return QJsonArray();
    return m_repository->getChatListOldestFirst();
}

QString ChatManager::getCurrentChatId() const
{
    return m_currentChatId;
}

void ChatManager::setCurrentChatId(const QString &chatId)
{
    if (m_currentChatId != chatId) {
        m_currentChatId = chatId;
        if (m_repository) {
            m_repository->setCurrentChatId(chatId);
        }
    }
}

QJsonArray ChatManager::peekChatMessages(const QString &chatId, int lastN, int skipFromEnd) const
{
    if (!m_repository) return QJsonArray();
    return m_repository->peekMessages(chatId, lastN, skipFromEnd);
}

QJsonObject ChatManager::loadChatMemory(const QString &chatId) const
{
    if (!m_repository) return QJsonObject();
    return m_repository->loadChatMemory(chatId);
}

QString ChatManager::generateChatTitle(const QJsonArray &messages)
{
    for (const QJsonValue &val : messages) {
        QJsonObject msg = val.toObject();
        if (msg["role"].toString() == "user") {
            const QJsonArray parts = msg["parts"].toArray();
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
    return "Новий чат";
}

bool ChatManager::chatExists(const QString &chatId) const
{
    if (!m_repository) return false;
    return m_repository->chatExists(chatId);
}
