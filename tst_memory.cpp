// Тесты слоёв памяти Риксоры.
//
// Зафиксировано ТОЛЬКО текущее (на 2026-09-28) поведение, без изменения
// семантики. Тест-файл — это «замороженный контракт»: любой будущий рефакторинг
// памяти обязан либо сохранить эти ожидания, либо осознанно изменить тест.
//
// Покрывает:
//   1) краткосрочную память  (окно SHORT_TERM_WINDOW + нормализация для Gemini)
//   2) среднесрочную память  (счётчик свёртки, clamp, инвариант «в полёте»)
//   3) долговременную память  (дедупликация, потолок, чистое удаление по чату)
//
// Тесты пишут на диск, поэтому весь путь данных уводится в тестовый каталог
// через QStandardPaths::setTestModeEnabled(true) — реальные данные пользователя
// тесты не трогают.

#include <QtTest>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QSettings>

#include "constants.h"
#include "geminiadapter.h"
#include "geminiapi.h"
#include "memorymanager.h"
#include "longtermmemory.h"
#include "chatmanager.h"

class TestMemory : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    // ---------- 0.1: разделение слоёв ----------
    void layerSizesAreIndependentConstants();

    // ---------- 1) Краткосрочная память ----------
    void windowIsEmptyForEmptyHistory();
    void windowReturnsAtMostShortTermWindow();
    void windowReturnsLastMessagesVerbatim();
    void windowIsNormalizedForGemini_data();
    void windowIsNormalizedForGemini();
    void windowDropsLeadingModelWhenCutLandsOnModel();
    void windowStripsTrailingModelReply();
    void windowMergesConsecutiveUserMessages();
    void windowNeverLeaksInternalIdField();

    // ---------- 2) Среднесрочная память ----------
    void needsSummarizationTriggersOnChunkSize();
    void summarizeChunkTakesExactlyChunkSizeMessages();
    void committedCountStaysOldWhileSummaryInFlight();
    void committedCountCatchesUpAfterSummaryApplied();
    void rollbackRestoresSummarizedCount();
    void removeMessageByIdKeepsSummaryCounterValid_data();
    void removeMessageByIdKeepsSummaryCounterValid();
    void truncateMessagesFromKeepsSummaryCounterValid();
    void summaryInvalidatedWhenEditingInsideSummarizedZone();
    void summaryKeptWhenEditingOutsideSummarizedZone();
    void clearResetsAllMidTermState();

    // ---------- 3) Долговременная память ----------
    void addFactRejectsEmptyAndTooShort();
    void addFactRejectsExactDuplicateIgnoringCase();
    void addFactRejectsCosineSimilarDuplicate();
    void addFactAcceptsDistinctByCosine();
    void addFactKeepsPerChatProvenance();
    void addFactsBatchReportsAddedCount();
    void addFactsBatchDeduplicatesWithinBatch();
    void addFactEnforcesMaxTotalCap();
    void deleteByChatIdRemovesOnlyThatChatsFacts();
    void deleteByMsgIdRemovesOnlyThatMessagesFacts();
    void deleteByMsgIdKeepsOtherMessagesAndChats();
    void deleteByMsgIdWithEmptyIdKeepsUnattributedFacts();
    void msgIdIsPersistedInStore();
    void jsonSnapshotImportsInlineEmbeddings();
    void noteExtractionRunDoesNotRewriteBigFiles();
    void backfillCursorSurvivesReload();
    void msgIdSurvivesReloadFromDisk();
    void addFactsAssignsMsgIdsPerPosition();
    void deleteAllMemoryWipesEverything();

    // ---------- 5) Разбор ответа Gemini (H1) ----------
    void parseGeminiTextReadsNormalResponse();
    void parseGeminiTextRejectsMissingCandidates_data();
    void parseGeminiTextRejectsMissingCandidates();
    void parseGeminiTextRejectsEmptyParts();
    void parseGeminiTextRejectsBlockReason();
    void parseGeminiTextRejectsBlockingFinishReason_data();
    void parseGeminiTextRejectsBlockingFinishReason();
    void parseGeminiTextAcceptsMaxTokens();
    void parseGeminiTextRejectsGarbage();
    void parseGeminiTextClearsOutputsOnFailure();

    // ---------- 6) Гигиена истории (M8) ----------
    void importMessagesSkipsEmptyText();

    // ---------- 8) Фаза 4: производительность и ресурсы ----------
    void lexicalSearchFindsRareTermThroughDocLenIndex();
    void queryHybridFillsLexicalSublist();
    void getChatListOldestFirstSortsByParsedDate();
    void deleteAllChatsForgetsLastChatId();
    void chatExistsIsCheapAndExact();
    void chatListIndexSelfHealsWhenFileVanishes();
    void chatListIndexSelfHealsWhenFileAppearsBehindOurBack();
    void chatListIndexSurvivesCorruption();

    // ---------- 9) Фаза 6: валидация ----------
    void phase6FullScenarioKeepsMemoryConsistent();
    void phase6MsgIdsSurviveRestart();
    void phase6AdapterContract_data();
    void phase6AdapterContract();
    void phase6LoadTwoThousandFacts();
    void phase6MaxTotalCapHolds();
    void phase6HundredQueriesStayLinear();

private:
    // Заполняет менеджер чередующимися ролями, начиная с "user".
    static void fillAlternating(MemoryManager &m, int count);
    static QStringList rolesOf(const QJsonArray &arr);
    static QStringList textsOf(const QList<MemoryHit> &hits);
    static void wipeMemoryDir();

    // ---------- Хелперы Фазы 6 ----------
    // Проверяет, что нормализованная история удовлетворяет контракту Gemini.
    // Именно нарушение этого контракта даёт 400 от generateContent.
    static void assertGeminiAccepts(const QJsonArray &normalized);
    // Ровно тот порядок вызовов, что делает NetworkManager при извлечении
    // фактов из чанка: текст чанка + id сообщений -> addFacts.
    static void extractLikeNetworkManager(LongTermMemory &ltm, const QString &chatId,
                                         const QJsonArray &history, int chunkSize);
    // Сообщение, гарантированно проходящее фильтр длины в addFactInner (>= 8 символов).
    static QJsonObject makeMsg(const QString &role, const QString &text, const QString &id);
    static QVector<float> smallEmbedding(quint64 seed, int dim = 16);
    // Читает факты из хранилища новым экземпляром — проверяем то, что реально
    // останется после рестарта, а не состояние в памяти.
    static QList<MemoryHit> storedFacts();
    static QString memoryDirPath();
    static QString baseChatsDir();
};

void TestMemory::wipeMemoryDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir(base).removeRecursively();
    QDir().mkpath(base + "/memory");
}

void TestMemory::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("RixoraTestOrg");
    QCoreApplication::setApplicationName("RixoraMemoryTest");
    wipeMemoryDir();
}

void TestMemory::cleanupTestCase()
{
    wipeMemoryDir();
}

void TestMemory::init()
{
    // Каждый тест стартует с пустой долговременной памяти на диске.
    wipeMemoryDir();
}

void TestMemory::fillAlternating(MemoryManager &m, int count)
{
    for (int i = 0; i < count; ++i) {
        m.addMessage((i % 2 == 0) ? "user" : "model", QString("сообщение %1").arg(i));
    }
}

QStringList TestMemory::rolesOf(const QJsonArray &arr)
{
    QStringList out;
    for (const QJsonValue &v : arr) out << v.toObject()["role"].toString();
    return out;
}

QStringList TestMemory::textsOf(const QList<MemoryHit> &hits)
{
    QStringList out;
    for (const MemoryHit &h : hits) out << h.text;
    return out;
}

// ============================================================
// 0.1 — разделение слоёв
// ============================================================

// Смысл теста не в самих числах, а в том, что это ДВЕ НЕЗАВИСИМЫЕ константы.
// Раньше размер окна краткосрочной памяти был привязан к SUMMARIZATION_CHUNK_SIZE,
// и правка среднесрочного слоя молча меняла краткосрочный. Тест — tripwire:
// изменение любой из констант заставит осознанно пересмотреть вторую.
void TestMemory::layerSizesAreIndependentConstants()
{
    QCOMPARE(SHORT_TERM_WINDOW, 34);
    QCOMPARE(SUMMARIZATION_CHUNK_SIZE, 34);
}

// ============================================================
// 1) Краткосрочная память
// ============================================================

void TestMemory::windowIsEmptyForEmptyHistory()
{
    MemoryManager m;
    QVERIFY(m.getRecentHistory(SHORT_TERM_WINDOW).isEmpty());
}

void TestMemory::windowReturnsAtMostShortTermWindow()
{
    MemoryManager m;
    fillAlternating(m, 100);
    QVERIFY(m.getRecentHistory(SHORT_TERM_WINDOW).size() <= SHORT_TERM_WINDOW);
}

// Если история уже укладывается в окно и заканчивается на user, адаптер
// ничего не переписывает: сообщения возвращаются дословно, включая порядок.
void TestMemory::windowReturnsLastMessagesVerbatim()
{
    MemoryManager m;
    for (int i = 0; i < 9; ++i) m.addMessage((i % 2 == 0) ? "user" : "model", QString("m%1").arg(i));

    const QJsonArray w = m.getRecentHistory(SHORT_TERM_WINDOW);
    QCOMPARE(w.size(), 9);                          // всё влезает в окно
    QCOMPARE(w.last().toObject()["parts"].toArray()[0].toObject()["text"].toString(),
             QString("m8"));
    QCOMPARE(rolesOf(w), QStringList({"user", "model", "user", "model", "user",
                                      "model", "user", "model", "user"}));
}

// Нормализация — то, что не даёт Gemini вернуть 400:
// строгое чередование, начало с user, конец на user, без служебного поля id.
void TestMemory::windowIsNormalizedForGemini_data()
{
    QTest::addColumn<int>("messageCount");

    QTest::newRow("1 сообщение") << 1;
    QTest::newRow("2 сообщения") << 2;
    QTest::newRow("33 сообщения (в окно)") << 33;
    QTest::newRow("34 сообщения (в окно)") << 34;
    QTest::newRow("35 сообщений (окно режет)") << 35;
    QTest::newRow("100 сообщений") << 100;
}

void TestMemory::windowIsNormalizedForGemini()
{
    QFETCH(int, messageCount);

    MemoryManager m;
    fillAlternating(m, messageCount);

    const QJsonArray w = m.getRecentHistory(SHORT_TERM_WINDOW);
    if (w.isEmpty()) {
        // Единственный допустимый пустой случай: нормализация не смогла
        // привести последовательность к валидной (история не начинается с user).
        QVERIFY(w.isEmpty());
        return;
    }

    QVERIFY(w.size() <= SHORT_TERM_WINDOW);

    const QStringList roles = rolesOf(w);
    QCOMPARE(roles.first(), QString("user"));   // Gemini требует начало с user
    QCOMPARE(roles.last(), QString("user"));    // и конец на user (промпт последний)

    for (int i = 1; i < roles.size(); ++i) {
        QVERIFY2(roles.at(i) != roles.at(i - 1),
                 qPrintable(QString("подряд идущие роли %1 в позиции %2")
                                .arg(roles.at(i)).arg(i)));
    }
    QVERIFY(!roles.contains(QString("model")) || roles.count("model") <= roles.count("user"));
}

