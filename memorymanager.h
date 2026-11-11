#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include "constants.h"

class MemoryManager : public QObject
{
    Q_OBJECT

public:
    explicit MemoryManager(QObject *parent = nullptr);

    // Каждое сообщение получает уникальный id (stored в объекте сообщения и в
    // чаate-файле), чтобы UI мог адресовать конкретное сообщение.
    QString addMessage(const QString &role, const QString &text);
    QJsonArray getRecentHistory(int maxMessages = SUMMARIZATION_CHUNK_SIZE) const;
    QJsonArray exportMessages() const;
    void importMessages(const QJsonArray &messages);
    void clear();

    // --- Проверка/получение по id ---
    QString lastMessageId() const;
    QString textById(const QString &id) const;
    int indexOfMessageById(const QString &id) const;

    // --- Редактирование истории сообщений ---
    bool removeMessageById(const QString &id);
    bool replaceMessageById(const QString &id, const QString &newText);
    void truncateMessagesFrom(int startIndex);
    void clampSummarizedCountTo(int maxCount);

    // Сводка недействительна, если из-под неё вырезали сообщение: она описывает
    // текст, которого в истории больше нет. Тогда сводку сбрасываем целиком, и
    // она будет пересобрана с нуля по оставшейся истории.
    void invalidateSummaryFrom(int index);

    // Среднесрочная память (выжимание смыслов)
    bool needsSummarization() const;
    QString getSummarizeChunkText();
    void rollbackSummarization();
    // Полная замена сводки (не накопление). Используется при "свёртке" старого
    // конспекта с новым куском в один ограниченный по размеру конспект — так
    // среднесрочная память не растёт бесконечно на длинных диалогах.
    void setMidTermSummary(const QString &summary);
    QString getMidTermSummary() const;

    // Персистентность сводки: счётчик, зафиксированный в сводке на диске.
    // Пока сводка "в полёте" (модель ещё отвечает), возвращается последний
    // закоммиченный счётчик — чтобы на диск никогда не ушла рассогласованная
    // пара (новый счётчик + старая сводка).
    int getCommittedSummarizedCount() const;
    void setSummarizedCount(int count);

    // Применяет сводку и обрезает историю в RAM: оставляет несвёрнутый хвост
    // плюс буфер перед свёрнутой зоной. Вызывается после успешного получения
    // сводки от модели. Гарантирует ограниченный размер RAM (~44 сообщения).
    void commitSummaryAndTrim(const QString &summary, int bufferSize = TRIM_BUFFER_SIZE);

private:
    QJsonArray m_fullChatHistory;
    QString m_midTermSummary;
    int m_summarizedCount = 0;
    int m_lastCommittedCount = 0;   // последний согласованный со сводкой счётчик
    bool m_summaryInFlight = false; // сводка запрашивается у модели (счётчик уже сдвинут)
};

#endif // MEMORYMANAGER_H