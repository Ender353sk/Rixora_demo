#include "sqlitememoryrepository.h"
#include "constants.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QDebug>
#include <QtMath>

SqliteMemoryRepository::SqliteMemoryRepository(QObject *parent)
    : QObject(parent)
{
}

SqliteMemoryRepository::~SqliteMemoryRepository()
{
    closeDatabase();
}

bool SqliteMemoryRepository::initialize(const QString &dbPath)
{
    QString path = dbPath;
    if (path.isEmpty()) {
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(path);
        path += "/rixora_memory.db";
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", "rixora_memory_conn");
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        qDebug() << "[SqliteMemoryRepository] Не вдалося відкрити БД:" << m_db.lastError().text();
        return false;
    }

    qDebug() << "[SqliteMemoryRepository] БД відкрито:" << path;

    if (!execDDL()) {
        qDebug() << "[SqliteMemoryRepository] Помилка виконання DDL";
        return false;
    }

    return true;
}

bool SqliteMemoryRepository::execDDL()
{
    // Схема БД вбудована безпосередньо у код, щоб не залежати від шляху до файлу.
    const QString ddl = R"(
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

CREATE TABLE IF NOT EXISTS chats (
    id           TEXT PRIMARY KEY,
    title        TEXT NOT NULL DEFAULT '',
    created_at   INTEGER NOT NULL,
    updated_at   INTEGER NOT NULL,
    message_count INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS messages (
    id            TEXT PRIMARY KEY,
    chat_id       TEXT NOT NULL,
    role          TEXT NOT NULL CHECK (role IN ('user', 'model')),
    text          TEXT NOT NULL,
    ts            INTEGER NOT NULL,
    order_index   INTEGER NOT NULL,
    FOREIGN KEY (chat_id) REFERENCES chats(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_messages_chat_id ON messages(chat_id);
CREATE INDEX IF NOT EXISTS idx_messages_chat_order ON messages(chat_id, order_index);

CREATE TABLE IF NOT EXISTS chat_memory (
    chat_id           TEXT PRIMARY KEY,
    mid_term_summary  TEXT NOT NULL DEFAULT '',
    summarized_count  INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (chat_id) REFERENCES chats(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS ltm_facts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    text        TEXT NOT NULL,
    vector_blob BLOB,
    chat_id     TEXT,
    msg_id      TEXT,
    ts          INTEGER NOT NULL,
    signature   INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_ltm_facts_chat_id ON ltm_facts(chat_id);
CREATE INDEX IF NOT EXISTS idx_ltm_facts_msg_id ON ltm_facts(msg_id);
CREATE INDEX IF NOT EXISTS idx_ltm_facts_ts ON ltm_facts(ts);

CREATE TABLE IF NOT EXISTS extraction_runs (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    run_at      INTEGER NOT NULL,
    chat_id     TEXT,
    facts_added INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS backfill_state (
    id            INTEGER PRIMARY KEY CHECK (id = 1),
    backfill_index INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS audit_log (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    action      TEXT NOT NULL,
    table_name  TEXT NOT NULL,
    record_id   TEXT,
    old_values  TEXT,
    new_values  TEXT,
    changed_at  INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS access_roles (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    role_name   TEXT NOT NULL UNIQUE,
    can_read    INTEGER NOT NULL DEFAULT 1,
    can_write   INTEGER NOT NULL DEFAULT 0,
    can_admin   INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS search_log (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    query_text       TEXT NOT NULL,
    results_count    INTEGER NOT NULL DEFAULT 0,
    exec_time_ms     INTEGER NOT NULL DEFAULT 0,
    executed_at      INTEGER NOT NULL
);

CREATE VIEW IF NOT EXISTS v_last_message AS
SELECT m.chat_id, m.text AS last_text, m.ts AS last_ts, m.role AS last_role
FROM messages m
WHERE m.id = (
    SELECT id FROM messages m2 WHERE m2.chat_id = m.chat_id ORDER BY order_index DESC LIMIT 1
);

CREATE VIEW IF NOT EXISTS v_fact_stats AS
SELECT chat_id, COUNT(*) AS fact_count, MAX(ts) AS last_fact_ts
FROM ltm_facts
GROUP BY chat_id;

CREATE TRIGGER IF NOT EXISTS trg_chats_insert_audit
AFTER INSERT ON chats
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, new_values, changed_at)
    VALUES ('INSERT', 'chats', NEW.id, json_object('title', NEW.title, 'created_at', NEW.created_at), strftime('%s', 'now') * 1000);
END;

CREATE TRIGGER IF NOT EXISTS trg_chats_update_audit
AFTER UPDATE ON chats
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, old_values, new_values, changed_at)
    VALUES ('UPDATE', 'chats', NEW.id,
        json_object('title', OLD.title, 'updated_at', OLD.updated_at),
        json_object('title', NEW.title, 'updated_at', NEW.updated_at),
        strftime('%s', 'now') * 1000);
END;

CREATE TRIGGER IF NOT EXISTS trg_messages_delete_audit
AFTER DELETE ON messages
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, old_values, changed_at)
    VALUES ('DELETE', 'messages', OLD.id,
        json_object('chat_id', OLD.chat_id, 'role', OLD.role, 'text', OLD.text),
        strftime('%s', 'now') * 1000);
END;

CREATE TRIGGER IF NOT EXISTS trg_ltm_facts_insert_audit
AFTER INSERT ON ltm_facts
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, new_values, changed_at)
    VALUES ('INSERT', 'ltm_facts', CAST(NEW.id AS TEXT),
        json_object('text', NEW.text, 'chat_id', NEW.chat_id, 'ts', NEW.ts),
        strftime('%s', 'now') * 1000);
END;

CREATE TRIGGER IF NOT EXISTS trg_ltm_facts_before_insert_check
BEFORE INSERT ON ltm_facts
FOR EACH ROW
WHEN LENGTH(TRIM(NEW.text)) < 8
BEGIN
    SELECT RAISE(ABORT, 'Текст факту занадто короткий (менше 8 символів)');
END;

INSERT OR IGNORE INTO access_roles(role_name, can_read, can_write, can_admin)
VALUES ('reader', 1, 0, 0),
       ('writer', 1, 1, 0),
       ('admin', 1, 1, 1);

INSERT OR IGNORE INTO backfill_state(id, backfill_index) VALUES (1, 0);
)";

    // Виконуємо увесь DDL як один запит — SQLite підтримує декілька операторів,
    // розділених ';', у одному виклику exec(). Це важливо для тригерів, які
    // містять ';' всередині BEGIN…END блоків: naïve розбиття по ';' bilo би
    // визначення тригера на частини.
    QSqlQuery query(m_db);
    if (!query.exec(ddl)) {
        qDebug() << "[SqliteMemoryRepository] DDL помилка:" << query.lastError().text();
        return false;
    }
    return true;
}

void SqliteMemoryRepository::closeDatabase()
{
    if (m_db.isOpen()) {
        m_db.close();
        QSqlDatabase::removeDatabase("rixora_memory_conn");
    }
}

// ============================================================================
// Чати
// ============================================================================

bool SqliteMemoryRepository::upsertChat(const QString &chatId, const QString &title,
                                        qint64 createdAt, qint64 updatedAt, int messageCount)
{
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO chats(id, title, created_at, updated_at, message_count) "
        "VALUES(:id, :title, :created_at, :updated_at, :message_count) "
        "ON CONFLICT(id) DO UPDATE SET "
        "title=excluded.title, updated_at=excluded.updated_at, message_count=excluded.message_count");
    query.bindValue(":id", chatId);
    query.bindValue(":title", title);
    query.bindValue(":created_at", createdAt);
    query.bindValue(":updated_at", updatedAt);
    query.bindValue(":message_count", messageCount);

    if (!query.exec()) {
        qDebug() << "[SqliteMemoryRepository] upsertChat помилка:" << query.lastError().text();
        return false;
    }
    return true;
}

bool SqliteMemoryRepository::deleteChat(const QString &chatId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM chats WHERE id = :id");
    query.bindValue(":id", chatId);
    if (!query.exec()) {
        qDebug() << "[SqliteMemoryRepository] deleteChat помилка:" << query.lastError().text();
        return false;
    }
    return true;
}

bool SqliteMemoryRepository::deleteAllChats()
{
    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM chats")) {
        qDebug() << "[SqliteMemoryRepository] deleteAllChats помилка:" << query.lastError().text();
        return false;
    }
    return true;
}

QJsonArray SqliteMemoryRepository::getChatList() const
{
    QJsonArray result;
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, title, created_at, updated_at, message_count FROM chats ORDER BY updated_at DESC")) {
        qDebug() << "[SqliteMemoryRepository] getChatList помилка:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        QJsonObject obj;
        obj["id"] = query.value(0).toString();
        obj["title"] = query.value(1).toString();
        obj["createdAt"] = query.value(2).toLongLong();
        obj["updatedAt"] = query.value(3).toLongLong();
        obj["messageCount"] = query.value(4).toInt();
        result.append(obj);
    }
    return result;
}

QJsonArray SqliteMemoryRepository::getChatListOldestFirst() const
{
    QJsonArray result;
    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, title, created_at, updated_at, message_count FROM chats ORDER BY created_at ASC")) {
        qDebug() << "[SqliteMemoryRepository] getChatListOldestFirst помилка:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        QJsonObject obj;
        obj["id"] = query.value(0).toString();
        obj["title"] = query.value(1).toString();
        obj["createdAt"] = query.value(2).toLongLong();
        obj["updatedAt"] = query.value(3).toLongLong();
        obj["messageCount"] = query.value(4).toInt();
        result.append(obj);
    }
    return result;
}