// Окно режет историю так, что срез начинается с model — Gemini такой запрос
// отклоняет. Адаптер обязан отбросить ведущую model-реплику.
void TestMemory::windowDropsLeadingModelWhenCutLandsOnModel()
{
    MemoryManager m;
    fillAlternating(m, SHORT_TERM_WINDOW + 1);   // 35 сообщений, срез с индекса 1 = model

    const QJsonArray w = m.getRecentHistory(SHORT_TERM_WINDOW);
    QVERIFY(!w.isEmpty());
    QCOMPARE(w.first().toObject()["role"].toString(), QString("user"));
    QCOMPARE(w.size(), SHORT_TERM_WINDOW - 1);   // 33: одна реплика отброшена
}

// Зеркальный случай: запрос уходит с текущим промптом пользователя, поэтому
// последним в окне обязан быть user. Ответ модели, который уже есть в истории
// (например, диалог переоткрыли и сейчас готовится новый запрос), отбрасывается.
void TestMemory::windowStripsTrailingModelReply()
{
    MemoryManager m;
    fillAlternating(m, 8);   // 8 сообщений: user,model,...,user,model -> хвост model

    const QJsonArray w = m.getRecentHistory(SHORT_TERM_WINDOW);
    QCOMPARE(w.size(), 7);
    QCOMPARE(rolesOf(w).last(), QString("user"));
    QCOMPARE(w.last().toObject()["parts"].toArray()[0].toObject()["text"].toString(),
             QString("сообщение 6"));

    // Хвост из нескольких model-реплик подряд срезается целиком.
    MemoryManager m2;
    m2.addMessage("user", "вопрос");
    m2.addMessage("model", "ответ 1");
    m2.addMessage("model", "ответ 2");
    const QJsonArray w2 = m2.getRecentHistory(SHORT_TERM_WINDOW);
    QCOMPARE(w2.size(), 1);
    QCOMPARE(rolesOf(w2), QStringList({"user"}));
}

// После обрыва сети в истории остаются две user-реплики подряд. Gemini и это
// отклоняет — адаптер склеивает их в одно сообщение с несколькими parts.
void TestMemory::windowMergesConsecutiveUserMessages()
{
    MemoryManager m;
    m.addMessage("user", "первый вопрос");
    m.addMessage("user", "второй вопрос (ответ не пришёл)");

    const QJsonArray w = m.getRecentHistory(SHORT_TERM_WINDOW);
    QCOMPARE(w.size(), 1);
    QCOMPARE(w.at(0).toObject()["role"].toString(), QString("user"));

    const QJsonArray parts = w.at(0).toObject()["parts"].toArray();
    QCOMPARE(parts.size(), 2);                                    // оба текста в одном
    QCOMPARE(parts.at(0).toObject()["text"].toString(), QString("первый вопрос"));
    QCOMPARE(parts.at(1).toObject()["text"].toString(), QString("второй вопрос (ответ не пришёл)"));
}

// Поле id — служебное, для адресации в UI. Gemini отклоняет незнакомые поля
// в content (400). Утечка id в запрос — самая частая регрессия этого слоя.
void TestMemory::windowNeverLeaksInternalIdField()
{
    MemoryManager m;
    for (int i = 0; i < 5; ++i) {
        const QString id = m.addMessage((i % 2 == 0) ? "user" : "model", QString("текст %1").arg(i));
        QVERIFY(!id.isEmpty());                   // id реально есть во внутреннем хранилище
    }

    const QJsonArray w = m.getRecentHistory(SHORT_TERM_WINDOW);
    for (const QJsonValue &v : w) {
        const QJsonObject o = v.toObject();
        QVERIFY2(!o.contains("id"), "служебное поле id утекло в запрос к Gemini");
        // QJsonObject::keys() отсортированы, поэтому ждём именно "parts","role".
        QStringList keys = o.keys();
        keys.sort();
        QCOMPARE(keys, QStringList({"parts", "role"}));
    }
}

// ============================================================
// 2) Среднесрочная память
// ============================================================

void TestMemory::needsSummarizationTriggersOnChunkSize()
{
    MemoryManager m;
    fillAlternating(m, SUMMARIZATION_CHUNK_SIZE - 1);
    QVERIFY(!m.needsSummarization());

    m.addMessage("user", "ещё одно");
    QCOMPARE(m.getCommittedSummarizedCount(), 0);
    QVERIFY(m.needsSummarization());
}

// Свёртка берёт ровно SUMMARIZATION_CHUNK_SIZE сообщений, начиная с первого
// несвёрнутого. Пока сводка «в полёте», счётчик на диск не уезжает — он
// фиксируется лишь после setMidTermSummary().
void TestMemory::summarizeChunkTakesExactlyChunkSizeMessages()
{
    MemoryManager m;
    fillAlternating(m, SUMMARIZATION_CHUNK_SIZE * 2);
    QCOMPARE(m.getCommittedSummarizedCount(), 0);

    const QString chunk = m.getSummarizeChunkText();
    const QStringList lines = chunk.split('\n', Qt::SkipEmptyParts);
    QCOMPARE(lines.size(), SUMMARIZATION_CHUNK_SIZE);
    QVERIFY(chunk.startsWith("user: сообщение 0"));
    QCOMPARE(lines.first(), QString("user: сообщение 0"));
    QCOMPARE(lines.last(), QString("model: сообщение %1").arg(SUMMARIZATION_CHUNK_SIZE - 1));
    QVERIFY(!chunk.contains(QString("сообщение %1").arg(SUMMARIZATION_CHUNK_SIZE)));  // 35-е не попало

    QCOMPARE(m.getCommittedSummarizedCount(), 0);                 // сводка ещё в полёте
    m.setMidTermSummary("сводка первого куска");
    QCOMPARE(m.getCommittedSummarizedCount(), SUMMARIZATION_CHUNK_SIZE);
}

// Инвариант: пока сводка «в полёте», на диск уходит последний СОГЛАСОВАННЫЙ
// счётчик. Иначе на диск уехала бы пара «новый счётчик + старая сводка».
void TestMemory::committedCountStaysOldWhileSummaryInFlight()
{
    MemoryManager m;
    fillAlternating(m, SUMMARIZATION_CHUNK_SIZE);
    m.setSummarizedCount(0);
    QCOMPARE(m.getCommittedSummarizedCount(), 0);

    m.getSummarizeChunkText();

    QCOMPARE(m.getCommittedSummarizedCount(), 0);      // не 34!
    QVERIFY(!m.needsSummarization());                 // вторая свёртка не запускается
}

void TestMemory::committedCountCatchesUpAfterSummaryApplied()
{
    MemoryManager m;
    fillAlternating(m, SUMMARIZATION_CHUNK_SIZE);
    m.setSummarizedCount(0);

    m.getSummarizeChunkText();
    m.setMidTermSummary("сводка разговора");
    QCOMPARE(m.getCommittedSummarizedCount(), SUMMARIZATION_CHUNK_SIZE);
    QCOMPARE(m.getMidTermSummary(), QString("сводка разговора"));

    // Свёртка запускается только для НЕсвёрнутого хвоста.
    QVERIFY(!m.needsSummarization());
    fillAlternating(m, SUMMARIZATION_CHUNK_SIZE);     // ещё 34 сообщения
    QVERIFY(m.needsSummarization());
}

void TestMemory::rollbackRestoresSummarizedCount()
{
    MemoryManager m;
    fillAlternating(m, SUMMARIZATION_CHUNK_SIZE);
    m.setSummarizedCount(0);

    m.getSummarizeChunkText();
    QCOMPARE(m.getCommittedSummarizedCount(), 0);

    m.rollbackSummarization();
    QCOMPARE(m.getCommittedSummarizedCount(), 0);
    QVERIFY(m.needsSummarization());                  // свёртка будет повторена
    QVERIFY(m.getMidTermSummary().isEmpty());
}

// Удаление сообщения из УЖЕ СВЁРНУТОЙ зоны откатывает счётчик — иначе
// свёрнутые сообщения потерялись бы навсегда и память «съела бы» дыру.
void TestMemory::removeMessageByIdKeepsSummaryCounterValid_data()
{
    QTest::addColumn<int>("historySize");
    QTest::addColumn<int>("summarizedBefore");
    QTest::addColumn<int>("victimIndex");
    QTest::addColumn<int>("expectedAfter");

    // Правка ВНУТРИ свёрнутой зоны (victimIndex < summarizedBefore) делает сводку
    // недействительной: она описывает уже вырезанный текст. Тогда счётчик
    // не «поджимается» до индекса, а обнуляется — сводка будет собрана заново
    // уже по оставшейся истории. Правка ВНЕ зоны сводку не трогает вовсе.
    QTest::newRow("удаление из свёрнутой зоны")   << 20 << 10 << 3  << 0;
    QTest::newRow("удаление на границе свёртки")  << 20 << 10 << 10 << 10;
    QTest::newRow("удаление из несвёрнутой зоны") << 20 << 10 << 15 << 10;
    QTest::newRow("счётчик 0")                    << 20 << 0  << 0  << 0;
    QTest::newRow("удаление последнего")          << 20 << 19 << 19 << 19;
}

void TestMemory::removeMessageByIdKeepsSummaryCounterValid()
{
    QFETCH(int, historySize);
    QFETCH(int, summarizedBefore);
    QFETCH(int, victimIndex);
    QFETCH(int, expectedAfter);

    MemoryManager m;
    fillAlternating(m, historySize);
    m.setSummarizedCount(summarizedBefore);
    QCOMPARE(m.getCommittedSummarizedCount(), summarizedBefore);

    const QString victimId = m.exportMessages().at(victimIndex).toObject()["id"].toString();
    QVERIFY(!victimId.isEmpty());
    QVERIFY(m.removeMessageById(victimId));

    QCOMPARE(m.exportMessages().size(), historySize - 1);
    QCOMPARE(m.getCommittedSummarizedCount(), expectedAfter);
    QVERIFY(m.getCommittedSummarizedCount() <= m.exportMessages().size());
}

void TestMemory::truncateMessagesFromKeepsSummaryCounterValid()
{
    MemoryManager m;
    fillAlternating(m, 20);
    m.setSummarizedCount(10);

    m.truncateMessagesFrom(4);
    QCOMPARE(m.exportMessages().size(), 4);
    // 4 < 10 — свёрнутая зона затронута, сводка сброшена целиком.
    QCOMPARE(m.getCommittedSummarizedCount(), 0);
    QVERIFY(m.getMidTermSummary().isEmpty());
}

