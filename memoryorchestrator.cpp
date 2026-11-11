#include "memoryorchestrator.h"
#include "memorylang.h"
#include "constants.h"
#include "geminiresponseparser.h"
#include "memorymanager.h"
#include "chatmanager.h"
#include "longtermmemory.h"
#include "apiconfigmanager.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

MemoryOrchestrator::MemoryOrchestrator(LongTermMemory &longTerm,
                                       MemoryManager &memoryManager,
                                       ChatManager &chatManager,
                                       ApiConfigManager &config,
                                       QNetworkAccessManager &nam,
                                       QObject *parent)
    : QObject(parent),
      m_longTerm(longTerm),
      m_memoryManager(memoryManager),
      m_chatManager(chatManager),
      m_config(config),
      m_nam(nam)
{
}

void MemoryOrchestrator::checkAndRunBackgroundMemoryTasks()
{
    if (m_memoryManager.needsSummarization()) {
        const QString chatId = m_chatManager.getCurrentChatId();
        const int countBase = m_memoryManager.getCommittedSummarizedCount();
        QString chunkText = m_memoryManager.getSummarizeChunkText();
        const QString prevSummary = m_memoryManager.getMidTermSummary();

        QString prompt;
        const QString langNote = "РџРёС€Рё РІС‹Р¶РёРјРєСѓ РЅР° СЏР·С‹РєРµ: " + memoryLanguageName(m_config.getLanguage()) + ".\n";
        if (prevSummary.isEmpty()) {
            prompt = "РўС‹ - Р°СЃСЃРёСЃС‚РµРЅС‚ РїР°РјСЏС‚Рё. РЎРґРµР»Р°Р№ РѕС‡РµРЅСЊ РєСЂР°С‚РєСѓСЋ РѕР±С‰СѓСЋ РІС‹Р¶РёРјРєСѓ СЌС‚РѕРіРѕ РєСѓСЃРєР° "
                     "РґРёР°Р»РѕРіР°: РѕРґРЅР°-РґРІРµ РєРѕСЂРѕС‚РєРёРµ С„СЂР°Р·С‹ Р±РµР· РЅСѓРјРµСЂР°С†РёРё, РјР°РєСЃРёРјСѓРј 500 СЃРёРјРІРѕР»РѕРІ, "
                     "С‚РѕР»СЊРєРѕ РІР°Р¶РЅРѕРµ Рѕ РїРѕР»СЊР·РѕРІР°С‚РµР»Рµ Рё РµРіРѕ РјРёСЂРµ.\n" + langNote + "\n" + chunkText;
        } else {
            prompt = "РўС‹ - Р°СЃСЃРёСЃС‚РµРЅС‚ РїР°РјСЏС‚Рё. РќРёР¶Рµ РґР°РЅС‹ С‚РµРєСѓС‰РёР№ РѕР±С‰РёР№ РєРѕРЅСЃРїРµРєС‚ РґРёР°Р»РѕРіР° Рё РЅРѕРІС‹Р№ РєСѓСЃРѕРє "
                     "РїРµСЂРµРїРёСЃРєРё. РћР±РЅРѕРІРё РєРѕРЅСЃРїРµРєС‚: РѕР±СЉРµРґРёРЅРё РµРіРѕ РІ РћР”РќРЈ РѕС‡РµРЅСЊ РєСЂР°С‚РєСѓСЋ СЃРїР»РѕС€РЅСѓСЋ РІС‹Р¶РёРјРєСѓ "
                     "Р±РµР· РЅСѓРјРµСЂР°С†РёРё, РјР°РєСЃРёРјСѓРј 500 СЃРёРјРІРѕР»РѕРІ. РЎРѕС…СЂР°РЅРё СЃР°РјРѕРµ РІР°Р¶РЅРѕРµ, РґРѕР±Р°РІСЊ РЅРѕРІСѓСЋ "
                     "РёРЅС„РѕСЂРјР°С†РёСЋ РёР· РЅРѕРІРѕРіРѕ РєСѓСЃРєР°.\n" + langNote + "\nРўРµРєСѓС‰РёР№ РєРѕРЅСЃРїРµРєС‚:\n"
                     + prevSummary.left(900) + "\n\nРќРѕРІС‹Р№ РєСѓСЃРѕРє РґРёР°Р»РѕРіР°:\n" + chunkText;
        }

        QUrl aiUrl(QString(GEMINI_API_BASE) + GEMINI_CHAT_MODEL + ":generateContent?key=" + m_config.getGeminiKey());
        QNetworkRequest req(aiUrl);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject json;
        json["contents"] = QJsonArray{QJsonObject{{"role", "user"}, {"parts", QJsonArray{QJsonObject{{"text", prompt}}}}}};

        QNetworkReply *reply = m_nam.post(req, QJsonDocument(json).toJson());

        connect(reply, &QNetworkReply::finished, this, [this, reply, chatId, countBase]() {
            reply->deleteLater();
            if (reply->error() == QNetworkReply::NoError) {
                QString newSum;
                QString blockReason;
                if (!parseGeminiText(reply->readAll(), &newSum, &blockReason)) {
                    if (chatId == m_chatManager.getCurrentChatId()) {
                        m_memoryManager.rollbackSummarization();
                        emit errorOccurred("РќРµ СѓРґР°Р»РѕСЃСЊ РѕР±РЅРѕРІРёС‚СЊ РїР°РјСЏС‚СЊ РґРёР°Р»РѕРіР°: " + blockReason);
                    }
                    qDebug() << "[Memory] РЎРІРѕРґРєР° РЅРµ РїРѕР»СѓС‡РµРЅР°:" << blockReason;
                    return;
                }
                if (newSum.size() > 700) newSum = newSum.left(697) + "...";

                if (chatId != m_chatManager.getCurrentChatId()) {
                    if (m_chatManager.chatExists(chatId)) {
                        QJsonArray msgs = m_chatManager.peekChatMessages(chatId, 100000, 0);
                        int summarizedAfter = countBase + SUMMARIZATION_CHUNK_SIZE;
                        int unsummarized = qMax(0, msgs.size() - summarizedAfter);
                        int keepTotal = TRIM_BUFFER_SIZE + unsummarized;
                        int removeFromStart = qMax(0, msgs.size() - keepTotal);
                        for (int i = 0; i < removeFromStart; ++i) msgs.removeAt(0);
                        m_chatManager.saveChat(chatId, msgs, newSum, summarizedAfter);
                        qDebug() << "[Memory] РЎРІРѕРґРєР° СЃРѕС…СЂР°РЅРµРЅР° РІ РёСЃС…РѕРґРЅС‹Р№ С‡Р°С‚ РїРѕСЃР»Рµ РїРµСЂРµРєР»СЋС‡РµРЅРёСЏ:" << chatId;
                    }
                    return;
                }

                m_memoryManager.commitSummaryAndTrim(newSum);
                m_chatManager.saveChat(chatId, m_memoryManager.exportMessages(),
                                       m_memoryManager.getMidTermSummary(),
                                       m_memoryManager.getCommittedSummarizedCount());
            } else {
                if (chatId == m_chatManager.getCurrentChatId()) {
                    m_memoryManager.rollbackSummarization();
                }
            }
        });
    }

    maybeRunFactExtraction();
}