bool SqliteMemoryRepository::chatExists(const QString &chatId) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM chats WHERE id = :id");
    query.bindValue(":id", chatId);
    if (!query.exec() || !query.next()) return false;
    return query.value(0).toInt() > 0;
}

QString SqliteMemoryRepository::getCurrentChatId() const
{
    return m_currentChatId;
}

void SqliteMemoryRepository::setCurrentChatId(const QString &chatId)
{
    m_currentChatId = chatId;
}

// ============================================================================
// Повідомлення
// ============================================================================

bool SqliteMemoryRepository::saveMessages(const QString &chatId, const QJsonArray &messages)
{
    QSqlQuery query(m_db);
    m_db.transaction();

    // Видаляємо старі повідомлення чату
    query.prepare("DELETE FROM messages WHERE chat_id = :chat_id");
    query.bindValue(":chat_id", chatId);
    if (!query.exec()) {
        m_db.rollback();
        qDebug() << "[SqliteMemoryRepository] saveMessages delete помилка:" << query.lastError().text();
        return false;
    }

    // Вставляємо нові
    query.prepare(
        "INSERT INTO messages(id, chat_id, role, text, ts, order_index) "
        "VALUES(:id, :chat_id, :role, :text, :ts, :order_index)");
    for (int i = 0; i < messages.size(); ++i) {
        QJsonObject msg = messages[i].toObject();
        query.bindValue(":id", msg.value("id").toString());
        query.bindValue(":chat_id", chatId);
        query.bindValue(":role", msg.value("role").toString());
        query.bindValue(":text", msg.value("parts").toArray()[0].toObject().value("text").toString());
        query.bindValue(":ts", QDateTime::currentMSecsSinceEpoch());
        query.bindValue(":order_index", i);
        if (!query.exec()) {
            m_db.rollback();
            qDebug() << "[SqliteMemoryRepository] saveMessages insert помилка:" << query.lastError().text();
            return false;
        }
    }

    // Оновлюємо лічильник повідомлень чату
    query.prepare("UPDATE chats SET message_count = :count, updated_at = :ts WHERE id = :id");
    query.bindValue(":count", messages.size());
    query.bindValue(":ts", QDateTime::currentMSecsSinceEpoch());
    query.bindValue(":id", chatId);
    if (!query.exec()) {
        m_db.rollback();
        qDebug() << "[SqliteMemoryRepository] saveMessages update chat помилка:" << query.lastError().text();
        return false;
    }

    if (!m_db.commit()) {
        qDebug() << "[SqliteMemoryRepository] saveMessages commit помилка:" << m_db.lastError().text();
        return false;
    }
    return true;
}