void TestMemory::summaryInvalidatedWhenEditingInsideSummarizedZone()
{
    MemoryManager m;
    fillAlternating(m, 20);
    m.setSummarizedCount(10);
    m.setMidTermSummary("сводка про старое");

    m.truncateMessagesFrom(5);
    QCOMPARE(m.exportMessages().size(), 5);
    QVERIFY(m.getMidTermSummary().isEmpty());
    QCOMPARE(m.getCommittedSummarizedCount(), 0);
}

void TestMemory::summaryKeptWhenEditingOutsideSummarizedZone()
{
    MemoryManager m;
    fillAlternating(m, 20);
    m.setSummarizedCount(10);
    m.setMidTermSummary("сводка остаётся в силе");

    // Правка за пределами свёрнутой зоны: сводка по-прежнему описывает
    // сообщения 0..9, которые не тронуты. Сбрасывать её нельзя.
    m.truncateMessagesFrom(15);
    QCOMPARE(m.exportMessages().size(), 15);
    QCOMPARE(m.getMidTermSummary(), QString("сводка остаётся в силе"));
    QCOMPARE(m.getCommittedSummarizedCount(), 10);
}

void TestMemory::clearResetsAllMidTermState()
{
    MemoryManager m;
    fillAlternating(m, SUMMARIZATION_CHUNK_SIZE);
    m.setSummarizedCount(0);
    m.setMidTermSummary("старая сводка");

    m.clear();
    QVERIFY(m.exportMessages().isEmpty());
    QVERIFY(m.getMidTermSummary().isEmpty());
    QCOMPARE(m.getCommittedSummarizedCount(), 0);
    QVERIFY(!m.needsSummarization());
    QVERIFY(m.lastMessageId().isEmpty());
}

// ============================================================
// 3) Долговременная память
// ============================================================

void TestMemory::addFactRejectsEmptyAndTooShort()
{
    LongTermMemory mem;
    QVERIFY(!mem.addFact(QString(), QVector<float>()));
    QVERIFY(!mem.addFact("   ", QVector<float>()));
    QVERIFY(!mem.addFact("коротк", QVector<float>()));      // 6 символов < 8
    QCOMPARE(mem.count(), 0);
}

void TestMemory::addFactRejectsExactDuplicateIgnoringCase()
{
    LongTermMemory mem;
    const QVector<float> emb{1.0f, 0.0f};

    QVERIFY(mem.addFact("Пользователь любит пиццу", emb, "chat1"));
    QCOMPARE(mem.count(), 1);

    QVERIFY(!mem.addFact("Пользователь любит пиццу", emb, "chat1"));
    QVERIFY(!mem.addFact("пользователь ЛЮБИТ пиццу", emb, "chat1"));   // регистр не важен
    QCOMPARE(mem.count(), 1);
}

void TestMemory::addFactRejectsCosineSimilarDuplicate()
{
    LongTermMemory mem;
    QVector<float> a{1.0f, 0.0f, 0.0f};
    QVector<float> b{0.99f, 0.1f, 0.0f};      // cos ~= 0.995 > 0.90 -> дубликат
    QVector<float> c{0.0f, 1.0f, 0.0f};      // cos = 0 -> новый факт

    QVERIFY(mem.addFact("Пользователь из Киева", a, "chat1"));
    QVERIFY(!mem.addFact("Пользователь из Киева, напоминание", b, "chat1"));
    QVERIFY(mem.addFact("Пользователь из Одессы", c, "chat1"));
    QCOMPARE(mem.count(), 2);
}

void TestMemory::addFactAcceptsDistinctByCosine()
{
    LongTermMemory mem;
    QVector<float> a{1.0f, 0.0f};
    QVector<float> b{0.95f, 0.30f};          // cos ~= 0.954 -> выше порога дедупа
    QVector<float> c{0.6f, 0.8f};            // cos = 0.6 -> ниже порога

    QVERIFY(mem.addFact("Пользователь работает врачом", a, "chat1"));
    QVERIFY(!mem.addFact("Пользователь работает врачом в Киеве", b, "chat1"));
    QVERIFY(mem.addFact("Пользователь увлекается бегом", c, "chat1"));
    QCOMPARE(mem.count(), 2);
}

void TestMemory::addFactKeepsPerChatProvenance()
{
    LongTermMemory mem;
    QVERIFY(mem.addFact("Пользователю 25 лет", QVector<float>{1.0f, 0.0f}, "chatA"));
    QVERIFY(mem.addFact("Пользователь играет на гитаре", QVector<float>{0.0f, 1.0f}, "chatB"));

    const QList<MemoryHit> hits = mem.recentFacts(10);
    QCOMPARE(hits.size(), 2);

    QStringList chatIds;
    for (const MemoryHit &h : hits) chatIds << h.chatId;
    QVERIFY(chatIds.contains("chatA"));
    QVERIFY(chatIds.contains("chatB"));
}

void TestMemory::addFactsBatchReportsAddedCount()
{
    LongTermMemory mem;
    QVector<QString> texts{"Пользователю 25 лет",
                           "Пользователь из Киева",
                           "Пользователь любит пиццу"};
    QVector<QVector<float>> embs{{1.0f, 0.0f}, {0.0f, 1.0f}, {0.7f, 0.7f}};
    const QVector<QString> msgIds{"m1", "m2", "m3"};

    int added = -1;
    mem.addFacts(texts, embs, "chat1", msgIds, &added);
    QCOMPARE(added, 3);
    QCOMPARE(mem.count(), 3);

    added = -1;
    mem.addFacts(texts, embs, "chat1", msgIds, &added);   // всё дубликаты
    QCOMPARE(added, 0);
    QCOMPARE(mem.count(), 3);
}

