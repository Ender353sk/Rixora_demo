#include "bm25index.h"
#include "texttokenizer.h"

#include <QtMath>
#include <algorithm>

void BM25Index::addDocument(int slot, const QString &text)
{
    removeDocument(slot);

    QHash<QString, int> terms;
    QStringList order;
    TextTokenizer::tokenizeInto(text, terms, &order);

    const int docLen = qMax(1, order.size());
    QSet<QString> docTerms;
    docTerms.reserve(terms.size());

    for (auto it = terms.constBegin(); it != terms.constEnd(); ++it) {
        const QString &term = it.key();
        const int tf = it.value();
        docTerms.insert(term);
        m_postings[term].insert(slot, static_cast<quint16>(qMin(tf, 65535)));
    }

    m_docTerms.insert(slot, docTerms);
    m_docLen.insert(slot, docLen);
    m_sumDocLen += docLen;
    ++m_docCount;
}

void BM25Index::removeDocument(int slot)
{
    const auto docIt = m_docTerms.constFind(slot);
    if (docIt == m_docTerms.constEnd()) return;

    for (const QString &term : docIt.value()) {
        auto postingIt = m_postings.find(term);
        if (postingIt == m_postings.end()) continue;
        postingIt->remove(slot);
        if (postingIt->isEmpty()) m_postings.erase(postingIt);
    }

    const auto lenIt = m_docLen.constFind(slot);
    if (lenIt != m_docLen.constEnd()) {
        m_sumDocLen -= lenIt.value();
        if (m_sumDocLen < 0) m_sumDocLen = 0;
    }

    m_docTerms.remove(slot);
    m_docLen.remove(slot);
    if (m_docCount > 0) --m_docCount;
}

void BM25Index::clear()
{
    m_postings.clear();
    m_docTerms.clear();
    m_docLen.clear();
    m_docCount = 0;
    m_sumDocLen = 0;
}

float BM25Index::lexicalScore(const QStringList &queryTerms, int slot) const
{
    if (m_docCount == 0 || queryTerms.isEmpty()) return 0.0f;

    const auto lenIt = m_docLen.constFind(slot);
    if (lenIt == m_docLen.constEnd()) return 0.0f;

    const double dl = static_cast<double>(lenIt.value());
    const double avgDl = static_cast<double>(m_sumDocLen) / static_cast<double>(qMax(1, m_docCount));

    const double k1 = 1.2, b = 0.75;
    double raw = 0.0;
    for (const QString &term : queryTerms) {
        const auto postingIt = m_postings.constFind(term);
        if (postingIt == m_postings.constEnd()) continue;
        const auto tfIt = postingIt->constFind(slot);
        if (tfIt == postingIt->constEnd() || tfIt.value() == 0) continue;

        const int tfDoc = tfIt.value();
        const int df = qMax(1, postingIt->size());
        const double idf = qLn(1.0 + (static_cast<double>(m_docCount) - df + 0.5)
                                     / (df + 0.5));
        const double norm = k1 * (1.0 - b + b * dl / qMax(1.0, avgDl));
        raw += idf * (tfDoc * (k1 + 1.0)) / (tfDoc + norm);
    }

    return static_cast<float>(raw / (1.0 + raw));
}

int BM25Index::docFreq(const QString &term) const
{
    const auto it = m_postings.constFind(term);
    return it == m_postings.constEnd() ? 0 : it->size();
}

float BM25Index::coverageScore(const QStringList &queryTerms, int rareQueryTerms, int slot) const
{
    const auto docIt = m_docTerms.constFind(slot);
    if (docIt == m_docTerms.constEnd() || docIt.value().isEmpty()) return 0.0f;

    const QSet<QString> &docTerms = docIt.value();
    int coveredRare = 0;
    for (const QString &term : queryTerms) {
        if (docTerms.contains(term) && docFreq(term) <= 2) ++coveredRare;
    }

    float coverage = rareQueryTerms > 0
                         ? static_cast<float>(coveredRare) / static_cast<float>(rareQueryTerms)
                         : 0.0f;

    if (coveredRare == 0 && rareQueryTerms == 0) {
        for (const QString &term : queryTerms) {
            if (docTerms.contains(term)) { coverage = 0.15f; break; }
        }
    }
    return coverage;
}

QSet<int> BM25Index::slotsForTerms(const QStringList &terms) const
{
    QSet<int> result;
    for (const QString &term : terms) {
        const auto it = m_postings.constFind(term);
        if (it == m_postings.constEnd()) continue;
        for (auto p = it->constBegin(); p != it->constEnd(); ++p) {
            result.insert(p.key());
        }
    }
    return result;
}