QJsonArray SqliteMemoryRepository::loadMessages(const QString &chatId) const
{
    QJsonArray result;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, role, text, ts, order_index FROM messages "
        "WHERE chat_id = :chat_id ORDER BY order_index ASC");
    query.bindValue(":chat_id", chatId);
    if (!query.exec()) {
        qDebug() << "[SqliteMemoryRepository] loadMessages помилка:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        QJsonObject msg;
        msg["id"] = query.value(0).toString();
        msg["role"] = query.value(1).toString();
        msg["parts"] = QJsonArray{QJsonObject{{"text", query.value(2).toString()}}};
        msg["ts"] = query.value(3).toLongLong();
        result.append(msg);
    }
    return result;
}

QJsonArray SqliteMemoryRepository::peekMessages(const QString &chatId, int lastN,
                                                int skipFromEnd) const
{
    QJsonArray result;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT id, role, text, ts, order_index FROM messages "
        "WHERE chat_id = :chat_id ORDER BY order_index DESC LIMIT :limit OFFSET :offset");
    query.bindValue(":chat_id", chatId);
    query.bindValue(":limit", lastN);
    query.bindValue(":offset", skipFromEnd);
    if (!query.exec()) {
        qDebug() << "[SqliteMemoryRepository] peekMessages помилка:" << query.lastError().text();
        return result;
    }
    QJsonArray temp;
    while (query.next()) {
        QJsonObject msg;
        msg["id"] = query.value(0).toString();
        msg["role"] = query.value(1).toString();
        msg["parts"] = QJsonArray{QJsonObject{{"text", query.value(2).toString()}}};
        msg["ts"] = query.value(3).toLongLong();
        temp.append(msg);
    }
    // Повертаємо у хронологічному порядку (старші першими)
    for (int i = temp.size() - 1; i >= 0; --i) {
        result.append(temp[i]);
    }
    return result;
}