void MemoryOrchestrator::maybeRunFactExtraction()
{
    const QDateTime now = QDateTime::currentDateTime();
    if (!m_longTerm.canRunExtraction()) return;

    if (!m_backfillDone) {
        m_backfillDone = true;
        if (!m_config.perChatMemory() && m_longTerm.count() < BACKFILL_THRESHOLD) {
            backfillLongTermMemory(now, 4);
        }
        return;
    }

    ++m_messagesSinceExtraction;
    if (m_messagesSinceExtraction < MESSAGES_BEFORE_EXTRACTION) return;

    if (m_lastExtractionRun.isValid() && m_lastExtractionRun.secsTo(now) < 8) return;

    if (!m_config.perChatMemory() && m_messagesSinceExtraction % BACKFILL_INTERVAL == 0) {
        backfillLongTermMemory(now, 1);
        m_messagesSinceExtraction = 0;
        m_lastExtractionRun = now;
        return;
    }

    static const int CHUNK_SIZE = EXTRACTION_CHUNK_SIZE;
    QJsonArray hist = m_memoryManager.exportMessages();
    if (hist.isEmpty()) return;

    QString chunk;
    QStringList chunkMsgIds;
    const QString userLabel = userRoleLabel(m_config.getLanguage());
    const QString assistantLabel = assistantRoleLabel(m_config.getLanguage());
    int start = qMax(0, hist.size() - CHUNK_SIZE);
    for (int i = start; i < hist.size(); ++i) {
        QJsonObject msg = hist[i].toObject();
        QString role = msg["role"].toString() == "user" ? userLabel : assistantLabel;
        QString t = msg["parts"].toArray()[0].toObject()["text"].toString();
        if (t.trimmed().isEmpty()) continue;
        chunk += role + ": " + t + "\n";
        chunkMsgIds << msg["id"].toString();
    }
    if (chunk.trimmed().isEmpty()) return;

    extractFactsFromChunk(chunk, m_chatManager.getCurrentChatId(), now, chunkMsgIds);
}

