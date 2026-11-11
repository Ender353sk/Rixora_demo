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

class ChatManager : public QObject
{
    Q_OBJECT

public:
    explicit ChatManager(QObject *parent = nullptr);

    Q_INVOKABLE QString createNewChat();
    Q_INVOKABLE void saveChat(const QString &chatId, const QJsonArray &messages,
                              const QString &midTermSummary = QString(),
                              int summarizedCount = 0);
    Q_INVOKABLE QJsonArray loadChat(const QString &chatId);
    Q_INVOKABLE void deleteChat(const QString &chatId);
    Q_INVOKABLE void deleteAllChats();
    Q_INVOKABLE QJsonArray getChatList();
    // Тот же список, но от САМОГО СТАРОГО чата к новому. Нужен бэкафиллу
    // долгосрочной памяти: он обязан идти по истории от начала, иначе свежие
    // чаты вытесняют старые и до них очередь не доходит. Порядок getChatList()
    // (сначала свежие) для UI не трогаем — он там осмыслен.
    Q_INVOKABLE QJsonArray getChatListOldestFirst();
    Q_INVOKABLE QString getCurrentChatId() const;
    Q_INVOKABLE void setCurrentChatId(const QString &chatId);
    Q_INVOKABLE QString generateChatTitle(const QJsonArray &messages);
    // lastN сообщений от указанного конца истории (skipFromEnd отсчитывается с конца);
    // используется для курсорной дозагрузки долгосрочной памяти из глубины чатов.
    Q_INVOKABLE QJsonArray peekChatMessages(const QString &chatId, int lastN,
                                            int skipFromEnd = 0) const;
    // Сохранённая сводка среднесрочной памяти чата (для восстановления после перезапуска).
    QJsonObject loadChatMemory(const QString &chatId) const;
    // O(1)-проверка «чат на диске есть». Раньше те же места делали
    // getChatList() — то есть открывали и разбирали JSON ВСЕХ чатов, чтобы
    // найти один. На каждый ответ это десятки файлов и полная выборка
    // сообщений в память, с ростом по мере накопления истории.
    bool chatExists(const QString &chatId) const;

private:
    QString chatsDirectory() const;
    QString chatFilePath(const QString &chatId) const;
    QString indexFilePath() const;
    QJsonObject readChatFile(const QString &chatId) const;
    void writeChatFile(const QString &chatId, const QJsonObject &data);

    // Индекс метаданных чатов: id -> {id,title,createdAt,updatedAt,messageCount}.
    // Позволяет строить список истории без чтения и разбора файлов чатов.
    // Самоиндексирующийся: если набор файлов на диске не совпадает с индексом,
    // индекс молча пересобирается чтением файлов, так что рассинхрона быть не
    // может (в т.ч. после копирования/удаления файлов вручную).
    QJsonObject readIndex() const;
    void writeIndex(const QJsonObject &index) const;
    void indexUpsert(const QString &chatId, const QJsonObject &meta);
    void indexRemove(const QString &chatId);

    QString m_currentChatId;
};

#endif // CHATMANAGER_H