bool SqliteMemoryRepository::messageExists(const QString &msgId) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT COUNT(*) FROM messages WHERE id = :id");
    query.bindValue(":id", msgId);
    if (!query.exec() || !query.next()) return false;
    return query.value(0).toInt() > 0;
}

// ============================================================================
// Середньострокова пам'ять (свідка)
// ============================================================================

bool SqliteMemoryRepository::saveChatMemory(const QString &chatId, const QString &midTermSummary,
                                            int summarizedCount)
{
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO chat_memory(chat_id, mid_term_summary, summarized_count) "
        "VALUES(:chat_id, :summary, :count) "
        "ON CONFLICT(chat_id) DO UPDATE SET "
        "mid_term_summary=excluded.mid_term_summary, summarized_count=excluded.summarized_count");
    query.bindValue(":chat_id", chatId);
    // QString() за замовчуванням — null, що bind'ить як SQL NULL.
    // chat_memory.mid_term_summary має NOT NULL, тому форсуємо порожній рядок.
    query.bindValue(":summary", midTermSummary.isNull() ? QString("") : midTermSummary);
    query.bindValue(":count", summarizedCount);
    if (!query.exec()) {
        qDebug() << "[SqliteMemoryRepository] saveChatMemory помилка:" << query.lastError().text();
        return false;
    }
    return true;
}

QJsonObject SqliteMemoryRepository::loadChatMemory(const QString &chatId) const
{
    QJsonObject result;
    QSqlQuery query(m_db);
    query.prepare("SELECT mid_term_summary, summarized_count FROM chat_memory WHERE chat_id = :chat_id");
    query.bindValue(":chat_id", chatId);
    if (!query.exec() || !query.next()) return result;
    result["midTermSummary"] = query.value(0).toString();
    result["summarizedCount"] = query.value(1).toInt();
    return result;
}

// ============================================================================
// Довгострокові факти
// ============================================================================

QByteArray SqliteMemoryRepository::serializeVector(const QVector<float> &v)
{
    QByteArray data;
    data.reserve(v.size() * sizeof(float));
    for (float val : v) {
        data.append(reinterpret_cast<const char*>(&val), sizeof(float));
    }
    return data;
}