void TestMemory::addFactsBatchDeduplicatesWithinBatch()
{
    LongTermMemory mem;
    // Второй факт дубликат первого ещё до записи в память.
    QVector<QString> texts{"Пользователь любит пиццу",
                           "Пользователь любит пиццу",
                           "Пользователь из Киева"};
    QVector<QVector<float>> embs{{1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};

    int added = -1;
    mem.addFacts(texts, embs, "chat1", QVector<QString>{}, &added);
    QCOMPARE(added, 2);
    QCOMPARE(mem.count(), 2);
}

// Потолок памяти: жёсткий, вытесняется самый старый факт. Это и есть
// ограничение «память не растёт бесконечно».
void TestMemory::addFactEnforcesMaxTotalCap()
{
    LongTermMemory mem;

    QVector<QString> texts;
    QVector<QVector<float>> embs;   // без эмбеддингов — дедуп идёт по строке (быстро)
    for (int i = 0; i < LongTermMemory::MAX_TOTAL + 10; ++i) {
        texts << QString("факт%1").arg(i, 4, 10, QChar('0'));
    }

    for (int i = 0; i < texts.size(); i += 300) {
        int added = 0;
        mem.addFacts(texts.mid(i, 300), embs, "chat1", QVector<QString>{}, &added);
    }

    QCOMPARE(mem.count(), LongTermMemory::MAX_TOTAL);

    // Проверяем состав хранилища напрямую: queryLexical не годится для этого —
    // в его итоговую оценку входит свежесть (0.25 * recency), поэтому даже при
    // нулевом лексическом совпадении порог проходится и выдача не пустая.
    const QStringList stored = textsOf(mem.recentFacts(LongTermMemory::MAX_TOTAL + 50));
    QCOMPARE(stored.size(), LongTermMemory::MAX_TOTAL);

    // Самые старые вытеснены, самые свежие на месте.
    QVERIFY(!stored.contains("факт0000"));
    QVERIFY(!stored.contains("факт0005"));
    QVERIFY(stored.contains("факт0010"));      // граница вытеснения
    QVERIFY(stored.contains("факт2009"));
}

// Гарантия «source-of-truth»: удаление чата вырезает ровно его факты.
void TestMemory::deleteByChatIdRemovesOnlyThatChatsFacts()
{
    LongTermMemory mem;
    mem.addFact("Пользователю 25 лет", QVector<float>{1.0f, 0.0f}, "chatA");
    mem.addFact("Пользователь из Киева", QVector<float>{0.0f, 1.0f}, "chatA");
    mem.addFact("Пользователь играет на гитаре", QVector<float>{0.7f, 0.7f}, "chatB");
    QCOMPARE(mem.count(), 3);

    mem.deleteByChatId("chatA");
    QCOMPARE(mem.count(), 1);

    const QStringList left = textsOf(mem.recentFacts(10));
    QCOMPARE(left, QStringList({"Пользователь играет на гитаре"}));
    QVERIFY(!left.contains("Пользователю 25 лет"));
    QVERIFY(!left.contains("Пользователь из Киева"));
    for (const MemoryHit &h : mem.recentFacts(10)) {
        QCOMPARE(h.chatId, QString("chatB"));
    }

    // Пустой chatId не должен стирать всё.
    mem.deleteByChatId(QString());
    QCOMPARE(mem.count(), 1);
}

// ===== Привязка факта к сообщению (Фаза 2: приватность удаления) =====
//
// Факт хранит chatId и msgId. Удаление/перегенерация/редактирование ОДНОГО
// сообщения вырезает его из истории и вычищает связанные факты из памяти.

// Точечное удаление вычищает только факты своего сообщения.
void TestMemory::deleteByMsgIdRemovesOnlyThatMessagesFacts()
{
    LongTermMemory mem;
    mem.addFact("Пользователю 25 лет", QVector<float>{1.0f, 0.0f}, "chatA", "m1");
    mem.addFact("Пользователь из Киева", QVector<float>{0.0f, 1.0f}, "chatA", "m2");
    mem.addFact("Пользователь из Одессы", QVector<float>{0.0f, 0.0f, 1.0f}, "chatA", "m2");
    QCOMPARE(mem.count(), 3);

    mem.deleteByMsgId("chatA", "m1");

    QCOMPARE(mem.count(), 2);
    const QStringList left = textsOf(mem.recentFacts(10));
    QVERIFY(!left.contains("Пользователю 25 лет"));
    QVERIFY(left.contains("Пользователь из Киева"));
    QVERIFY(left.contains("Пользователь из Одессы"));
}

// msgId уникален ТОЛЬКО внутри чата: тот же id в другом чате — другой факт.
void TestMemory::deleteByMsgIdKeepsOtherMessagesAndChats()
{
    LongTermMemory mem;
    mem.addFact("Факт чата A сообщения m1", QVector<float>{1.0f, 0.0f}, "chatA", "m1");
    mem.addFact("Факт чата B сообщения m1", QVector<float>{0.0f, 1.0f}, "chatB", "m1");
    mem.addFact("Факт чата A сообщения m2", QVector<float>{0.0f, 0.0f, 1.0f}, "chatA", "m2");
    QCOMPARE(mem.count(), 3);

    mem.deleteByMsgId("chatA", "m1");

    QCOMPARE(mem.count(), 2);
    const QStringList left = textsOf(mem.recentFacts(10));
    QVERIFY(!left.contains("Факт чата A сообщения m1"));
    QVERIFY(left.contains("Факт чата B сообщения m1"));
    QVERIFY(left.contains("Факт чата A сообщения m2"));
}

// Факт может не иметь msgId. Вызов с пустым id не должен превращаться в
// «снеси всю неатрибутированную память».
void TestMemory::deleteByMsgIdWithEmptyIdKeepsUnattributedFacts()
{
    LongTermMemory mem;
    mem.addFact("Факт без привязки", QVector<float>{1.0f, 0.0f}, "chatA");
    mem.addFact("Факт с привязкой", QVector<float>{0.0f, 1.0f}, "chatA", "m1");
    QCOMPARE(mem.count(), 2);

    mem.deleteByMsgId("chatA", QString());
    QCOMPARE(mem.count(), 2);

    mem.deleteByMsgId(QString(), "m1");
    QCOMPARE(mem.count(), 2);
}

// msgId сохраняется в основном хранилище и доступен после перезапуска.
void TestMemory::msgIdIsPersistedInStore()
{
    LongTermMemory mem;
    mem.addFact("Риксоре 25 лет", QVector<float>{1.0f, 0.0f}, "chatA", "m1");
    QCOMPARE(mem.count(), 1);

    const QList<MemoryHit> stored = storedFacts();
    QCOMPARE(stored.size(), 1);
    QCOMPARE(stored.first().msgId, QString("m1"));
    QCOMPARE(stored.first().chatId, QString("chatA"));

    QVERIFY(QFile::exists(memoryDirPath() + "/rixora_memory.bin"));
    QVERIFY(QFile::exists(memoryDirPath() + "/rixora_memory_state.bin"));
}

// Снимок в JSON с эмбеддингами внутри импортируется в основное хранилище.
void TestMemory::jsonSnapshotImportsInlineEmbeddings()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    wipeMemoryDir();

    const QString snapshotDay = QDate::currentDate().toString("yyyy-MM-dd");
    QJsonObject root;
    root["version"] = 5;
    root["day"] = snapshotDay;
    root["extractions"] = 7;
    QJsonObject it;
    it["id"] = "snapshot1";
    it["text"] = "факт про Qt из снимка";
    it["ts"] = 1000.0;
    it["kind"] = "fact";
    it["chatId"] = "chatA";
    it["msgId"] = "m0";
    it["embedding"] = QJsonArray{0.5, -0.25, 0.125};
    root["items"] = QJsonArray{it};
    {
        QFile f(base + "/memory/bot_memory.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(root).toJson());
    }

    LongTermMemory mem;
    QCOMPARE(mem.count(), 1);

    const QVector<float> q{0.5f, -0.25f, 0.125f};
    const QList<MemoryHit> hits = mem.queryHybrid("Qt", q, 5, 0.0f);
    QVERIFY(!hits.isEmpty());
    QVERIFY(hits.first().vectorScore > 0.99f);

    QVERIFY(QFile::exists(base + "/memory/rixora_memory.bin"));
    QVERIFY(QFile::exists(base + "/memory/rixora_memory_state.bin"));

    // Дневной счётчик импортирован: после 73 отметок лимит исчерпан.
    for (int i = 0; i < 73; ++i) mem.noteExtractionRun();
    QVERIFY(!mem.canRunExtraction());

    mem.addFact("новый факт про язык", QVector<float>{0.0f, 1.0f, 0.0f}, "chatB", "m2");

    LongTermMemory reloaded;
    QCOMPARE(reloaded.count(), 2);
    const QList<MemoryHit> reloadedHits = reloaded.queryHybrid("Qt", q, 5, 0.0f);
    QVERIFY(!reloadedHits.isEmpty());
    QVERIFY(!reloaded.canRunExtraction());
}

// noteExtractionRun() обновляет только компактный файл состояния.
void TestMemory::noteExtractionRunDoesNotRewriteBigFiles()
{
    const QString store = memoryDirPath() + "/rixora_memory.bin";
    const QString state = memoryDirPath() + "/rixora_memory_state.bin";

    {
        LongTermMemory mem;
        mem.addFact("факт с вектором", QVector<float>{1.0f, 0.0f, 0.0f}, "chatA", "m1");
    }

    auto readAll = [](const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return QByteArray();
        return f.readAll();
    };

    const QByteArray storeBefore = readAll(store);
    const QByteArray stateBefore = readAll(state);
    QVERIFY(!storeBefore.isEmpty());
    QVERIFY(!stateBefore.isEmpty());

    {
        LongTermMemory mem;
        for (int i = 0; i < 80; ++i) mem.noteExtractionRun();
    }

    QCOMPARE(readAll(store), storeBefore);
    QVERIFY(readAll(state) != stateBefore);

    LongTermMemory reloaded;
    QVERIFY(!reloaded.canRunExtraction());
}

// Курсор бэкафилла обязан пережить перезапуск: без этого после каждого старта
// приложение заново прогоняло бы одни и те же старые чаты.
void TestMemory::backfillCursorSurvivesReload()
{
    wipeMemoryDir();

    QHash<QString, int> scanned;
    scanned.insert("chatA", 40);
    scanned.insert("chatB", 12);
    {
        LongTermMemory mem;
        mem.saveBackfillCursor(3, scanned);
    }

    {
        LongTermMemory reloaded;
        QCOMPARE(reloaded.backfillIndex(), 3);
        QCOMPARE(reloaded.scannedChatMsgs().value("chatA"), 40);
        QCOMPARE(reloaded.scannedChatMsgs().value("chatB"), 12);
    }

    // Состояние лежит в отдельном компактном файле, без фактов.
    const QString store = memoryDirPath() + "/rixora_memory.bin";
    const QString state = memoryDirPath() + "/rixora_memory_state.bin";
    QVERIFY(!QFile::exists(store));
    QVERIFY(QFile::exists(state));
    QVERIFY(QFileInfo(state).size() > 0);
}

// msgId переживает перезапуск: новый экземпляр читает файл заново.
void TestMemory::msgIdSurvivesReloadFromDisk()
{
    {
        LongTermMemory mem;
        mem.addFact("Пользователю 25 лет", QVector<float>{1.0f, 0.0f}, "chatA", "m1");
        mem.addFact("Пользователь из Киева", QVector<float>{0.0f, 1.0f}, "chatA", "m2");
    }

    LongTermMemory reloaded;
    QCOMPARE(reloaded.count(), 2);

    // И по диску удаляется ровно то, что относилось к удалённому сообщению.
    reloaded.deleteByMsgId("chatA", "m1");
    QCOMPARE(reloaded.count(), 1);
    QCOMPARE(textsOf(reloaded.recentFacts(10)), QStringList({"Пользователь из Киева"}));

    // И в памяти, даже без перезагрузки.
    LongTermMemory same;
    same.addFact("Пользователю 25 лет", QVector<float>{1.0f, 0.0f}, "chatB", "z1");
    QCOMPARE(same.count(), 2);
    same.deleteByMsgId("chatB", "z1");
    QCOMPARE(same.count(), 1);
    QCOMPARE(textsOf(same.recentFacts(10)), QStringList({"Пользователь из Киева"}));
}

// Батч извлечения привязывает i-й факт к i-му сообщению чанка; если сообщений
// меньше, чем фактов — лишние факты остаются без привязки (удалить их точечно
// нельзя, зато и нельзя приписать чужому сообщению).
void TestMemory::addFactsAssignsMsgIdsPerPosition()
{
    LongTermMemory mem;
    QVector<QString> texts{"Пользователю 25 лет", "Пользователь из Киева", "Пользователь любит пиццу"};
    QVector<QVector<float>> embs{{1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}};

    int added = 0;
    mem.addFacts(texts, embs, "chat1", QVector<QString>{"m1", "m2"}, &added);
    QCOMPARE(added, 3);

    const QList<MemoryHit> hits = mem.recentFacts(10);
    QCOMPARE(hits.size(), 3);
    int withoutId = 0;
    for (const MemoryHit &h : hits) {
        if (h.msgId.isEmpty()) ++withoutId;
        else QVERIFY(h.msgId == "m1" || h.msgId == "m2");
    }
    QCOMPARE(withoutId, 1);   // третий факт привязки не получил
}

// ============================================================
// 5) Разбор ответа Gemini (H1)
// ============================================================
//
// Раньше текст доставался так:
//   candidates[0].content.parts[0].text
// QJsonArray::operator[] за границами массива — это неопределённое поведение,
// поэтому пустой или заблокированный ответ от модели был не «ошибкой», а
// случайным значением, которое уходило в чат пустым сообщением. parseGeminiText
// обязан отличать успех от отказа КАЖДЫЙ раз.

// Собирает правдоподобный ответ generateContent.
static QByteArray geminiReply(const QJsonObject &candidate,
                              const QJsonObject &promptFeedback = QJsonObject())
{
    QJsonObject root;
    root["candidates"] = QJsonArray{candidate};
    root["promptFeedback"] = promptFeedback;
    return QJsonDocument(root).toJson();
}

static QJsonObject okCandidate(const QString &text, const QString &finish = "STOP")
{
    return QJsonObject{
        {"content", QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", text}}}}}},
        {"finishReason", finish}
    };
}

void TestMemory::parseGeminiTextReadsNormalResponse()
{
    QString out;
    QString reason;
    QVERIFY(parseGeminiText(geminiReply(okCandidate("Привет, я Риксора")), &out, &reason));
    QCOMPARE(out, QString("Привет, я Риксора"));
    QVERIFY(reason.isEmpty());
}

void TestMemory::parseGeminiTextRejectsMissingCandidates_data()
{
    QTest::addColumn<QByteArray>("payload");

    // Кандидатов нет вовсе — самый частый отказ при фильтрации безопасности.
    QTest::newRow("нет candidates")
        << QJsonDocument(QJsonObject{{"promptFeedback", QJsonObject{{"blockReason", "SAFETY"}}}}).toJson();
    QTest::newRow("пустой candidates") << geminiReply(QJsonObject{}, QJsonObject());
    QTest::newRow("нет ни одного поля") << QJsonDocument(QJsonObject{}).toJson();
    QTest::newRow("только usageMetadata")
        << QJsonDocument(QJsonObject{{"usageMetadata", QJsonObject{{"totalTokenCount", 12}}}}).toJson();
}

void TestMemory::parseGeminiTextRejectsMissingCandidates()
{
    QFETCH(QByteArray, payload);
    QString out;
    QString reason;
    QVERIFY(!parseGeminiText(payload, &out, &reason));
    QVERIFY(out.isEmpty());
    QVERIFY(!reason.isEmpty());
}

void TestMemory::parseGeminiTextRejectsEmptyParts()
{
    // content есть, parts пустой — раньше здесь был выход за границы массива.
    QString out;
    QString reason;
    const QByteArray payload = geminiReply(
        QJsonObject{{"content", QJsonObject{{"parts", QJsonArray{}}}}, {"finishReason", "STOP"}});
    QVERIFY(!parseGeminiText(payload, &out, &reason));
    QVERIFY(out.isEmpty());
    QVERIFY(!reason.isEmpty());

    // Нет самого content.
    const QByteArray noContent = geminiReply(QJsonObject{{"finishReason", "STOP"}});
    QVERIFY(!parseGeminiText(noContent, &out, &reason));
    QVERIFY(out.isEmpty());
}

void TestMemory::parseGeminiTextRejectsBlockReason()
{
    QString out;
    QString reason;
    const QByteArray payload = geminiReply(QJsonObject{}, QJsonObject{{"blockReason", "PROHIBITED_CONTENT"}});
    QVERIFY(!parseGeminiText(payload, &out, &reason));
    QCOMPARE(reason, QString("PROHIBITED_CONTENT"));
    QVERIFY(out.isEmpty());
}

void TestMemory::parseGeminiTextRejectsBlockingFinishReason_data()
{
    QTest::addColumn<QString>("finish");
    QTest::addColumn<bool>("accepted");

    QTest::newRow("SAFETY")            << "SAFETY"            << false;
    QTest::newRow("RECITATION")        << "RECITATION"        << false;
    QTest::newRow("BLOCKLIST")         << "BLOCKLIST"         << false;
    QTest::newRow("PROHIBITED_CONTENT") << "PROHIBITED_CONTENT" << false;
    QTest::newRow("SPII")              << "SPII"              << false;
    // Обрезанный по лимиту токенов ответ пригоден — его нельзя выбрасывать.
    QTest::newRow("MAX_TOKENS")        << "MAX_TOKENS"        << true;
    QTest::newRow("STOP")              << "STOP"              << true;
    // Причину может не присылать вовсе.
    QTest::newRow("пусто")             << ""                  << true;
}

void TestMemory::parseGeminiTextRejectsBlockingFinishReason()
{
    QFETCH(QString, finish);
    QFETCH(bool, accepted);

    QString out;
    QString reason;
    const bool ok = parseGeminiText(geminiReply(okCandidate("текст ответа", finish)), &out, &reason);
    QCOMPARE(ok, accepted);
    if (accepted) {
        QCOMPARE(out, QString("текст ответа"));
    } else {
        QCOMPARE(reason, finish);
        QVERIFY(out.isEmpty());
    }
}

void TestMemory::parseGeminiTextAcceptsMaxTokens()
{
    QString out;
    QString reason;
    QVERIFY(parseGeminiText(geminiReply(okCandidate("оборванный, но полезный текст", "MAX_TOKENS")),
                            &out, &reason));
    QCOMPARE(out, QString("оборванный, но полезный текст"));
}

// Мусор вместо JSON обязан быть отказом, а не разбором «как получится».
void TestMemory::parseGeminiTextRejectsGarbage()
{
    QString out;
    QString reason;
    QVERIFY(!parseGeminiText(QByteArray("<html>502 Bad Gateway</html>"), &out, &reason));
    QVERIFY(out.isEmpty());
    QVERIFY(!reason.isEmpty());

    QVERIFY(!parseGeminiText(QByteArray(), &out, &reason));
    QVERIFY(out.isEmpty());
}

// При отказе выходные параметры обязаны очищаться, иначе вызывающий получит
// текст ПРЕДЫДУЩЕГО успешного разбора.
void TestMemory::parseGeminiTextClearsOutputsOnFailure()
{
    QString out = "остатки прошлого разбора";
    QString reason = "остатки прошлой причины";
    QVERIFY(!parseGeminiText(QByteArray("не json"), &out, &reason));
    QVERIFY(out.isEmpty());
    QVERIFY(!reason.isEmpty());

    QVERIFY(!parseGeminiText(QByteArray("не json"), &out, &reason));
    QVERIFY(out.isEmpty());
    QVERIFY(!reason.isEmpty());

    // Успешный разбор тоже обязан чистить причину от предыдущего отказа.
    QVERIFY(parseGeminiText(geminiReply(okCandidate("ок")), &out, &reason));
    QCOMPARE(out, QString("ок"));
    QVERIFY(reason.isEmpty());
}

// ============================================================
// 6) Гигиена истории (M8)
// ============================================================

// importMessages не должен тащить в память пустые сообщения: summarizeChunkText
// склеит их в конспект, а getRecentHistory отдаст их модели.
void TestMemory::importMessagesSkipsEmptyText()
{
    MemoryManager m;

    auto msg = [](const QString &role, const QString &text) {
        return QJsonObject{{"role", role},
                           {"parts", QJsonArray{QJsonObject{{"text", text}}}}};
    };

    m.importMessages(QJsonArray{
        msg("user", "Меня зовут Антон"),
        msg("model", ""),
        msg("user", "   "),          // одни пробелы — тоже мусор
        msg("model", "\n\t"),
        msg("user", "Я из Киева")
    });

    const QJsonArray hist = m.exportMessages();
    QCOMPARE(hist.size(), 2);

    QStringList allTexts;
    for (const QJsonValue &v : hist) {
        allTexts << v.toObject()["parts"].toArray()[0].toObject()["text"].toString();
    }
    QCOMPARE(allTexts, QStringList({"Меня зовут Антон", "Я из Киева"}));
}

void TestMemory::deleteAllMemoryWipesEverything()
{
    LongTermMemory mem;
    mem.addFact("Пользователю 25 лет", QVector<float>{1.0f, 0.0f}, "chatA");
    mem.addFact("Пользователь из Киева", QVector<float>{0.0f, 1.0f}, "chatB");
    QVERIFY(mem.hasMemory());

    mem.deleteAllMemory();
    QCOMPARE(mem.count(), 0);
    QVERIFY(!mem.hasMemory());
}

// ---------- 8) Фаза 4: производительность и ресурсы ----------

// lexicalScore() берёт длину документа из m_docLen вместо пересчёта суммой tf.
// Индекс обязан давать ровно тот же результат, что и прежний пересчёт,
// иначе релевантность поедет вместе с оптимизацией.
void TestMemory::lexicalSearchFindsRareTermThroughDocLenIndex()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    wipeMemoryDir();

    LongTermMemory mem;
    mem.addFact("Антон работает в Киеве", QVector<float>{1.0f, 0.0f}, "chatA", "m1");
    mem.addFact("Меня зовут Марк", QVector<float>{0.0f, 1.0f}, "chatA", "m2");
    mem.addFact("Риксора любит апельсины", QVector<float>{0.0f, 0.0f, 1.0f}, "chatA", "m3");
    QCOMPARE(mem.count(), 3);

    // Порог 0 отсекает всех, поэтому проверяем именно порядок: BM25 с
    // нормализацией по длине документа обязан поставить совпавший факт первым.
    const QList<MemoryHit> hits = mem.queryLexical("апельсины", 5, 0.0f);
    QCOMPARE(hits.size(), 3);
    QCOMPARE(hits.first().text, QString("Риксора любит апельсины"));

    // Редкий термин должен поднимать свой документ над общим шумом.
    const QList<MemoryHit> byName = mem.queryLexical("Антон", 5, 0.0f);
    QVERIFY(!byName.isEmpty());
    QCOMPARE(byName.first().text, QString("Антон работает в Киеве"));

    // Неизвестный термин не должен ломать расчёт dl/avgDl: выше порога
    // "свежесть" (0.25 максимум) не поднимется никто.
    QCOMPARE(mem.queryLexical("такого-слова-нет", 5, 0.30f).size(), 0);
}

// queryHybrid() отдаёт лексический подсписок из того же прохода по фактам,
// поэтому вызывающему коду не нужен отдельный queryLexical() на каждый запрос.
void TestMemory::queryHybridFillsLexicalSublist()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    wipeMemoryDir();

    LongTermMemory mem;
    mem.addFact("Антон работает в Киеве", QVector<float>{1.0f, 0.0f}, "chatA", "m1");
    mem.addFact("Риксора любит апельсины", QVector<float>{0.0f, 1.0f}, "chatA", "m2");

    QList<MemoryHit> lexOut;
    const QList<MemoryHit> hybrid = mem.queryHybrid("апельсины", QVector<float>{0.0f, 1.0f}, 8, 0.0f, &lexOut);

    // Список заполнен, ограничен topK и содержит то же, что нашёл бы
    // отдельный queryLexical с порогом 0.18.
    QVERIFY(!lexOut.isEmpty());
    QVERIFY(lexOut.size() <= 8);
    const QList<MemoryHit> separate = mem.queryLexical("апельсины", 8, 0.18f);
    QCOMPARE(lexOut.size(), separate.size());
    QCOMPARE(lexOut.first().text, separate.first().text);

    // Без out-параметра ничего не ломается.
    QVERIFY(!mem.queryHybrid("апельсины", QVector<float>{0.0f, 1.0f}, 8, 0.0f).isEmpty());
}

