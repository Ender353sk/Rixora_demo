#include "geminiclient.h"
#include "apiconfigmanager.h"
#include "chatmanager.h"
#include "constants.h"
#include "geminiresponseparser.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

GeminiClient::GeminiClient(ApiConfigManager &config, ChatManager &chatManager, QObject *parent)
    : QObject(parent), m_config(config), m_chatManager(chatManager)
{
}

void GeminiClient::abortCurrentReply()
{
    if (m_embedReply && m_embedReply->isRunning()) {
        m_embedReply->abort();
    }
    if (m_currentReply && m_currentReply->isRunning()) {
        m_currentReply->abort();
    }
}

bool GeminiClient::isBusy() const
{
    return (m_currentReply && m_currentReply->isRunning())
        || (m_embedReply && m_embedReply->isRunning());
}

void GeminiClient::resetStreamState()
{
    m_streamBuffer.clear();
    m_rawBody.clear();
    m_streamText.clear();
    m_blockReason.clear();
    m_streamStarted = false;
}

// Разбор SSE-потока построчно. Каждая строка "data: {json}" — одно событие
// streamGenerateContent, несущее дельту текста (терминальное событие приходит
// с пустым parts; [DONE] у Gemini нет). Неполные байты остаются в буфере.
void GeminiClient::consumeStreamBytes(const QByteArray &bytes, bool flush)
{
    m_rawBody.append(bytes);
    m_streamBuffer.append(bytes);

    int idx;
    while ((idx = m_streamBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_streamBuffer.left(idx);
        m_streamBuffer.remove(0, idx + 1);
        handleStreamLine(line);
    }

    if (flush && !m_streamBuffer.isEmpty()) {
        handleStreamLine(m_streamBuffer);
        m_streamBuffer.clear();
    }
}

void GeminiClient::handleStreamLine(const QByteArray &line)
{
    const QByteArray trimmed = line.trimmed();
    if (!trimmed.startsWith("data:")) return;  // пустые строки, \r, комментарии

    const QByteArray payload = trimmed.mid(5).trimmed();
    if (payload.isEmpty() || payload == "[DONE]") return;
    handleStreamPayload(payload);
}

void GeminiClient::handleStreamPayload(const QByteArray &payload)
{
    // Промпт уже заблокирован — последующие события не стримим.
    if (!m_blockReason.isEmpty()) return;

    QJsonParseError parseError{};
    const QJsonObject root = QJsonDocument::fromJson(payload, &parseError).object();
    if (parseError.error != QJsonParseError::NoError) return;

    const QString blocked = root["promptFeedback"].toObject()["blockReason"].toString();
    if (!blocked.isEmpty()) {
        m_blockReason = blocked;
        return;
    }

    const QJsonArray candidates = root["candidates"].toArray();
    if (candidates.isEmpty()) return;

    const QJsonObject candidate = candidates.at(0).toObject();
    const QString finish = candidate["finishReason"].toString();
    if (!finish.isEmpty() && finish != "STOP" && finish != "MAX_TOKENS") {
        m_blockReason = finish;
        return;
    }

    // Склеиваем текстовые части, пропуская суммары размышлений (thought):
    // в финальном не-стриминговом ответе их тоже не было бы.
    const QJsonArray parts = candidate["content"].toObject()["parts"].toArray();
    QString text;
    const QJsonArray partsList = parts;
    for (const QJsonValue &value : partsList) {
        const QJsonObject part = value.toObject();
        if (part["thought"].toBool()) continue;
        text += part["text"].toString();
    }
    if (text.isEmpty()) return;  // терминальное событие parts:[] — просто конец

    if (!m_streamStarted) {
        m_streamStarted = true;
        emit streamStarted();
    }
    m_streamText += text;
    emit responseChunk(text);
}

void GeminiClient::callGeminiDirectly(const QString &systemContext,
                                      const QJsonArray &history,
                                      bool voiceEnabled)
{
    // Чат, в котором задан вопрос: если пользователь переключится до прихода
    // ответа, ответ должен уйти в исходный чат, а не в текущий.
    const QString requestChatId = m_chatManager.getCurrentChatId();

    const QString apiKey = m_config.getGeminiKey();
    QUrl aiUrl(QString(GEMINI_API_BASE) + GEMINI_CHAT_MODEL + ":streamGenerateContent?alt=sse&key=" + apiKey);
    QNetworkRequest aiReq(aiUrl);
    aiReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject aiJson;
    aiJson["contents"] = history;

    QString targetLangName = languageCodeToName(m_config.getLanguage());

    QString basePrompt = QString::fromUtf8(
        "Ты — виртуальный ИИ-ассистент по имени Риксора (ударение всегда на 'о'), отвечай кратко и нейтрально, "
        "но как живой человек. Приоритетный язык для ответов в этом чате: %1 "
        "(однако если пользователь пишет тебе на другом языке или просит сменить язык — всегда отвечай на его языке). "
        "Не используй эмодзи и спецсимволы, но если ты пишешь радостный "
        "или грустный ответ на русском или украинском, можешь очень редко (учитывая историю чата) использовать "
        "скобки в конце предложения (сразу после последней буквы), чтобы передать радость или грусть. "
        "В разговоре с пользователем НИ В КОЕМ СЛУЧАЕ не упоминай информацию, не связанную с темой текущего "
        "разговора, даже если пользователь начинает разговор с нейтральных тем вроде «как ты?», НИКОГДА не "
        "упоминай в разговоре с пользователем информацию из прошлого на темы, о которых сам пользователь не "
        "упомянул в текущем сообщении.").arg(targetLangName);

    QString fullSystemInstruction = basePrompt;
    if (!systemContext.isEmpty()) {
        fullSystemInstruction += "\n\n" + QString::fromUtf8(
            "Системная память (это твои воспоминания из прошлых разговоров и сохранённые факты о пользователе — "
            "применяй их к текущему вопросу, если это уместно):\n") + systemContext;
    }

    QJsonObject sysInst;
    sysInst["parts"] = QJsonArray{QJsonObject{{"text", fullSystemInstruction}}};
    aiJson["systemInstruction"] = sysInst;

    QNetworkReply *aiReply = m_networkAccessManager.post(aiReq, QJsonDocument(aiJson).toJson());
    m_currentReply = aiReply;
    const quint64 generation = ++m_generation;
    resetStreamState();

    // Дельты читаем прямо из сокета, не дожидаясь конца ответа —
    // ради этого и затевается стриминг.
    connect(aiReply, &QNetworkReply::readyRead, this, [this, aiReply, generation]() {
        if (generation != m_generation) return;
        consumeStreamBytes(aiReply->readAll(), false);
    });

    connect(aiReply, &QNetworkReply::finished, this, [this, aiReply, voiceEnabled, generation, requestChatId]() {
        if (m_currentReply == aiReply) m_currentReply = nullptr;
        aiReply->deleteLater();

        if (generation != m_generation) return;

        if (aiReply->error() == QNetworkReply::OperationCanceledError) return;

        if (aiReply->error() != QNetworkReply::NoError) {
            emit errorReceived("Network error: " + aiReply->errorString());
            return;
        }

        // Дочитываем хвост потока (последнее событие может прийти без \n).
        consumeStreamBytes(aiReply->readAll(), true);

        if (!m_blockReason.isEmpty()) {
            emit errorReceived("Rixora couldn't answer: " + m_blockReason);
            return;
        }

        // Основной путь — накопленные дельты. Fallback на разбор сырого
        // тела нужен, только если поток не принёс ни одного чанка
        // (например, сервер ответил обычным JSON, а не SSE).
        QString aiResponse = m_streamText;
        if (aiResponse.isEmpty()) {
            QString blockReason;
            if (!parseGeminiText(m_rawBody, &aiResponse, &blockReason)) {
                emit errorReceived("Rixora couldn't answer: " + blockReason);
                return;
            }
        }

        if (requestChatId != m_chatManager.getCurrentChatId()) {
            emit chatSwitchedDuringRequest(requestChatId, aiResponse);
            return;
        }

        emit responseReceived(aiResponse, voiceEnabled);
    });
}

void GeminiClient::embedQuery(const QString &text, bool voiceEnabled, bool allowRetry)
{
    const QString apiKey = m_config.getGeminiKey();
    QUrl embedUrl(QString(GEMINI_API_BASE) + GEMINI_EMBED_MODEL + ":embedContent?key=" + apiKey);
    QNetworkRequest embedReq(embedUrl);
    embedReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject embedContent; embedContent["parts"] = QJsonArray{QJsonObject{{"text", text}}};
    QJsonObject embedJson; embedJson["model"] = QString("models/") + GEMINI_EMBED_MODEL; embedJson["content"] = embedContent;

    QNetworkReply *embedReply = m_networkAccessManager.post(embedReq, QJsonDocument(embedJson).toJson());
    m_embedReply = embedReply;
    const quint64 generation = ++m_generation;

    connect(embedReply, &QNetworkReply::finished, this, [this, embedReply, text, voiceEnabled, allowRetry, generation]() {
        if (m_embedReply == embedReply) m_embedReply = nullptr;
        embedReply->deleteLater();

        if (generation != m_generation) return;

        if (embedReply->error() == QNetworkReply::OperationCanceledError) return;

        QVector<float> queryVector;
        if (embedReply->error() != QNetworkReply::NoError) {
            // Реальную причину отбрасывать нельзя: иначе пользователь видит
            // «Empty embedding» вместо, например, 404 по имени модели.
            emit errorReceived("Embedding request failed: "
                               + embedReply->errorString() + " — "
                               + describeGeminiError(embedReply->readAll()));
            return;
        }

        QVector<QVector<float>> embeddings;
        if (parseEmbeddingResponse(embedReply->readAll(), &embeddings) && !embeddings.isEmpty()) {
            queryVector = embeddings.first();
        }

        // Повтор ровно один раз: рекурсия идёт с allowRetry = false.
        if (queryVector.isEmpty() && allowRetry) {
            embedQuery(text, voiceEnabled, false);
            return;
        }

        if (queryVector.isEmpty()) {
            emit errorReceived("Empty embedding, retry failed");
            return;
        }

        emit embedCompleted(text, queryVector, voiceEnabled);
    });
}

QString GeminiClient::languageCodeToName(const QString &code) const
{
    if (code == "en") return "English";
    if (code == "es") return "Español";
    if (code == "ko") return "Korean";
    if (code == "de") return "Deutsch";
    if (code == "uk") return "Ukrainian";
    if (code == "fr") return "Français";
    if (code == "ja") return "Japanese";
    return "Russian";
}