QVector<float> SqliteMemoryRepository::deserializeVector(const QByteArray &blob)
{
    QVector<float> v;
    int count = blob.size() / sizeof(float);
    v.reserve(count);
    for (int i = 0; i < count; ++i) {
        float val;
        memcpy(&val, blob.constData() + i * sizeof(float), sizeof(float));
        v.append(val);
    }
    return v;
}

bool SqliteMemoryRepository::addFact(const QString &text, const QVector<float> &embedding,
                                     const QString &chatId, const QString &msgId)
{
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO ltm_facts(text, vector_blob, chat_id, msg_id, ts, signature) "
        "VALUES(:text, :vector, :chat_id, :msg_id, :ts, :signature)");
    query.bindValue(":text", text.trimmed());
    query.bindValue(":vector", serializeVector(embedding));
    query.bindValue(":chat_id", chatId);
    query.bindValue(":msg_id", msgId);
    query.bindValue(":ts", QDateTime::currentMSecsSinceEpoch());
    query.bindValue(":signature", 0); //퀀т16 сигнатура — для простоти 0, обчислюється на боку Qt
    if (!query.exec()) {
        qDebug() << "[SqliteMemoryRepository] addFact помилка:" << query.lastError().text();
        return false;
    }
    return true;
}

bool SqliteMemoryRepository::addFacts(const QVector<QString> &texts,
                                      const QVector<QVector<float>> &embeddings,
                                      const QString &chatId, const QVector<QString> &msgIds,
                                      int *addedCount)
{
    int added = 0;
    QSqlQuery query(m_db);
    m_db.transaction();

    for (int i = 0; i < texts.size(); ++i) {
        const QString &text = texts[i];
        const QString trimmed = text.trimmed();
        if (trimmed.isEmpty() || trimmed.size() < 8) continue;

        const QVector<float> embedding = i < embeddings.size() ? embeddings.at(i) : QVector<float>();
        const QString msgId = i < msgIds.size() ? msgIds.at(i) : QString();

        query.prepare(
            "INSERT INTO ltm_facts(text, vector_blob, chat_id, msg_id, ts, signature) "
            "VALUES(:text, :vector, :chat_id, :msg_id, :ts, :signature)");
        query.bindValue(":text", trimmed);
        query.bindValue(":vector", serializeVector(embedding));
        query.bindValue(":chat_id", chatId);
        query.bindValue(":msg_id", msgId);
        query.bindValue(":ts", QDateTime::currentMSecsSinceEpoch());
        query.bindValue(":signature", 0);
        if (query.exec()) {
            ++added;
        }
    }

    if (!m_db.commit()) {
        qDebug() << "[SqliteMemoryRepository] addFacts commit помилка:" << m_db.lastError().text();
        if (addedCount) *addedCount = 0;
        return false;
    }

    if (addedCount) *addedCount = added;
    return true;
}

QList<MemoryHit> SqliteMemoryRepository::queryLexical(const QString &queryText, int topK,
                                                       float threshold) const
{
    QList<MemoryHit> results;
    QSqlQuery query(m_db);
    // SQLite FTS не підключено, тому простий LIKE-пошук як лексичний фолбек
    query.prepare(
        "SELECT text, chat_id, msg_id, ts FROM ltm_facts "
        "WHERE text LIKE :query ORDER BY ts DESC LIMIT :topK");
    query.bindValue(":query", "%" + queryText + "%");
    query.bindValue(":topK", topK);
    if (!query.exec()) {
        qDebug() << "[SqliteMemoryRepository] queryLexical помилка:" << query.lastError().text();
        return results;
    }
    while (query.next()) {
        MemoryHit hit;
        hit.text = query.value(0).toString();
        hit.kind = "fact";
        hit.chatId = query.value(1).toString();
        hit.msgId = query.value(2).toString();
        hit.ts = query.value(3).toLongLong();
        hit.score = 0.5f;
        hit.vectorScore = 0.0f;
        hit.lexicalScore = 0.5f;
        results.append(hit);
    }
    return results;
}

