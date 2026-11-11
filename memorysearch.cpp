#include "memorysearch.h"
#include "longtermmemorycore.h"
#include "constants.h"
#include "texttokenizer.h"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QPair>
#include <QtMath>
#include <algorithm>

namespace {

MemoryHit hitFor(const LongTermMemoryCore &core, int slot)
{
    MemoryHit hit;
    hit.text = core.textAt(slot);
    hit.kind = "fact";
    hit.chatId = core.chatIdAt(slot);
    hit.msgId = core.msgIdAt(slot);
    hit.ts = core.tsAt(slot);
    return hit;
}

float vectorCosine(const LongTermMemoryCore &core, int slot,
                   const QVector<float> &queryEmbedding)
{
    int dim = 0;
    const float *vec = core.embeddingData(slot, &dim);
    if (!vec || dim != queryEmbedding.size()) return 0.0f;
    return qMax(0.0f, LongTermMemoryCore::cosineSimilarity(queryEmbedding.constData(),
                                                          vec, dim));
}

float slotCosine(const LongTermMemoryCore &core, int a, int b)
{
    int dimA = 0, dimB = 0;
    const float *vecA = core.embeddingData(a, &dimA);
    const float *vecB = core.embeddingData(b, &dimB);
    if (!vecA || !vecB || dimA <= 0 || dimA != dimB) return 0.0f;
    return qMax(0.0f, LongTermMemoryCore::cosineSimilarity(vecA, vecB, dimA));
}

} // namespace

MemorySearch::MemorySearch(BM25Index &index, const LongTermMemoryCore &core,
                           QObject *parent)
    : QObject(parent),
      m_index(index),
      m_core(core)
{
}

QList<MemoryHit> MemorySearch::queryLexical(const QString &queryText,
                                            int topK, float threshold) const
{
    if (m_core.aliveCount() == 0 || queryText.trimmed().isEmpty()) {
        return QList<MemoryHit>();
    }

    const QStringList queryTerms = TextTokenizer::tokenize(queryText);
    if (queryTerms.isEmpty()) return QList<MemoryHit>();

    struct Candidate {
        int slot = 0;
        float total = 0.0f;
    };

    QList<Candidate> candidates;
    for (int slot : m_core.aliveSlots()) {
        const float lex = m_index.lexicalScore(queryTerms, slot);
        const double rec = recencyScore(m_core.tsAt(slot));
        const float total = 0.75f * lex + 0.25f * static_cast<float>(rec);
        if (total < threshold) continue;
        candidates.append({slot, total});
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.total > b.total; });

    QList<MemoryHit> hits;
    for (const Candidate &c : candidates) {
        MemoryHit hit = hitFor(m_core, c.slot);
        hit.lexicalScore = c.total;
        hit.score = c.total;
        hits.append(hit);
        if (hits.size() >= topK) break;
    }
    return hits;
}