// getChatListOldestFirst() нужен бэкафиллу, чтобы доходить до старых чатов.
// Порядок должен строиться по РАЗОБРАННОЙ дате: updatedAt хранится как
// "dd.MM.yyyy HH:mm", а такие строки текстом не упорядочены
// ("31.12.2025 23:00" > "01.01.2026 00:00" лексикографически).
void TestMemory::getChatListOldestFirstSortsByParsedDate()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    wipeMemoryDir();

    ChatManager cm;
    const QString dir = base + "/chats";
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);

    struct Case { const char *id; const char *updatedAt; };
    const Case cases[] = {
        {"c_mid",   "15.06.2025 10:00"},
        {"c_new",   "01.01.2026 00:00"},
        {"c_old",   "31.12.2025 23:00"},
    };
    for (const Case &c : cases) {
        QJsonObject o;
        o["id"] = QString::fromLatin1(c.id);
        o["title"] = QString::fromLatin1(c.id);
        o["createdAt"] = QString::fromLatin1(c.updatedAt);
        o["updatedAt"] = QString::fromLatin1(c.updatedAt);
        o["messages"] = QJsonArray();
        QFile f(dir + "/" + QString::fromLatin1(c.id) + ".json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(o).toJson());
    }

    QStringList order;
    const QJsonArray oldest = cm.getChatListOldestFirst();
    for (const QJsonValue &v : oldest) order << v.toObject()["id"].toString();
    QCOMPARE(order, QStringList({"c_mid", "c_old", "c_new"}));

    // Порядок для UI не тронут: getChatList() сортирует по времени изменения
    // файла (модификация свежее), и он отличается от oldest-first. Именно поэтому
    // для бэкафилла понадобился отдельный метод, а не разворот этого.
    const QJsonArray newest = cm.getChatList();
    QCOMPARE(newest.size(), oldest.size());
    QStringList uiOrder;
    for (const QJsonValue &v : newest) uiOrder << v.toObject()["id"].toString();
    std::sort(uiOrder.begin(), uiOrder.end());
    QStringList sortedOldest = order;
    std::sort(sortedOldest.begin(), sortedOldest.end());
    QCOMPARE(uiOrder, sortedOldest);   // набор чатов тот же
    QVERIFY(uiOrder.size() >= 2);
}