QList<MemoryHit> SqliteMemoryRepository::queryHybrid(const QString &queryText,
                                                      const QVector<float> &queryEmbedding,
                                                      int topK, float threshold,
                                                      QList<MemoryHit> *lexicalOut) const
{
    QList<MemoryHit> vecHits;
    QList<MemoryHit> lexHits;

    // Лексичний пошук
    lexHits = queryLexical(queryText, topK, threshold);
    if (lexicalOut) *lexicalOut = lexHits;

    // Векторний пошук — завантажуємо всі факти з векторами та обчислюємо косинус на боку Qt
    QSqlQuery query(m_db);
    if (!query.exec("SELECT text, chat_id, msg_id, ts, vector_blob FROM ltm_facts WHERE vector_blob IS NOT NULL")) {
        qDebug() << "[SqliteMemoryRepository] queryHybrid vector помилка:" << query.lastError().text();
        return vecHits;
    }

    struct FactWithScore {
        MemoryHit hit;
        float score;
    };
    QVector<FactWithScore> scored;

    while (query.next()) {
        QByteArray blob = query.value(4).toByteArray();
        QVector<float> emb = deserializeVector(blob);
        if (emb.size() != queryEmbedding.size() || emb.isEmpty()) continue;

        // Косинусна схожість
        double dot = 0.0, normA = 0.0, normB = 0.0;
        for (int i = 0; i < emb.size(); ++i) {
            dot += double(emb[i]) * double(queryEmbedding[i]);
            normA += double(emb[i]) * double(emb[i]);
            normB += double(queryEmbedding[i]) * double(queryEmbedding[i]);
        }
        if (normA == 0.0 || normB == 0.0) continue;
        float sim = float(dot / (qSqrt(normA) * qSqrt(normB)));

        MemoryHit hit;
        hit.text = query.value(0).toString();
        hit.kind = "fact";
        hit.chatId = query.value(1).toString();
        hit.msgId = query.value(2).toString();
        hit.ts = query.value(3).toLongLong();
        hit.vectorScore = sim;
        hit.score = sim;
        scored.append({hit, sim});
    }

    // Сортуємо за схожістю (спадання)
    std::sort(scored.begin(), scored.end(),
              [](const FactWithScore &a, const FactWithScore &b) { return a.score > b.score; });

    for (int i = 0; i < qMin(topK, scored.size()); ++i) {
        vecHits.append(scored[i].hit);
    }

    // Об'єднуємо векторний і лексичний (дедукція за текстом)
    QList<MemoryHit> merged;
    QHash<QString, bool> seen;
    for (const MemoryHit &h : vecHits) {
        if (!seen.contains(h.text)) {
            seen.insert(h.text, true);
            merged.append(h);
        }
    }
    for (const MemoryHit &h : lexHits) {
        if (!seen.contains(h.text)) {
            seen.insert(h.text, true);
            merged.append(h);
        }
    }

    // Сортировка за score
    std::sort(merged.begin(), merged.end(),
              [](const MemoryHit &a, const MemoryHit &b) { return a.score > b.score; });

    while (merged.size() > topK) merged.removeLast();
    return merged;
}

QList<MemoryHit> SqliteMemoryRepository::recentFacts(int n) const
{
    QList<MemoryHit> results;
    QSqlQuery query(m_db);
    query.prepare(
        "SELECT text, chat_id, msg_id, ts FROM ltm_facts ORDER BY ts DESC LIMIT :n");
    query.bindValue(":n", n);
    if (!query.exec()) {
        qDebug() << "[SqliteMemoryRepository] recentFacts помилка:" << query.lastError().text();
        return results;
    }
    while (query.next()) {
        MemoryHit hit;
        hit.text = query.value(0).toString();
        hit.kind = "fact";
        hit.chatId = query.value(1).toString();
        hit.msgId = query.value(2).toString();
        hit.ts = query.value(3).toLongLong();
        hit.score = 0.5f;
        results.append(hit);
    }
    return results;
}

