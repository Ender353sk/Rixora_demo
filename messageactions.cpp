#include "messageactions.h"
#include "chatutils.h"
#include "memorymanager.h"
#include "longtermmemory.h"
#include "chatmanager.h"
#include "geminiapi.h"
#include "audioengine.h"
#include "apiconfigmanager.h"
#include <QGuiApplication>
#include <QDebug>

MessageActions::MessageActions(MemoryManager &memoryManager,
                               LongTermMemory &longTerm,
                               ChatManager &chatManager,
                               GeminiApi &geminiApi,
                               AudioEngine &audioEngine,
                               QObject *parent)
    : QObject(parent),
      m_memoryManager(memoryManager),
      m_longTerm(longTerm),
      m_chatManager(chatManager),
      m_geminiApi(geminiApi),
      m_audioEngine(audioEngine)
{
    // Единственное место, где состояние озвучки сбрасывается: id очищается
    // ровно тогда, когда движок действительно перестал озвучивать.
    connect(&m_audioEngine, &AudioEngine::ttsFinished, this, [this]() {
        clearSpeakingMessage();
    });
}

bool MessageActions::deleteChatMessage(const QString &messageId)
{
    if (messageId.isEmpty()) return false;
    if (m_geminiApi.isBusy()) return false;

    const QString chatId = m_chatManager.getCurrentChatId();
    if (chatId.isEmpty()) {
        qDebug() << "[MessageActions] Удаление сообщения отменено: не выбран чат.";
        return false;
    }
    if (!m_memoryManager.removeMessageById(messageId)) return false;

    m_longTerm.deleteByMsgId(m_chatManager.getCurrentChatId(), messageId);
    m_chatManager.saveChat(chatId, m_memoryManager.exportMessages(),
                           m_memoryManager.getMidTermSummary(),
                           m_memoryManager.getCommittedSummarizedCount());
    qDebug() << "[MessageActions] Сообщение удалено из чата и памяти:" << messageId;
    return true;
}

bool MessageActions::rewriteChatMessage(const QString &messageId)
{
    if (messageId.isEmpty()) return false;
    if (m_geminiApi.isBusy()) return false;

    const int idx = m_memoryManager.indexOfMessageById(messageId);
    if (idx < 0) return false;

    const QJsonArray hist = m_memoryManager.exportMessages();
    if (idx >= hist.size()) return false;
    if (hist[idx].toObject()["role"].toString() != "model") return false;
    if (idx == 0) return false;
    if (hist[idx - 1].toObject()["role"].toString() != "user") return false;

    const QString prompt = hist[idx - 1].toObject()["parts"]
                               .toArray()[0].toObject()["text"].toString();
    if (prompt.trimmed().isEmpty()) return false;

    const QStringList staleIds = messageIdsFrom(hist, idx);
    m_memoryManager.truncateMessagesFrom(idx);
    for (const QString &staleId : staleIds) {
        m_longTerm.deleteByMsgId(m_chatManager.getCurrentChatId(), staleId);
    }
    m_chatManager.saveChat(m_chatManager.getCurrentChatId(), m_memoryManager.exportMessages(),
                           m_memoryManager.getMidTermSummary(),
                           m_memoryManager.getCommittedSummarizedCount());

    // Без долговременной памяти эмбеддинг ни на что не влияет (фактов для
    // поиска нет) — экономим круговой запрос сети, как в processUserMessage.
    if (m_longTerm.hasMemory()) {
        m_geminiApi.embedQuery(prompt, false, true);
    } else {
        m_geminiApi.callGeminiDirectly(m_geminiApi.buildMemoryContext(prompt, QVector<float>()), false);
    }
    qDebug() << "[MessageActions] Перезапись ответа Риксоры для сообщения:" << messageId;
    return true;
}

int MessageActions::editUserMessage(const QString &messageId, const QString &newText)
{
    if (messageId.isEmpty()) return -1;
    if (newText.trimmed().isEmpty()) return -1;
    if (m_geminiApi.isBusy()) return -1;

    const int idx = m_memoryManager.indexOfMessageById(messageId);
    if (idx < 0) return -1;

    const QJsonArray hist = m_memoryManager.exportMessages();
    if (idx >= hist.size()) return -1;
    if (hist[idx].toObject()["role"].toString() != "user") return -1;
    if (hist[idx].toObject()["parts"].toArray()[0].toObject()["text"].toString() == newText) return -1;

    const bool hasResponse = (idx + 1) < hist.size()
        && hist[idx + 1].toObject()["role"].toString() == "model";

    m_memoryManager.replaceMessageById(messageId, newText);

    QStringList staleIds;
    staleIds << messageId;
    if (hasResponse) {
        staleIds += messageIdsFrom(hist, idx + 1);
    }

    for (const QString &staleId : staleIds) {
        m_longTerm.deleteByMsgId(m_chatManager.getCurrentChatId(), staleId);
    }

    if (hasResponse) {
        m_memoryManager.truncateMessagesFrom(idx + 1);
        m_chatManager.saveChat(m_chatManager.getCurrentChatId(), m_memoryManager.exportMessages(),
                               m_memoryManager.getMidTermSummary(),
                               m_memoryManager.getCommittedSummarizedCount());
        // Как и при перезаписи: без долговременной памяти вектор не нужен.
        if (m_longTerm.hasMemory()) {
            m_geminiApi.embedQuery(newText, false, true);
        } else {
            m_geminiApi.callGeminiDirectly(m_geminiApi.buildMemoryContext(newText, QVector<float>()), false);
        }
        qDebug() << "[MessageActions] Промпт изменён, запущена перегенерация ответа:" << messageId;
        return 1;
    }

    m_chatManager.saveChat(m_chatManager.getCurrentChatId(), m_memoryManager.exportMessages(),
                           m_memoryManager.getMidTermSummary(),
                           m_memoryManager.getCommittedSummarizedCount());
    qDebug() << "[MessageActions] Промпт изменён (ответа ещё нет), без перегенерации:" << messageId;
    return 0;
}

void MessageActions::speakMessage(const QString &messageId)
{
    const QString text = m_memoryManager.textById(messageId);
    if (text.trimmed().isEmpty()) return;

    if (m_audioEngine.isPlaying() || !m_speakingMessageId.isEmpty()) {
        m_audioEngine.stop();
    }
    m_speakingMessageId = messageId;
    emit speakingMessageIdChanged();
    m_audioEngine.enqueueTextForTTS(text);
    qDebug() << "[MessageActions] Озвучка сообщения:" << messageId;
}

void MessageActions::clearSpeakingMessage()
{
    if (m_speakingMessageId.isEmpty()) return;
    m_speakingMessageId.clear();
    emit speakingMessageIdChanged();
}

QString MessageActions::speakingMessageId() const
{
    return m_speakingMessageId;
}

void MessageActions::copyToClipboard(const QString &text)
{
    if (text.isEmpty()) return;
    QGuiApplication::clipboard()->setText(text);
    qDebug() << "[MessageActions] Текст сообщения скопирован в буфер обмена.";
}

QString MessageActions::lastMessageId() const
{
    return m_memoryManager.lastMessageId();
}