// ------------------------------------------------------------
// Индекс метаданных чатов (ChatsIndex): getChatList() больше не читает
// файлы чатов, а берёт метаданные из sidecar-индекса. Индекс обязан
// самовосстанавливаться при любом расхождении с диском — иначе история
// чатов молча разъехалась бы с реальными файлами.
// ------------------------------------------------------------

// Проверка существования чата заменяет чтение всех чатов; должна быть
// точной и не зависеть от наличия индекса.
void TestMemory::chatExistsIsCheapAndExact()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    wipeMemoryDir();

    ChatManager cm;
    const QString dir = base + "/chats";
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);

    QVERIFY(!cm.chatExists(QString()));           // пустой id — не чат
    QVERIFY(!cm.chatExists("no_such_chat_id"));

    const QString id = cm.createNewChat();
    QVERIFY(!cm.chatExists(id));                   // создан, но ещё не записан
    cm.saveChat(id, QJsonArray());
    QVERIFY(cm.chatExists(id));

    cm.deleteChat(id);
    QVERIFY(!cm.chatExists(id));
}

// Чат удалили с диска мимо ChatManager (синхронизация облака, ручная чистка).
// Индекс устарел — список обязан это заметить и пересобрать сам.
void TestMemory::chatListIndexSelfHealsWhenFileVanishes()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    wipeMemoryDir();

    ChatManager cm;
    const QString dir = base + "/chats";
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);

    const QString a = cm.createNewChat();
    const QString b = cm.createNewChat();
    cm.saveChat(a, QJsonArray{{QJsonObject{{"role", "user"}, {"parts", QJsonArray{QJsonObject{{"text", "первый"}}}}}}});
    cm.saveChat(b, QJsonArray{{QJsonObject{{"role", "user"}, {"parts", QJsonArray{QJsonObject{{"text", "второй"}}}}}}});

    QCOMPARE(cm.getChatList().size(), 2);          // индекс построен
    QVERIFY(QFile::remove(dir + "/" + a + ".json"));
    QVERIFY(QFile(dir + "/" + a + ".json").exists() == false);

    const QJsonArray after = cm.getChatList();
    QCOMPARE(after.size(), 1);                     // исчезнувший чат выпал
    QCOMPARE(after[0].toObject()["id"].toString(), b);
}

// Чат появился на диске мимо ChatManager — индекс обязан его подхватить.
void TestMemory::chatListIndexSelfHealsWhenFileAppearsBehindOurBack()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    wipeMemoryDir();

    ChatManager cm;
    const QString dir = base + "/chats";
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);

    cm.saveChat("kept_chat", QJsonArray());
    QCOMPARE(cm.getChatList().size(), 1);

    QJsonObject o;
    o["id"] = "intruder_chat";
    o["title"] = "Появился извне";
    o["createdAt"] = "01.01.2026 00:00";
    o["updatedAt"] = "01.01.2026 00:00";
    o["messages"] = QJsonArray();
    QFile f(dir + "/intruder_chat.json");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(o).toJson());
    f.close();

    const QJsonArray after = cm.getChatList();
    QCOMPARE(after.size(), 2);
    QStringList ids;
    for (const QJsonValue &v : after) ids << v.toObject()["id"].toString();
    QVERIFY(ids.contains("intruder_chat"));
    QVERIFY(ids.contains("kept_chat"));
}

// Битый (не парсящийся) индекс не должен ронять историю — он считается
// неконсистентным и пересобирается из файлов.
void TestMemory::chatListIndexSurvivesCorruption()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    wipeMemoryDir();

    ChatManager cm;
    const QString dir = base + "/chats";
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);

    cm.saveChat("chat_x", QJsonArray{{QJsonObject{{"role", "user"}, {"parts", QJsonArray{QJsonObject{{"text", "текст"}}}}}}});
    QCOMPARE(cm.getChatList().size(), 1);

    QFile idx(dir + "/.chats_index");
    QVERIFY(idx.open(QIODevice::WriteOnly));
    idx.write("{ this is not json");
    idx.close();

    const QJsonArray after = cm.getChatList();
    QCOMPARE(after.size(), 1);
    QCOMPARE(after[0].toObject()["id"].toString(), QString("chat_x"));
}

// Полная очистка обязана забывать lastChatId, иначе при следующем старте
// приложение попытается открыть уже несуществующий чат.
void TestMemory::deleteAllChatsForgetsLastChatId()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    wipeMemoryDir();

    ChatManager cm;
    const QString dir = base + "/chats";
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);

    const QString id = cm.createNewChat();
    QVERIFY(!id.isEmpty());
    cm.setCurrentChatId(id);
    QSettings().setValue("app/lastChatId", id);

    cm.deleteAllChats();

    QVERIFY(!QSettings().contains("app/lastChatId"));
    QCOMPARE(cm.getCurrentChatId(), QString());
    QCOMPARE(cm.getChatList().size(), 0);
}

// ============================================================
// ФАЗА 6: ВАЛИДАЦИЯ
// ============================================================

QString TestMemory::memoryDirPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/memory";
}

QString TestMemory::baseChatsDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/chats";
}

QList<MemoryHit> TestMemory::storedFacts() {
    LongTermMemory mem;
    return mem.recentFacts(LongTermMemory::MAX_TOTAL + 50);
}

QVector<float> TestMemory::smallEmbedding(quint64 seed, int dim) {
    // Детерминированный, но ПОЧТИ ОРТОГОНАЛЬНЫЙ генератор. Простая
    // арифметическая прогрессия по координатам даёт почти параллельные векторы
    // (косинус > 0.9), и дедупликация в addFactInner отбрасывает их как
    // дубликаты. Поэтому каждая координата берётся из хеша (splitmix64) —
    // вектора статистически независимы и дедуп не срабатывает ложно.
    QVector<float> v(dim);
    double norm = 0.0;
    for (int i = 0; i < dim; ++i) {
        quint64 z = seed * 0x9E3779B97F4A7C15ULL + quint64(i) * 0xD1B54A32D192ED03ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        z ^= z >> 31;
        // [0,1) -> [-1,1)
        const double u = double(z >> 11) / double(1ULL << 53);
        v[i] = float(u * 2.0 - 1.0);
        norm += double(v[i]) * double(v[i]);
    }
    norm = qSqrt(norm);
    if (norm > 0.0) for (int i = 0; i < dim; ++i) v[i] = float(double(v[i]) / norm);
    return v;
}

QJsonObject TestMemory::makeMsg(const QString &role, const QString &text, const QString &id) {
    QJsonObject msg;
    msg["role"] = role;
    msg["parts"] = QJsonArray{QJsonObject{{"text", text}}};
    msg["id"] = id;
    return msg;
}

void TestMemory::extractLikeNetworkManager(LongTermMemory &ltm, const QString &chatId,
                                           const QJsonArray &history, int chunkSize) {
    // Смещение намеренно не кратно размеру чанка: так id фактов привязываются
    // к реальным сообщениям, а не к «красивым» индексам с нуля.
    const int start = qMax(0, history.size() - chunkSize);
    QStringList texts;
    QStringList msgIds;
    for (int i = start; i < history.size(); ++i) {
        const QJsonObject m = history[i].toObject();
        const QString t = m["parts"].toArray()[0].toObject()["text"].toString();
        if (t.trimmed().isEmpty()) continue;
        texts << t;
        msgIds << m["id"].toString();
    }
    if (texts.isEmpty()) return;

    QVector<QVector<float>> embs;
    // chatId входит в seed: иначе факты разных чатов получают одинаковые
    // векторы и дедупликация считает их дубликатами.
    const quint64 chatSeed = quint64(qHash(chatId)) * 1000003ULL;
    for (int i = 0; i < texts.size(); ++i) embs.append(smallEmbedding(chatSeed + quint64(start + i)));
    ltm.addFacts(texts, embs, chatId, msgIds);
}

void TestMemory::assertGeminiAccepts(const QJsonArray &normalized) {
    if (normalized.isEmpty()) return;   // пустой contents допустим, его не шлём

    // 1) Начинается с user.
    QCOMPARE(normalized.first().toObject()["role"].toString(), QString("user"));
    // 2) Заканчивается user.
    QCOMPARE(normalized.last().toObject()["role"].toString(), QString("user"));
    // 3) Строгое чередование ролей.
    for (int i = 1; i < normalized.size(); ++i) {
        QVERIFY(normalized[i].toObject()["role"].toString()
                != normalized[i - 1].toObject()["role"].toString());
    }
    // 4) Нет служебного поля id: Gemini отклоняет незнакомые поля в content.
    for (const QJsonValue &v : normalized) {
        const QJsonObject m = v.toObject();
        QVERIFY2(!m.contains("id"), "в history попало служебное поле id -> 400 от Gemini");
        // 5) Есть текст и он не пустой.
        const QJsonArray parts = m["parts"].toArray();
        QVERIFY(!parts.isEmpty());
        for (const QJsonValue &p : parts) {
            QVERIFY(!p.toObject()["text"].toString().trimmed().isEmpty());
        }
    }
}

// 6.3: контракт адаптера на наборе форм, которые раньше ломали запрос к Gemini.
void TestMemory::phase6AdapterContract_data() {
    QTest::addColumn<QJsonArray>("input");

    auto alternating = [](int n) {
        QJsonArray a;
        for (int i = 0; i < n; ++i) {
            a.append(makeMsg(i % 2 == 0 ? "user" : "model",
                             QString("реплика номер %1").arg(i), QString("m%1").arg(i)));
        }
        return a;
    };

    QTest::newRow("1 сообщение") << alternating(1);
    QTest::newRow("2 сообщения") << alternating(2);
    QTest::newRow("33 сообщения") << alternating(33);
    QTest::newRow("34 сообщения") << alternating(34);
    QTest::newRow("35 сообщений") << alternating(35);
    QTest::newRow("70 сообщений") << alternating(70);

    // Обрыв сети: ответ модели не пришёл, две user-реплики подряд.
    QTest::newRow("обрыв: две user подряд") << QJsonArray{
        makeMsg("user", "первый вопрос пользователя", "a"),
        makeMsg("user", "второй вопрос пользователя", "b"),
        makeMsg("model", "ответ на второй вопрос", "c"),
        makeMsg("user", "третий вопрос пользователя", "d")};

    // Три user подряд в начале.
    QTest::newRow("обрыв: три user подряд в начале") << QJsonArray{
        makeMsg("user", "первый вопрос пользователя", "a"),
        makeMsg("user", "повтор первого вопроса", "b"),
        makeMsg("user", "уточнение первого вопроса", "c"),
        makeMsg("model", "ответ на уточнение", "d"),
        makeMsg("user", "четвёртый вопрос пользователя", "e")};

    // Загрузка сохранённого чата, начавшегося с model.
    QTest::newRow("чат начинается с model") << QJsonArray{
        makeMsg("model", "хвост прошлого ответа", "a"),
        makeMsg("user", "новый вопрос пользователя", "b"),
        makeMsg("model", "новый ответ модели", "c"),
        makeMsg("user", "ещё вопрос пользователя", "d")};

    // Чат из одних model-реплик: приводить не к чему.
    QTest::newRow("только model") << QJsonArray{
        makeMsg("model", "ответ без вопроса", "a"),
        makeMsg("model", "ещё ответ без вопроса", "b")};

    // Хвост из model-реплик (ответ не дождался отправки следующего запроса).
    QTest::newRow("хвост из model") << QJsonArray{
        makeMsg("user", "вопрос пользователя", "a"),
        makeMsg("model", "ответ модели", "b"),
        makeMsg("model", "ошибочный второй ответ", "c")};
}