bool SqliteMemoryRepository::deleteFactsByChatId(const QString &chatId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM ltm_facts WHERE chat_id = :chat_id");
    query.bindValue(":chat_id", chatId);
    if (!query.exec()) {
        qDebug() << "[SqliteMemoryRepository] deleteFactsByChatId помилка:" << query.lastError().text();
        return false;
    }
    return true;
}

bool SqliteMemoryRepository::deleteFactsByMsgId(const QString &chatId, const QString &msgId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM ltm_facts WHERE chat_id = :chat_id AND msg_id = :msg_id");
    query.bindValue(":chat_id", chatId);
    query.bindValue(":msg_id", msgId);
    if (!query.exec()) {
        qDebug() << "[SqliteMemoryRepository] deleteFactsByMsgId помилка:" << query.lastError().text();
        return false;
    }
    return true;
}

bool SqliteMemoryRepository::deleteAllFacts()
{
    QSqlQuery query(m_db);
    if (!query.exec("DELETE FROM ltm_facts")) {
        qDebug() << "[SqliteMemoryRepository] deleteAllFacts помилка:" << query.lastError().text();
        return false;
    }
    return true;
}

int SqliteMemoryRepository::factCount() const
{
    QSqlQuery query(m_db);
    if (!query.exec("SELECT COUNT(*) FROM ltm_facts")) return 0;
    query.next();
    return query.value(0).toInt();
}

bool SqliteMemoryRepository::hasFacts() const
{
    return factCount() > 0;
}

// ============================================================================
// Екстракція / бэкфілл
// ============================================================================

bool SqliteMemoryRepository::canRunExtraction() const
{
    // SQLite не має денного ліміту в БД — перевіряється на боку LongTermMemory
    return true;
}

void SqliteMemoryRepository::noteExtractionRun()
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO extraction_runs(run_at, chat_id, facts_added) VALUES(:ts, '', 0)");
    query.bindValue(":ts", QDateTime::currentMSecsSinceEpoch());
    query.exec();
}

int SqliteMemoryRepository::backfillIndex() const
{
    QSqlQuery query(m_db);
    if (!query.exec("SELECT backfill_index FROM backfill_state WHERE id = 1")) return 0;
    if (query.next()) return query.value(0).toInt();
    return 0;
}

QHash<QString, int> SqliteMemoryRepository::scannedChatMsgs() const
{
    QHash<QString, int> result;
    // backfill_state зберігає тільки індекс, скановані чати — в JSON-подібному форматі
    // для простоти повертаємо порожній хеш (керується на боку LongTermMemory)
    return result;
}

void SqliteMemoryRepository::saveBackfillCursor(int index, const QHash<QString, int> &scanned)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE backfill_state SET backfill_index = :idx WHERE id = 1");
    query.bindValue(":idx", index);
    query.exec();
}

// ============================================================================
// Аудит
// ============================================================================

void SqliteMemoryRepository::writeAudit(const QString &action, const QString &tableName,
                                        const QString &recordId, const QJsonObject &oldValues,
                                        const QJsonObject &newValues)
{
    QSqlQuery query(m_db);
    query.prepare(
        "INSERT INTO audit_log(action, table_name, record_id, old_values, new_values, changed_at) "
        "VALUES(:action, :table, :record, :old, :new, :ts)");
    query.bindValue(":action", action);
    query.bindValue(":table", tableName);
    query.bindValue(":record", recordId);
    query.bindValue(":old", QJsonDocument(oldValues).toJson(QJsonDocument::Compact));
    query.bindValue(":new", QJsonDocument(newValues).toJson(QJsonDocument::Compact));
    query.bindValue(":ts", QDateTime::currentMSecsSinceEpoch());
    query.exec();
}

// ============================================================================
// Резервне копіювання / відновлення
// ============================================================================

bool SqliteMemoryRepository::backupDatabase(const QString &backupPath) const
{
    QFile dbFile(m_db.databaseName());
    if (!dbFile.exists()) return false;
    return dbFile.copy(backupPath);
}

bool SqliteMemoryRepository::restoreDatabase(const QString &backupPath)
{
    closeDatabase();
    QFile::copy(backupPath, m_db.databaseName());
    return m_db.open();
}