void MemoryOrchestrator::backfillLongTermMemory(const QDateTime &now, int maxChats)
{
    m_backfillIndex = m_longTerm.backfillIndex();
    m_scannedChatMsgs = m_longTerm.scannedChatMsgs();

    const QJsonArray list = m_chatManager.getChatListOldestFirst();
    const int total = list.size();
    if (total == 0) return;

    const QString userLabel = userRoleLabel(m_config.getLanguage());
    const QString assistantLabel = assistantRoleLabel(m_config.getLanguage());

    int processed = 0;
    for (int k = 0; k < qMin(total, maxChats); ++k) {
        if (!m_longTerm.canRunExtraction()) break;

        const int idx = m_backfillIndex % total;
        m_backfillIndex = (m_backfillIndex + 1) % total;

        const QJsonObject meta = list[idx].toObject();
        const QString id = meta["id"].toString();

        const int totalMsgs = meta["messageCount"].toInt();
        const int scannedFromEnd = m_scannedChatMsgs.value(id, 0);
        const int remaining = totalMsgs - scannedFromEnd;
        if (remaining <= 0) continue;

        const int batchSize = qMin(EXTRACTION_CHUNK_SIZE, remaining);
        const QJsonArray msgs = m_chatManager.peekChatMessages(id, batchSize, scannedFromEnd);
        if (msgs.isEmpty()) continue;

        QString chunk;
        QStringList chunkMsgIds;
        for (const QJsonValue &mv : msgs) {
            QJsonObject msg = mv.toObject();
            QString role = msg["role"].toString() == "user" ? userLabel : assistantLabel;
            QString t = msg["parts"].toArray()[0].toObject()["text"].toString();
            if (t.trimmed().isEmpty()) continue;
            chunk += role + ": " + t + "\n";
            chunkMsgIds << msg["id"].toString();
        }
        if (chunk.trimmed().isEmpty()) {
            m_scannedChatMsgs[id] = scannedFromEnd + batchSize;
            continue;
        }

        m_scannedChatMsgs[id] = scannedFromEnd + msgs.size();
        ++processed;
        extractFactsFromChunk(chunk, id, now, chunkMsgIds);
    }

    m_longTerm.saveBackfillCursor(m_backfillIndex, m_scannedChatMsgs);

    if (processed > 0) {
        qDebug() << "[Memory] Р‘СЌРєР°С„РёР»Р» РґРѕР»РіРѕСЃСЂРѕС‡РЅРѕР№ РїР°РјСЏС‚Рё РїРѕ СЃСѓС‰РµСЃС‚РІСѓСЋС‰РёРј С‡Р°С‚Р°Рј:" << processed;
    }
}

