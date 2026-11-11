#include "geminiapi.h"
#include "geminiclient.h"
#include "memorycontextbuilder.h"
#include "constants.h"
#include "memorymanager.h"
#include "chatmanager.h"
#include "longtermmemory.h"

GeminiApi::GeminiApi(ApiConfigManager &config,
                     MemoryManager &memory,
                     ChatManager &chat,
                     LongTermMemory &longTerm,
                     QObject *parent)
    : QObject(parent),
      m_memoryManager(memory),
      m_client(std::make_unique<GeminiClient>(config, chat)),
      m_contextBuilder(std::make_unique<MemoryContextBuilder>(longTerm, memory, chat, config))
{
    connect(m_client.get(), &GeminiClient::streamStarted, this, &GeminiApi::streamStarted);
    connect(m_client.get(), &GeminiClient::responseChunk, this, &GeminiApi::responseChunk);
    connect(m_client.get(), &GeminiClient::responseReceived, this, &GeminiApi::responseReceived);
    connect(m_client.get(), &GeminiClient::chatSwitchedDuringRequest, this, &GeminiApi::chatSwitchedDuringRequest);
    connect(m_client.get(), &GeminiClient::errorReceived, this, &GeminiApi::errorReceived);
    connect(m_client.get(), &GeminiClient::embedCompleted, this, [this](const QString &text, const QVector<float> &embedding, bool voiceEnabled) {
        QString systemContext = m_contextBuilder->buildMemoryContext(text, embedding);
        m_client->callGeminiDirectly(systemContext, m_memoryManager.getRecentHistory(SHORT_TERM_WINDOW), voiceEnabled);
    });
}

GeminiApi::~GeminiApi() {}

void GeminiApi::callGeminiDirectly(const QString &systemContext, bool voiceEnabled)
{
    m_client->callGeminiDirectly(systemContext, m_memoryManager.getRecentHistory(SHORT_TERM_WINDOW), voiceEnabled);
}

void GeminiApi::embedQuery(const QString &text, bool voiceEnabled, bool allowRetry)
{
    m_client->embedQuery(text, voiceEnabled, allowRetry);
}

QString GeminiApi::buildMemoryContext(const QString &userText, const QVector<float> &queryVector)
{
    return m_contextBuilder->buildMemoryContext(userText, queryVector);
}

void GeminiApi::abortCurrentReply()
{
    m_client->abortCurrentReply();
}

bool GeminiApi::isBusy() const
{
    // Клиент уже мог быть уничтожен (порядок разрушения фасадов) — не падать.
    return m_client && m_client->isBusy();
}