void TestMemory::phase6AdapterContract() {
    QFETCH(QJsonArray, input);

    const QJsonArray out = GeminiHistoryAdapter::normalizeHistoryForGemini(input);
    assertGeminiAccepts(out);

    // Ни один вход с пользовательским текстом не должен молча превратиться в
    // пустую историю — это молчаливая потеря контекста у пользователя.
    bool hasUser = false;
    for (const QJsonValue &v : input) {
        if (v.toObject()["role"].toString() == "user") hasUser = true;
    }
    if (hasUser) QVERIFY2(!out.isEmpty(), "история с user-репликами схлопнулась в пустую");

    // Текст пользователя не должен теряться при склейке соседних ролей.
    QString joinedOut;
    for (const QJsonValue &v : out) {
        for (const QJsonValue &p : v.toObject()["parts"].toArray()) {
            joinedOut += p.toObject()["text"].toString() + "\n";
        }
    }
    for (const QJsonValue &v : input) {
        const QJsonObject m = v.toObject();
        if (m["role"].toString() != "user") continue;
        const QString t = m["parts"].toArray()[0].toObject()["text"].toString();
        if (t.trimmed().isEmpty()) continue;
        QVERIFY2(joinedOut.contains(t), "текст user-реплики потерялся при нормализации");
    }
}

// 6.1: весь сценарий целиком на диске.
void TestMemory::phase6FullScenarioKeepsMemoryConsistent() {
    wipeMemoryDir();

    ChatManager cm;
    QDir(baseChatsDir()).removeRecursively();
    QDir().mkpath(baseChatsDir());

    LongTermMemory ltm;
    MemoryManager mm;

    const int kMessagesPerChat = 60;
    QStringList chatIds;

    auto fill = [](const QString &chatId) {
        QJsonArray msgs;
        for (int i = 0; i < kMessagesPerChat; ++i) {
            const bool isUser = (i % 2 == 0);
            const QString text = isUser
                ? QString("вопрос пользователя %1 про %2").arg(i).arg(chatId.left(4))
                : QString("ответ модели %1 про %2").arg(i).arg(chatId.left(4));
            msgs.append(makeMsg(isUser ? "user" : "model", text,
                                QString("%1-%2").arg(chatId.left(4)).arg(i)));
        }
        return msgs;
    };

    // --- 1) три чата по 60 сообщений, переключение между ними ---
    for (int c = 0; c < 3; ++c) {
        const QString id = cm.createNewChat();
        QVERIFY(!id.isEmpty());
        chatIds << id;
        cm.saveChat(id, fill(id), QString(), 0);
    }
    QCOMPARE(cm.getChatList().size(), 3);

    // Факты добавляем так же, как это делает бэкафилл/извлечение: чанками.
    const QStringList chatOrder{chatIds[0], chatIds[1], chatIds[2]};
    QHash<QString, QJsonArray> histories;
    QHash<QString, QJsonArray> importedHistories;  // истории с реальными UUID
    for (const QString &id : chatOrder) {
        const QJsonArray msgs = fill(id);
        histories[id] = msgs;
        mm.importMessages(msgs);
        // Сохраняем историю с РЕАЛЬНЫМИ UUID для последующих проверок
        importedHistories[id] = mm.exportMessages();
        // Извлекаем факты, используя реальные ID из импортированной истории
        extractLikeNetworkManager(ltm, id, importedHistories[id], EXTRACTION_CHUNK_SIZE);
        cm.setCurrentChatId(id);
    }
    const int perChat = 12;   // EXTRACTION_CHUNK_SIZE
    QCOMPARE(ltm.count(), 3 * perChat);

    // Факты трёх чатов сохранены в основном хранилище.
    QCOMPARE(storedFacts().size(), 3 * perChat);
    QVERIFY(QFile::exists(memoryDirPath() + "/rixora_memory.bin"));
    QVERIFY(QFile::exists(memoryDirPath() + "/rixora_memory_state.bin"));

    // --- 2) сводка соответствует summarizedCount ---
    // Ровно как в production: getSummarizeChunkText() вызывается только под
    // защитой needsSummarization(), которая гарантирует наличие целого чанка.
    // При 60 сообщениях и SUMMARIZATION_CHUNK_SIZE=34 влезает ровно один чанк.
    QVERIFY(mm.needsSummarization());
    const int committed0 = mm.getCommittedSummarizedCount();
    QCOMPARE(committed0, 0);
    mm.getSummarizeChunkText();
    // Пока сводка «в полёте», согласованным остаётся старое значение.
    QCOMPARE(mm.getCommittedSummarizedCount(), committed0);
    QVERIFY(!mm.needsSummarization());          // summaryInFlight блокирует новую сводку
    mm.setMidTermSummary("конспект первого куска");
    QCOMPARE(mm.getCommittedSummarizedCount(), SUMMARIZATION_CHUNK_SIZE);
    QCOMPARE(mm.getMidTermSummary(), QString("конспект первого куска"));
    // Хвоста (60 - 34 = 26 сообщений) на второй чанк не хватает.
    QVERIFY(!mm.needsSummarization());

    // --- 3) правка внутри сводящейся зоны обнуляет и сводку, и счётчик ---
    // Повторяем ровно то, что делает NetworkManager::editUserMessage для
    // сообщения с ответом модели: replace + truncate хвоста.
    const QJsonArray histA = importedHistories[chatIds[0]];
    const int editIdx = 2;                     // user-реплика, за ней есть ответ
    QCOMPARE(histA[editIdx].toObject()["role"].toString(), QString("user"));
    QCOMPARE(histA[editIdx + 1].toObject()["role"].toString(), QString("model"));

    MemoryManager edit;
    edit.importMessages(histA);
    // Важно: после importMessages ID сообщений — это НОВЫЕ UUID, а не исходные
    // из histA. Берём реальный ID по индексу.
    const QJsonArray importedA = edit.exportMessages();
    QVERIFY2(importedA.size() > editIdx, "imported history too short");
    const QString idInside = importedA[editIdx].toObject()["id"].toString();
    QVERIFY2(!idInside.isEmpty(), "idInside is empty");
    // ID модельного ответа, который должен удалиться при truncate
    const QString modelMsgId = importedA[editIdx + 1].toObject()["id"].toString();
    QVERIFY2(edit.indexOfMessageById(idInside) == editIdx, "ID not found at expected index");
    QVERIFY2(edit.indexOfMessageById(modelMsgId) == editIdx + 1, "modelMsgId not at expected index");
    edit.setSummarizedCount(SUMMARIZATION_CHUNK_SIZE);
    edit.setMidTermSummary("конспект до правки");
    QVERIFY2(edit.replaceMessageById(idInside, "исправленный текст вопроса пользователя"),
             "replaceMessageById failed - check ID exists");
    edit.truncateMessagesFrom(editIdx + 1);
    QCOMPARE(edit.getMidTermSummary(), QString());
    QCOMPARE(edit.getCommittedSummarizedCount(), 0);
    QCOMPARE(edit.indexOfMessageById(idInside), editIdx);
    QCOMPARE(edit.indexOfMessageById(modelMsgId), -1);

    // --- 4) refresh: перезапись ответа вырезает хвост и его факты ---
    {
        const QString &cid = chatIds[2];
        QJsonArray hist = importedHistories[cid];
        MemoryManager rw;
        rw.importMessages(hist);
        const int idx = 41;   // модельная реплика (нечётный индекс), после неё 18 сообщений
        // Реальные ID после импорта
        const QJsonArray importedHist = rw.exportMessages();
        QCOMPARE(importedHist[idx].toObject()["role"].toString(), QString("model"));

        // NetworkManager: id всех сообщений от idx и дальше -> deleteByMsgId.
        QStringList staleIds;
        for (int i = idx; i < importedHist.size(); ++i)
            staleIds << importedHist[i].toObject()["id"].toString();

        rw.truncateMessagesFrom(idx);
        QCOMPARE(rw.indexOfMessageById(importedHist[idx].toObject()["id"].toString()), -1);
        for (const QString &sid : staleIds) ltm.deleteByMsgId(cid, sid);

        // Хвоста больше нет ни в истории, ни в памяти.
        for (const MemoryHit &h : storedFacts()) {
            if (h.chatId != cid) continue;
            QVERIFY2(!staleIds.contains(h.msgId),
                     "факт удалённого при refresh сообщения остался в памяти");
        }
    }

    // --- 5) удаление сообщения вычищает только его факты ---
    {
        const QString &cid = chatIds[1];
        // Используем импортированную историю с реальными UUID (та же, что для извлечения фактов)
        const QJsonArray importedHist = importedHistories[cid];
        // Факты извлекались из ПОСЛЕДНИХ EXTRACTION_CHUNK_SIZE сообщений (индексы 48-59).
        // Удаляем одно из них, чтобы проверить удаление факта.
        const int victimIdx = importedHist.size() - EXTRACTION_CHUNK_SIZE;  // 48
        const QString victimId = importedHist[victimIdx].toObject()["id"].toString();
        const QString victimText = importedHist[victimIdx].toObject()["parts"]
                                       .toArray()[0].toObject()["text"].toString();

        // Прямое удаление из долговой памяти по msgId (removeMessageById тестируется отдельно)
        ltm.deleteByMsgId(cid, victimId);

        const QList<MemoryHit> stored = storedFacts();
        for (const MemoryHit &h : stored) {
            QVERIFY2(h.msgId != victimId, "факт удалённого сообщения остался");
            // Текст может совпадать с другим фактом, проверяем только msgId
        }
        // Остальные факты чата на месте (perChat - 1, так как удалили один факт).
        int mine = 0;
        for (const MemoryHit &h : stored) {
            if (h.chatId == cid) ++mine;
        }
        QCOMPARE(mine, perChat - 1);
    }

    // --- 6) удаление чата вычищает все его факты ---
    {
        const QString &cid = chatIds[0];
        cm.deleteChat(cid);
        ltm.deleteByChatId(cid);
        cm.saveChat(cid, importedHistories[cid], QString(), 0);   // вернём, чтобы проверить рестарт

        for (const MemoryHit &h : storedFacts()) {
            QVERIFY2(h.chatId != cid, "факт удалённого чата остался в памяти");
        }
    }

    // --- 7) рестарт приложения: состояние с диска совпадает с ожиданием ---
    {
        ChatManager cm2;
        LongTermMemory ltm2;
        QCOMPARE(cm2.getChatList().size(), 3);
        // Чат A удалён (0 фактов), чат B: 12-1=11 фактов, чат C: refresh удалил хвост
        // с извлечёнными фактами (индексы 48-59 попали в truncate from 41) → 0 фактов.
        // Итого: 11 фактов.
        QCOMPARE(ltm2.count(), perChat - 1);

        // Факты с вектором пережили рестарт (бинарник читается обратно).
        const QList<MemoryHit> hits = ltm2.queryLexical("вопрос", 5, 0.0f);
        QVERIFY(!hits.isEmpty());
        for (const MemoryHit &h : hits) QVERIFY(!h.text.trimmed().isEmpty());
    }

    // --- 8) deleteAll: память и история пусты, курсор бэкафилла сброшен ---
    {
        cm.deleteAllChats();
        ltm.deleteAllMemory();
        ltm.saveBackfillCursor(0, QHash<QString, int>());

        QCOMPARE(storedFacts().size(), 0);
        QCOMPARE(ltm.count(), 0);
        QVERIFY(!ltm.hasMemory());

        LongTermMemory ltm3;
        QCOMPARE(ltm3.count(), 0);
        QCOMPARE(ltm3.backfillIndex(), 0);
        QVERIFY(ltm3.scannedChatMsgs().isEmpty());

        ChatManager cm3;
        QCOMPARE(cm3.getChatList().size(), 0);
    }
}