QList<MemoryHit> MemorySearch::queryHybrid(const QString &queryText,
                                           const QVector<float> &queryEmbedding,
                                           int topK, float threshold,
                                           QList<MemoryHit> *lexicalOut) const
{
    const int alive = m_core.aliveCount();
    if (alive == 0) return QList<MemoryHit>();

    const bool haveVector = !queryEmbedding.isEmpty();
    if (queryText.trimmed().isEmpty() && !haveVector) return QList<MemoryHit>();

    struct Candidate {
        int slot = 0;
        float vectorScore = 0.0f;
        float lexicalScore = 0.0f;
        double recency = 0.0;
        float coverage = 0.0f;
        float total = 0.0f;
    };

    const QList<int> aliveSlots = m_core.aliveSlots();
    const QStringList queryTerms = TextTokenizer::tokenize(queryText);

    int rareQueryTerms = 0;
    for (const QString &term : queryTerms) {
        if (m_index.docFreq(term) <= 2) ++rareQueryTerms;
    }

    QHash<int, float> lexBySlot;
    lexBySlot.reserve(aliveSlots.size());
    QList<Candidate> lexCandidates;

    for (int slot : aliveSlots) {
        const float lex = m_index.lexicalScore(queryTerms, slot);
        lexBySlot.insert(slot, lex);

        if (lexicalOut) {
            const double rec = recencyScore(m_core.tsAt(slot));
            const float lexTotal = 0.75f * lex + 0.25f * static_cast<float>(rec);
            if (lexTotal >= 0.18f) {
                Candidate c;
                c.slot = slot;
                c.lexicalScore = lex;
                c.recency = rec;
                c.total = lexTotal;
                lexCandidates.append(c);
            }
        }
    }

    QHash<int, float> vecBySlot;
    QList<int> vectorCandidates;

    if (haveVector) {
        if (alive <= 256) {
            vectorCandidates = aliveSlots;
        } else {
            const quint16 querySignature = LongTermMemoryCore::signatureOf(queryEmbedding);

            QVector<QPair<int, int>> ranked;
            ranked.reserve(aliveSlots.size());
            for (int slot : aliveSlots) {
                int dim = 0;
                const float *vec = m_core.embeddingData(slot, &dim);
                if (!vec || dim != queryEmbedding.size()) continue;
                ranked.append({slot, LongTermMemoryCore::signatureDistance(
                                         querySignature, m_core.signatureAt(slot))});
            }

            std::sort(ranked.begin(), ranked.end(),
                      [](const QPair<int, int> &a, const QPair<int, int> &b) {
                          return a.second < b.second;
                      });

            const int keep = qMin(64, ranked.size());
            vectorCandidates.reserve(keep);
            for (int i = 0; i < keep; ++i) vectorCandidates.append(ranked.at(i).first);
        }

        vecBySlot.reserve(vectorCandidates.size());
        for (int slot : vectorCandidates) {
            vecBySlot.insert(slot, vectorCosine(m_core, slot, queryEmbedding));
        }
    }

    QSet<int> candidateSet = m_index.slotsForTerms(queryTerms);
    for (int slot : vectorCandidates) candidateSet.insert(slot);

    QList<Candidate> candidates;
    candidates.reserve(candidateSet.size());

    for (int slot : candidateSet) {
        const float vec = vecBySlot.value(slot, 0.0f);
        const float lex = lexBySlot.value(slot, 0.0f);
        const float coverage = m_index.coverageScore(queryTerms, rareQueryTerms, slot);
        const double rec = recencyScore(m_core.tsAt(slot));

        bool isCandidate = false;
        if (haveVector && vec > 0.20f) isCandidate = true;
        if (lex > 0.22f) isCandidate = true;
        if ((!haveVector || vec <= 0.20f) && coverage >= 0.40f) isCandidate = true;
        if (!isCandidate) continue;

        Candidate c;
        c.slot = slot;
        c.vectorScore = vec;
        c.lexicalScore = lex;
        c.recency = rec;
        c.coverage = coverage;

        if (haveVector) {
            c.total = 0.48f * vec + 0.38f * lex
                    + 0.08f * static_cast<float>(rec) + 0.06f * coverage;
        } else {
            c.total = 0.62f * lex + 0.22f * static_cast<float>(rec) + 0.16f * coverage;
        }
        candidates.append(c);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b) { return a.total > b.total; });

    QList<Candidate> pool;
    for (const Candidate &c : candidates) {
        if (c.total < threshold && c.vectorScore < 0.50f) continue;
        pool.append(c);
        if (pool.size() >= 40) break;
    }

    const double lambda = 0.75;
    QList<int> chosen;
    QList<MemoryHit> hits;

    while (!pool.isEmpty() && chosen.size() < qMin(topK, alive)) {
        int bestIndex = -1;
        float bestScore = -1.0f;

        for (int p = 0; p < pool.size(); ++p) {
            double maxSim = 0.0;
            for (int ci = 0; ci < chosen.size(); ++ci) {
                const double sim = slotCosine(m_core, pool.at(p).slot, chosen.at(ci));
                if (sim > maxSim) maxSim = sim;
            }
            const double mmr = lambda * static_cast<double>(pool.at(p).total)
                             - (1.0 - lambda) * maxSim;
            if (mmr > bestScore) {
                bestScore = static_cast<float>(mmr);
                bestIndex = p;
            }
        }

        if (bestIndex < 0) break;

        const Candidate pick = pool.takeAt(bestIndex);
        chosen.append(pick.slot);

        MemoryHit hit = hitFor(m_core, pick.slot);
        hit.vectorScore = pick.vectorScore;
        hit.lexicalScore = pick.lexicalScore;
        hit.score = qMax(0.0f, bestScore);
        hits.append(hit);
    }

    if (lexicalOut) {
        std::sort(lexCandidates.begin(), lexCandidates.end(),
                  [](const Candidate &a, const Candidate &b) { return a.total > b.total; });

        for (const Candidate &c : lexCandidates) {
            if (lexicalOut->size() >= topK) break;
            MemoryHit hit = hitFor(m_core, c.slot);
            hit.lexicalScore = c.total;
            hit.score = c.total;
            lexicalOut->append(hit);
        }
    }

    return hits;
}

QList<MemoryHit> MemorySearch::recentFacts(int n) const
{
    QList<MemoryHit> out;
    if (n <= 0) return out;

    QList<int> ordered = m_core.aliveSlots();
    if (ordered.isEmpty()) return out;

    std::sort(ordered.begin(), ordered.end(), [this](int a, int b) {
        return m_core.tsAt(a) > m_core.tsAt(b);
    });

    for (int slot : ordered) {
        out.append(hitFor(m_core, slot));
        if (out.size() >= n) break;
    }
    return out;
}

float MemorySearch::cosineSimilarity(const QVector<float> &vecA, const QVector<float> &vecB)
{
    if (vecA.size() != vecB.size() || vecA.isEmpty()) return 0.0f;
    return LongTermMemoryCore::cosineSimilarity(vecA.constData(), vecB.constData(),
                                               vecA.size());
}

double MemorySearch::recencyScore(qint64 ts)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double msPerDay = 86400.0 * 1000.0;
    double ageDays = double(now - ts) / msPerDay;
    if (ageDays < 0.0) ageDays = 0.0;
    return qExp(-ageDays / RECENCY_HALF_LIFE_DAYS);
}
