#include "memorymanager.h"
#include "geminiadapter.h"
#include <QDebug>

MemoryManager::MemoryManager(QObject *parent)
    : QObject(parent)
{
}

QString MemoryManager::addMessage(const QString &role, const QString &text) {
    QJsonObject msg;
    msg["role"] = role;
    msg["parts"] = QJsonArray{QJsonObject{{"text", text}}};
    msg["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_fullChatHistory.append(msg);
    return msg["id"].toString();
}

QString MemoryManager::lastMessageId() const {
    if (m_fullChatHistory.isEmpty()) return QString();
    return m_fullChatHistory.last().toObject()["id"].toString();
}

QString MemoryManager::textById(const QString &id) const {
    for (const QJsonValue &val : m_fullChatHistory) {
        const QJsonObject o = val.toObject();
        if (o["id"].toString() == id) {
            return o["parts"].toArray()[0].toObject()["text"].toString();
        }
    }
    return QString();
}

int MemoryManager::indexOfMessageById(const QString &id) const {
    for (int i = 0; i < m_fullChatHistory.size(); ++i) {
        if (m_fullChatHistory[i].toObject()["id"].toString() == id) return i;
    }
    return -1;
}

bool MemoryManager::removeMessageById(const QString &id) {
    for (int i = 0; i < m_fullChatHistory.size(); ++i) {
        if (m_fullChatHistory[i].toObject()["id"].toString() == id) {
            m_fullChatHistory.removeAt(i);
            invalidateSummaryFrom(i);
            clampSummarizedCountTo(i);
            return true;
        }
    }
    return false;
}

bool MemoryManager::replaceMessageById(const QString &id, const QString &newText) {
    for (int i = 0; i < m_fullChatHistory.size(); ++i) {
        QJsonObject o = m_fullChatHistory[i].toObject();
        if (o["id"].toString() == id) {
            o["parts"] = QJsonArray{QJsonObject{{"text", newText}}};
            m_fullChatHistory[i] = o;
            return true;
        }
    }
    return false;
}

void MemoryManager::truncateMessagesFrom(int startIndex) {
    if (startIndex < 0 || startIndex >= m_fullChatHistory.size()) return;
    while (m_fullChatHistory.size() > startIndex) {
        m_fullChatHistory.removeLast();
    }
    invalidateSummaryFrom(startIndex);
    clampSummarizedCountTo(startIndex);
}

// Сводка покрывает первые m_summarizedCount сообщений. Если правка затрагивает
// это диапазон (index < m_summarizedCount), сводка описывает уже несуществующий
// текст — сбрасываем её полностью. Иначе (правка за пределами сводки) сводка
// остаётся валидной, и её трогать нельзя.
void MemoryManager::invalidateSummaryFrom(int index) {
    if (index < 0) return;
    if (index >= m_summarizedCount) return;

    qDebug() << "[MemoryManager] Сводка недействительна (правка с индекса"
             << index << "), сбрасываю.";
    m_midTermSummary.clear();
    m_summarizedCount = 0;
    m_lastCommittedCount = 0;
    m_summaryInFlight = false;
}

void MemoryManager::clampSummarizedCountTo(int maxCount) {
    if (maxCount < 0) return;
    if (m_summarizedCount > maxCount) m_summarizedCount = maxCount;
    m_summarizedCount = qMin(m_summarizedCount, m_fullChatHistory.size());
    if (m_lastCommittedCount > m_summarizedCount) m_lastCommittedCount = m_summarizedCount;
}

QJsonArray MemoryManager::getRecentHistory(int maxMessages) const {
    QJsonArray shortTermHistory;
    int startIndex = qMax(0, m_fullChatHistory.size() - maxMessages);
    for (int i = startIndex; i < m_fullChatHistory.size(); ++i) {
        shortTermHistory.append(m_fullChatHistory[i]);
    }

    if (shortTermHistory.isEmpty()) return shortTermHistory;

    // Приведение к формату Gemini (строгое чередование user/model, завершается user) —
    // зона ответственности адаптера: MemoryManager не знает о нюансах конкретной LLM.
    return GeminiHistoryAdapter::normalizeHistoryForGemini(shortTermHistory);
}

QJsonArray MemoryManager::exportMessages() const {
    return m_fullChatHistory;
}

void MemoryManager::importMessages(const QJsonArray &messages) {
    m_fullChatHistory = QJsonArray();
    for (const QJsonValue &val : messages) {
        QJsonObject msg = val.toObject();
        QString role = msg["role"].toString();
        QString text = msg["parts"].toArray()[0].toObject()["text"].toString();
        // Пустое сообщение в истории ломает две вещи: summarizeChunkText склеит
        // его в конспект, а getRecentHistory отдаст его модели. Мусорный текст
        // (в т.ч. одни пробелы) в память не попадает.
        if (text.trimmed().isEmpty()) continue;
        if (role.isEmpty() && !text.isEmpty()) {
            role = msg["isUser"].toBool() ? "user" : "model";
        }
        // Сохраняем ID из загруженного чата, если он есть (важно для привязки фактов к msgId).
        // Если ID нет — генерируем новый (совместимость со старыми файлами).
        QString id = msg["id"].toString();
        if (id.isEmpty()) {
            id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        QJsonObject newMsg;
        newMsg["role"] = role;
        newMsg["parts"] = QJsonArray{QJsonObject{{"text", text}}};
        newMsg["id"] = id;
        m_fullChatHistory.append(newMsg);
    }
}

void MemoryManager::clear() {
    m_fullChatHistory = QJsonArray();
    m_midTermSummary.clear();
    m_summarizedCount = 0;
    m_lastCommittedCount = 0;
    m_summaryInFlight = false;
    qDebug() << "[MemoryManager] Память полностью очищена.";
}

bool MemoryManager::needsSummarization() const {
    // Пока одна сводка "в полёте", новую не запускаем: две параллельные свёртки
    // гонялись бы друг с другом, и более поздний ответ перезаписал бы первую,
    // потеряв кусок истории из конспекта.
    if (m_summaryInFlight) return false;
    return (m_fullChatHistory.size() - m_summarizedCount >= SUMMARIZATION_CHUNK_SIZE);
}

QString MemoryManager::getSummarizeChunkText() {
    // Собираем в локальную строку и отдаём её же: отдельного поля-хранения
    // не держим, состояние свёртки описывают m_summarizedCount,
    // m_lastCommittedCount и m_summaryInFlight.
    QString chunk;
    for (int i = m_summarizedCount; i < m_summarizedCount + SUMMARIZATION_CHUNK_SIZE; ++i) {
        QString role = m_fullChatHistory[i].toObject()["role"].toString();
        QString text = m_fullChatHistory[i].toObject()["parts"].toArray()[0].toObject()["text"].toString();
        chunk += role + ": " + text + "\n";
    }
    m_lastCommittedCount = m_summarizedCount;
    m_summarizedCount += SUMMARIZATION_CHUNK_SIZE;
    m_summaryInFlight = true;
    return chunk;
}

void MemoryManager::rollbackSummarization() {
    m_summarizedCount = qMax(0, m_summarizedCount - SUMMARIZATION_CHUNK_SIZE);
    m_summaryInFlight = false;
    m_lastCommittedCount = m_summarizedCount;
}

void MemoryManager::setMidTermSummary(const QString &summary) {
    m_midTermSummary = summary;
    m_summaryInFlight = false;
    m_lastCommittedCount = m_summarizedCount;
}

QString MemoryManager::getMidTermSummary() const {
    return m_midTermSummary;
}

int MemoryManager::getCommittedSummarizedCount() const {
    return m_summaryInFlight ? m_lastCommittedCount : m_summarizedCount;
}

void MemoryManager::setSummarizedCount(int count) {
    m_summarizedCount = qMax(0, qMin(count, m_fullChatHistory.size()));
    m_lastCommittedCount = m_summarizedCount;
    m_summaryInFlight = false;
}

void MemoryManager::commitSummaryAndTrim(const QString &summary, int bufferSize) {
    // Оставляем: последние bufferSize сообщений свёрнутого чанка + весь несвёрнутый хвост.
    // Удаляем сообщения с НАЧАЛА (старые, уже описанные в сводке).
    int keepFrom = qMax(0, m_summarizedCount - bufferSize);
    int removeCount = keepFrom;
    for (int i = 0; i < removeCount; ++i) {
        m_fullChatHistory.removeAt(0);
    }

    // Корректируем счётчик свёрнутых сообщений: осталось bufferSize свёрнутых
    m_summarizedCount -= removeCount;
    m_summarizedCount = qMax(0, m_summarizedCount);
    m_summarizedCount = qMin(m_summarizedCount, m_fullChatHistory.size());
    if (m_lastCommittedCount > m_summarizedCount) m_lastCommittedCount = m_summarizedCount;

    // Применяем новую сводку (после корректировки счётчика)
    setMidTermSummary(summary);
}