void MemoryOrchestrator::extractFactsFromChunk(const QString &chunk, const QString &chatId,
                                               const QDateTime &now, const QStringList &msgIds)
{
    const QString prompt = "РўС‹ - Р°СЃСЃРёСЃС‚РµРЅС‚ РїР°РјСЏС‚Рё. Р’С‹РґРµР»Рё РёР· СЌС‚РѕРіРѕ РґРёР°Р»РѕРіР° РўРћР›Р¬РљРћ РґРѕР»РіРѕРІСЂРµРјРµРЅРЅС‹Рµ "
        "С„Р°РєС‚С‹ Рѕ РїРѕР»СЊР·РѕРІР°С‚РµР»Рµ РёР»Рё Рѕ РµРіРѕ РјРёСЂРµ, РєРѕС‚РѕСЂС‹Рµ РР-РєРѕРјРїР°РЅСЊРѕРЅ РґРѕР»Р¶РµРЅ РїРѕРјРЅРёС‚СЊ РІРµС‡РЅРѕ: "
        "РёРјСЏ, РІРѕР·СЂР°СЃС‚, РїСЂРѕС„РµСЃСЃРёСЋ, СѓРІР»РµС‡РµРЅРёСЏ, РїСЂРёРІС‹С‡РєРё, РїР»Р°РЅС‹, РІР°Р¶РЅС‹Рµ СЃРѕР±С‹С‚РёСЏ, РѕС‚РЅРѕС€РµРЅРёСЏ, РІРєСѓСЃС‹, "
        "РїСЂРµРґРїРѕС‡С‚РµРЅРёСЏ. РќР• РІРєР»СЋС‡Р°Р№ РїСЂРёРІРµС‚СЃС‚РІРёСЏ, РѕР±С‰РёРµ С„СЂР°Р·С‹, РІРѕРїСЂРѕСЃС‹ РёР»Рё РѕРґРЅРѕСЂР°Р·РѕРІС‹Рµ СЂРµРїР»РёРєРё. "
        "РљР°Р¶РґС‹Р№ С„Р°РєС‚ - РѕРґРЅР° РєРѕСЂРѕС‚РєР°СЏ СЃРѕРґРµСЂР¶Р°С‚РµР»СЊРЅР°СЏ С„СЂР°Р·Р° РѕС‚РґРµР»СЊРЅРѕР№ СЃС‚СЂРѕРєРѕР№. Р‘РµР· РЅСѓРјРµСЂР°С†РёРё, "
        "Р±РµР· С‚РёСЂРµ Рё РјР°СЂРєРµСЂРѕРІ. РњР°РєСЃРёРјСѓРј 8 С„Р°РєС‚РѕРІ.\n"
        "Р’РђР–РќРћ: Р·Р°РїРёСЃС‹РІР°Р№ РєР°Р¶РґС‹Р№ С„Р°РєС‚ РЅР° С‚РѕРј Р¶Рµ СЏР·С‹РєРµ, РЅР° РєРѕС‚РѕСЂРѕРј РѕРЅ РїСЂРѕР·РІСѓС‡Р°Р» РІ РґРёР°Р»РѕРіРµ, "
        "РЅРµ РїРµСЂРµРІРѕРґРё РµРіРѕ. РЇР·С‹Рє Р±РѕС‚Р° СЃРµР№С‡Р°СЃ: " + memoryLanguageName(m_config.getLanguage()) +
        ". Р¤Р°РєС‚ РґРѕР»Р¶РµРЅ Р·РІСѓС‡Р°С‚СЊ С‚Р°Рє, РєР°Рє РµРіРѕ РїРѕС‚РѕРј РЅР°Р№РґС‘С‚ РїРѕРёСЃРє РїРѕ СЃРјС‹СЃР»Сѓ РІ РґСЂСѓРіРѕРј С‡Р°С‚Рµ.\n\nР”РёР°Р»РѕРі:\n" + chunk;

    QUrl aiUrl(QString(GEMINI_API_BASE) + GEMINI_CHAT_MODEL + ":generateContent?key=" + m_config.getGeminiKey());
    QNetworkRequest req(aiUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["contents"] = QJsonArray{QJsonObject{{"role", "user"}, {"parts", QJsonArray{QJsonObject{{"text", prompt}}}}}};

    QNetworkReply *reply = m_nam.post(req, QJsonDocument(json).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, chatId, now, msgIds]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qDebug() << "[Memory] РР·РІР»РµС‡РµРЅРёРµ С„Р°РєС‚РѕРІ РЅРµ СѓРґР°Р»РѕСЃСЊ, Р±СѓРґРµС‚ РїРѕРІС‚РѕСЂРµРЅРѕ РїРѕР·Р¶Рµ.";
            return;
        }

        QString rawText;
        QString blockReason;
        if (!parseGeminiText(reply->readAll(), &rawText, &blockReason)) {
            qDebug() << "[Memory] РР·РІР»РµС‡РµРЅРёРµ С„Р°РєС‚РѕРІ РЅРµ СѓРґР°Р»РѕСЃСЊ:" << blockReason;
            return;
        }
        QStringList facts = extractFactsList(rawText);
        if (facts.isEmpty()) {
            m_lastExtractionRun = now;
            m_messagesSinceExtraction = 0;
            m_longTerm.noteExtractionRun();
            return;
        }

        QUrl embedUrl(QString(GEMINI_API_BASE) + GEMINI_EMBED_MODEL + ":batchEmbedContents?key=" + m_config.getGeminiKey());
        QNetworkRequest embedReq(embedUrl);
        embedReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject batchJson;
        batchJson["model"] = QString("models/") + GEMINI_EMBED_MODEL;
        QJsonArray requests;
        QVector<QString> factMsgIds;
        factMsgIds.reserve(facts.size());
        for (int i = 0; i < facts.size(); ++i) {
            const QString &f = facts[i];
            factMsgIds << (i < msgIds.size() ? msgIds[i] : QString());
            QJsonObject rq;
            rq["model"] = QString("models/") + GEMINI_EMBED_MODEL;
            rq["content"] = QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", f}}}}};
            requests.append(rq);
        }
        batchJson["requests"] = requests;

        QNetworkReply *embedReply = m_nam.post(embedReq, QJsonDocument(batchJson).toJson());

        connect(embedReply, &QNetworkReply::finished, this,
                [this, embedReply, facts, factMsgIds, chatId, now]() {
            embedReply->deleteLater();

            QVector<QVector<float>> embeddings;
            if (embedReply->error() != QNetworkReply::NoError) {
                // Р¤Р°РєС‚С‹ Р±РµР· РІРµРєС‚РѕСЂРѕРІ Р±РµСЃРїРѕР»РµР·РЅС‹ РґР»СЏ СЃРµРјР°РЅС‚РёС‡РµСЃРєРѕРіРѕ РїРѕРёСЃРєР°, Р°
                // РІС‹Р·РѕРІ noteExtractionRun() РЅРёР¶Рµ РїРѕС‚СЂР°С‚РёР» Р±С‹ СЃСѓС‚РѕС‡РЅСѓСЋ РєРІРѕС‚Сѓ.
                // РџСЂРµСЂС‹РІР°РµРј РІСЃС‘ Рё РѕСЃС‚Р°РІР»СЏРµРј РёР·РІР»РµС‡РµРЅРёРµ РЅР° РїРѕС‚РѕРј.
                qDebug() << "[Memory] Р­РјР±РµРґРґРёРЅРіРё С„Р°РєС‚РѕРІ РЅРµ РїРѕР»СѓС‡РµРЅС‹:"
                         << embedReply->errorString()
                         << "вЂ”" << describeGeminiError(embedReply->readAll());
                return;
            }

            if (!parseEmbeddingResponse(embedReply->readAll(), &embeddings)) {
                qDebug() << "[Memory] РћС‚РІРµС‚ СЃ СЌРјР±РµРґРґРёРЅРіР°РјРё С„Р°РєС‚РѕРІ РЅРµ СЂР°СЃРїРѕР·РЅР°РЅ.";
                return;
            }

            if (embeddings.size() != facts.size()) {
                qDebug() << "[Memory] Р§РёСЃР»Рѕ СЌРјР±РµРґРґРёРЅРіРѕРІ РЅРµ СЃРѕРІРїР°Р»Рѕ СЃ С‡РёСЃР»РѕРј С„Р°РєС‚РѕРІ:"
                         << embeddings.size() << "РїСЂРѕС‚РёРІ" << facts.size();
                return;
            }

            if (!m_chatManager.chatExists(chatId)) {
                m_longTerm.noteExtractionRun();
                qDebug() << "[Memory] РР·РІР»РµС‡РµРЅРёРµ РїСЂРѕРїСѓС‰РµРЅРѕ вЂ” С‡Р°С‚ СѓРґР°Р»С‘РЅ РІРѕ РІСЂРµРјСЏ Р·Р°РїСЂРѕСЃР°:" << chatId;
                return;
            }

            int added = 0;
            QVector<QString> texts(facts.begin(), facts.end());
            m_longTerm.addFacts(texts, embeddings, chatId, factMsgIds, &added);

            m_lastExtractionRun = now;
            m_messagesSinceExtraction = 0;
            m_longTerm.noteExtractionRun();
            qDebug() << "[Memory] РР·РІР»РµС‡РµРЅРѕ С„Р°РєС‚РѕРІ:" << facts.size() << ", РґРѕР±Р°РІР»РµРЅРѕ РЅРѕРІС‹С…:" << added;
        });
    });
}

QStringList MemoryOrchestrator::extractFactsList(const QString &raw) const
{
    QStringList out;
    for (QString line : raw.split('\n')) {
        line = line.trimmed();
        while (line.startsWith("-") || line.startsWith("вЂў") || line.startsWith("*")
               || line.startsWith("вЂ“") || line.startsWith("вЂ”")) {
            line = line.mid(1).trimmed();
        }
        while (!line.isEmpty() && line[0].isDigit()) {
            line = line.mid(1).trimmed();
            if (line.startsWith(".") || line.startsWith(")") || line.startsWith("(")) {
                line = line.mid(1).trimmed();
                break;
            }
        }
        if (line.size() < 8) continue;
        if (line.size() > 240) line = line.left(237) + "...";
        out << line;
        if (out.size() >= 8) break;
    }
    return out;
}