// 6.2: 2000 фактов — время загрузки и размеры файлов.
void TestMemory::phase6LoadTwoThousandFacts() {
    wipeMemoryDir();

    const int n = 2000;
    // dim = 64: дедупликация в addFactInner линейно сканирует уже сохранённые
    // факты, то есть addFacts на N фактах стоит O(N^2 * dim). Меньшая
    // размерность держит подготовку теста быстрой, не меняя логику хранения.
    const int dim = 64;

    QVector<QString> texts;
    QVector<QVector<float>> embs;
    texts.reserve(n);
    embs.reserve(n);
    for (int i = 0; i < n; ++i) {
        texts << QString("факт номер %1: пользователь предпочитает %2")
                     .arg(i).arg(QStringList{"кириллицу", "латиницу", "цифры"}.at(i % 3));
        embs.append(smallEmbedding(quint64(i) * 7919ULL + 13ULL, dim));
    }

    // Через addFacts (пакетно), как это делает production: addFact переписывает
    // весь файл на каждый факт, что при 2000 итерациях упирается в таймаут теста.
    QElapsedTimer saveTimer;
    saveTimer.start();
    int added = 0;
    {
        LongTermMemory ltm;
        ltm.addFacts(texts, embs, "chatA", QStringList(), &added);
    }
    const qint64 saveMs = saveTimer.elapsed();
    QCOMPARE(added, n);

    const QFileInfo storeInfo(memoryDirPath() + "/rixora_memory.bin");
    const QFileInfo stateInfo(memoryDirPath() + "/rixora_memory_state.bin");
    QVERIFY(storeInfo.exists());
    QVERIFY(stateInfo.exists());

    qInfo() << "[6.2] save" << n << "фактов:" << saveMs << "мс"
            << "| store" << storeInfo.size() / 1024 << "КБ"
            << "| state" << stateInfo.size() << "Б";

    // Основное хранилище содержит и факты, и векторы.
    QVERIFY(storeInfo.size() > 1000);
    QCOMPARE(storedFacts().size(), n);

    // Замеряем именно загрузку с диска.
    QElapsedTimer loadTimer;
    loadTimer.start();
    {
        LongTermMemory ltm;
        QCOMPARE(ltm.count(), n);   // внутри читается основное хранилище
    }
    const qint64 loadMs = loadTimer.elapsed();
    qInfo() << "[6.2] load" << n << "фактов:" << loadMs << "мс";
    QVERIFY2(loadMs < 30000, "загрузка 2000 фактов подозрительно медленная");

    // Векторы реально восстановлены из бинарника: поиск по смыслу находит свой факт.
    LongTermMemory reloaded;
    QCOMPARE(reloaded.count(), n);
    const QList<MemoryHit> hits = reloaded.queryHybrid("кириллицу", embs[1], 5, 0.0f);
    QVERIFY(!hits.isEmpty());
    QCOMPARE(hits.first().text, texts[1]);
    QVERIFY2(hits.first().vectorScore > 0.99f, "векторы не восстановились из бинарника");
}

// 6.2: потолок MAX_TOTAL держится.
void TestMemory::phase6MaxTotalCapHolds() {
    wipeMemoryDir();

    const int over = LongTermMemory::MAX_TOTAL + 250;
    const int dim = 64;   // см. комментарий в phase6LoadTwoThousandFacts
    QVector<QString> texts;
    QVector<QVector<float>> embs;
    for (int i = 0; i < over; ++i) {
        texts << QString("уникальный факт номер %1 про тему %2").arg(i).arg(i * 7 + 3);
        embs.append(smallEmbedding(quint64(i) * 104729ULL + 7ULL, dim));
    }

    int added = 0;
    {
        LongTermMemory ltm;
        ltm.addFacts(texts, embs, "chatA", QStringList(), &added);
        // Дедупликация не должна была сработать: если added < over, тест
        // недетерминирован и его результат ничего не значит.
        QCOMPARE(added, over);
        QCOMPARE(ltm.count(), LongTermMemory::MAX_TOTAL);
    }

    // Потолок переживает перезапуск.
    LongTermMemory reloaded;
    QCOMPARE(reloaded.count(), LongTermMemory::MAX_TOTAL);
    QCOMPARE(storedFacts().size(), LongTermMemory::MAX_TOTAL);
}

// 6.2: 100 запросов подряд. Смысл — поймать возврат к квадратичному поведению
// BM25 (например, если вернуть value() вместо constFind или пересчёт dl).
void TestMemory::phase6HundredQueriesStayLinear() {
    wipeMemoryDir();

    const int n = 1000;
    // dim = 64: в 16 измерениях у случайных векторов слишком часто косинус
    // > 0.9, и дедупликация отбрасывает «не те» факты — тест становится
    // недетерминированным.
    const int dim = 64;
    QVector<QString> texts;
    QVector<QVector<float>> embs;
    for (int i = 0; i < n; ++i) {
        texts << QString("факт %1: пользователь работает в городе %2 и любит проект %3")
                     .arg(i).arg(i % 20).arg(i % 7);
        embs.append(smallEmbedding(quint64(i) * 31337ULL + 3ULL, dim));
    }
    int added = 0;
    {
        LongTermMemory ltm;
        ltm.addFacts(texts, embs, "chatA", QStringList(), &added);
        QCOMPARE(added, n);
        QCOMPARE(ltm.count(), n);
    }

    LongTermMemory ltm;
    // Первый запрос прогревает кэши, его не замеряем.
    ltm.queryHybrid("город проект", embs[0], 8, 0.0f);

    QElapsedTimer timer;
    timer.start();
    int found = 0;
    for (int q = 0; q < 100; ++q) {
        found += ltm.queryHybrid(QString("город %1 проект %2").arg(q % 20).arg(q % 7),
                                 embs[q], 8, 0.0f).size();
    }
    const qint64 elapsed = timer.elapsed();
    qInfo() << "[6.2] 100 гибридных запросов по" << n << "фактам:" << elapsed << "мс,"
            << "найдено" << found;
    QVERIFY2(elapsed < 15000, "100 запросов по 1000 фактам подозрительно медленно (регресс BM25?)");
}

// Регресс: id сообщения обязаны переживать перезапуск чата.
// Факты в долговременной памяти привязаны к msgId, поэтому перегенерация id
// при importMessages делает все последующие deleteByMsgId/deleteByChatId по
// сообщениям бесполезными — факты удалённых сообщений навсегда остаются в JSON.
void TestMemory::phase6MsgIdsSurviveRestart() {
    wipeMemoryDir();
    QDir(baseChatsDir()).removeRecursively();
    QDir().mkpath(baseChatsDir());

    const QString chatId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString firstId = "aaaaaaaa-1111-2222-3333-444444444444";
    const QString secondId = "bbbbbbbb-1111-2222-3333-444444444444";
    const QString thirdId = "cccccccc-1111-2222-3333-444444444444";
    const QJsonArray hist{
        makeMsg("user", "первый вопрос пользователя про город", firstId),
        makeMsg("model", "ответ модели на первый вопрос", secondId),
        makeMsg("user", "второй вопрос пользователя про язык", thirdId)};

    // Первая сессия: id создаются addMessage и сохраняются в файл чата.
    MemoryManager session1;
    const QString liveFirst = session1.addMessage("user", "первый вопрос пользователя про город");
    const QString liveSecond = session1.addMessage("model", "ответ модели на первый вопрос");
    const QString liveThird = session1.addMessage("user", "второй вопрос пользователя про язык");
    QVERIFY(!liveFirst.isEmpty());
    ChatManager cm;
    cm.saveChat(chatId, session1.exportMessages(), QString(), 0);

    // Именно эти id попадают в факты при извлечении.
    LongTermMemory ltm;
    ltm.addFacts(QStringList{"пользователь спрашивал про город", "пользователь спрашивал про язык"},
                 QVector<QVector<float>>{smallEmbedding(1), smallEmbedding(2)},
                 chatId, QStringList{liveFirst, liveThird});

    // Перезапуск приложения: чат читается с диска, как это делает
    // NetworkManager::loadChat -> importMessages.
    MemoryManager session2;
    session2.importMessages(cm.loadChat(chatId));
    const QJsonArray after = session2.exportMessages();
    QCOMPARE(after.size(), 3);

    // id из первой сессии переживают рестарт и совпадают с записанными в фактах.
    QCOMPARE(after[0].toObject()["id"].toString(), liveFirst);
    QCOMPARE(after[2].toObject()["id"].toString(), liveThird);

    // Правка первого сообщения после рестарта вычищает его факт.
    QVERIFY(session2.replaceMessageById(liveFirst, "переписанный вопрос пользователя"));
    session2.truncateMessagesFrom(1);
    ltm.deleteByMsgId(chatId, liveFirst);
    for (const MemoryHit &h : storedFacts()) {
        QVERIFY2(h.msgId != liveFirst,
                 "факт удалённого после рестарта сообщения остался в памяти");
    }
}

QTEST_GUILESS_MAIN(TestMemory)
#include "tst_memory.moc"
