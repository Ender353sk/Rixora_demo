#include "memorycontextbuilder.h"
#include "longtermmemory.h"
#include "memorymanager.h"
#include "chatmanager.h"
#include "apiconfigmanager.h"

MemoryContextBuilder::MemoryContextBuilder(LongTermMemory &longTerm,
                                           MemoryManager &memoryManager,
                                           ChatManager &chatManager,
                                           ApiConfigManager &config,
                                           QObject *parent)
    : QObject(parent),
      m_longTerm(longTerm),
      m_memoryManager(memoryManager),
      m_chatManager(chatManager),
      m_config(config)
{
}

QString MemoryContextBuilder::buildMemoryContext(const QString &userText, const QVector<float> &queryVector)
{
    QString ctx;

    if (!m_memoryManager.getMidTermSummary().isEmpty()) {
        ctx += "Главное из текущего диалога: " + m_memoryManager.getMidTermSummary() + "\n";
    }

    QList<MemoryHit> vecHits;
    QList<MemoryHit> lexHits;
    vecHits = m_longTerm.queryHybrid(userText, queryVector, 8, 0.16f, &lexHits);

    const QString activeChatId = m_config.perChatMemory() ? m_chatManager.getCurrentChatId() : QString();
    auto keepOnlyActiveChat = [&activeChatId](QList<MemoryHit> &hits) {
        if (activeChatId.isEmpty()) { hits.clear(); return; }
        int out = 0;
        for (int i = 0; i < hits.size(); ++i) {
            const MemoryHit &h = hits.at(i);
            if (h.chatId == activeChatId) hits[out++] = h;
        }
        hits.resize(out);
    };
    if (m_config.perChatMemory()) {
        keepOnlyActiveChat(vecHits);
        keepOnlyActiveChat(lexHits);
    }

    QList<MemoryHit> memories;
    QHash<QString, int> seen;
    auto pushHit = [&memories, &seen](const MemoryHit &h) {
        const QString key = h.text.trimmed();
        if (key.isEmpty() || seen.contains(key)) return;
        seen.insert(key, 1);
        memories.append(h);
    };
    for (const MemoryHit &h : vecHits) pushHit(h);
    for (const MemoryHit &h : lexHits) pushHit(h);

    if (memories.size() < 8) {
        QList<MemoryHit> recents = m_longTerm.recentFacts(8);
        if (m_config.perChatMemory()) keepOnlyActiveChat(recents);
        for (const MemoryHit &h : recents) {
            if (memories.size() >= 8) break;
            pushHit(h);
        }
    }

    while (memories.size() > 8) memories.removeLast();

    if (!memories.isEmpty()) {
        ctx += "Воспоминания Риксоры (ранее, возможно из других чатов):\n";
        for (const MemoryHit &h : memories) {
            ctx += "- " + h.text.trimmed() + "\n";
        }
    }

    return ctx.trimmed();
}
