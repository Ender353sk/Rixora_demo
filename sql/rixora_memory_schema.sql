-- ============================================================================
-- Схема бази даних підсистеми довготривалої пам'яті AI-чат-асистента Ріксора
-- СУБД: SQLite (offline-first, крос-платформенність)
-- Курсова робота з дисципліни «Бази даних»
-- ============================================================================

PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

-- ---------------------------------------------------------------------------
-- 1. Чати (метадані кожного чату)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS chats (
    id           TEXT PRIMARY KEY,          -- QUuid без дужок
    title        TEXT NOT NULL DEFAULT '',
    created_at   INTEGER NOT NULL,          -- ms epoch
    updated_at   INTEGER NOT NULL,
    message_count INTEGER NOT NULL DEFAULT 0
);

-- ---------------------------------------------------------------------------
-- 2. Повідомлення (короткострокова історія chat-у)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS messages (
    id            TEXT PRIMARY KEY,          -- QUuid повідомлення
    chat_id       TEXT NOT NULL,
    role          TEXT NOT NULL CHECK (role IN ('user', 'model')),
    text          TEXT NOT NULL,
    ts            INTEGER NOT NULL,          -- ms epoch
    order_index   INTEGER NOT NULL,          -- позиція в чаті
    FOREIGN KEY (chat_id) REFERENCES chats(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_messages_chat_id ON messages(chat_id);
CREATE INDEX IF NOT EXISTS idx_messages_chat_order ON messages(chat_id, order_index);

-- ---------------------------------------------------------------------------
-- 3. Середньострокова пам'ять (свідка chat-у)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS chat_memory (
    chat_id           TEXT PRIMARY KEY,
    mid_term_summary  TEXT NOT NULL DEFAULT '',
    summarized_count  INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY (chat_id) REFERENCES chats(id) ON DELETE CASCADE
);

-- ---------------------------------------------------------------------------
-- 4. Довгострокові факти (векторна пам'ять)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS ltm_facts (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    text        TEXT NOT NULL,
    vector_blob BLOB,                      -- serialize QVector<float> як raw bytes
    chat_id     TEXT,
    msg_id      TEXT,
    ts          INTEGER NOT NULL,          -- ms epoch
    signature   INTEGER NOT NULL DEFAULT 0 -- quint16 сигнатура для швидкого відбору
);

CREATE INDEX IF NOT EXISTS idx_ltm_facts_chat_id ON ltm_facts(chat_id);
CREATE INDEX IF NOT EXISTS idx_ltm_facts_msg_id ON ltm_facts(msg_id);
CREATE INDEX IF NOT EXISTS idx_ltm_facts_ts ON ltm_facts(ts);

-- ---------------------------------------------------------------------------
-- 5. Журнал запусків екстракції фактів
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS extraction_runs (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    run_at      INTEGER NOT NULL,          -- ms epoch
    chat_id     TEXT,
    facts_added INTEGER NOT NULL DEFAULT 0
);

-- ---------------------------------------------------------------------------
-- 6. Курсор бэкфіллу старих чатів
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS backfill_state (
    id            INTEGER PRIMARY KEY CHECK (id = 1),
    backfill_index INTEGER NOT NULL DEFAULT 0
);

-- ---------------------------------------------------------------------------
-- 7. Журнал аудиту змін
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS audit_log (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    action      TEXT NOT NULL,             -- INSERT/UPDATE/DELETE
    table_name  TEXT NOT NULL,
    record_id   TEXT,
    old_values  TEXT,                      -- JSON
    new_values  TEXT,                      -- JSON
    changed_at  INTEGER NOT NULL           -- ms epoch
);

-- ---------------------------------------------------------------------------
-- 8. Ролі доступу
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS access_roles (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    role_name   TEXT NOT NULL UNIQUE,
    can_read    INTEGER NOT NULL DEFAULT 1,
    can_write   INTEGER NOT NULL DEFAULT 0,
    can_admin   INTEGER NOT NULL DEFAULT 0
);

-- ---------------------------------------------------------------------------
-- 9. Журнал пошукових запитів (для EXPLAIN / аналізу продуктивності)
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS search_log (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    query_text       TEXT NOT NULL,
    results_count    INTEGER NOT NULL DEFAULT 0,
    exec_time_ms     INTEGER NOT NULL DEFAULT 0,
    executed_at      INTEGER NOT NULL
);

-- ---------------------------------------------------------------------------
-- Представлення (views)
-- ---------------------------------------------------------------------------

-- Останнє повідомлення кожного чату (для списку історії)
CREATE VIEW IF NOT EXISTS v_last_message AS
SELECT m.chat_id, m.text AS last_text, m.ts AS last_ts, m.role AS last_role
FROM messages m
WHERE m.id = (
    SELECT id FROM messages m2 WHERE m2.chat_id = m.chat_id ORDER BY order_index DESC LIMIT 1
);

-- Статистика факів по чатах
CREATE VIEW IF NOT EXISTS v_fact_stats AS
SELECT chat_id, COUNT(*) AS fact_count, MAX(ts) AS last_fact_ts
FROM ltm_facts
GROUP BY chat_id;

-- ---------------------------------------------------------------------------
-- Тригери аудиту
-- ---------------------------------------------------------------------------

-- Аудит вставки чату
CREATE TRIGGER IF NOT EXISTS trg_chats_insert_audit
AFTER INSERT ON chats
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, new_values, changed_at)
    VALUES ('INSERT', 'chats', NEW.id, json_object('title', NEW.title, 'created_at', NEW.created_at), strftime('%s', 'now') * 1000);
END;

-- Аудит оновлення чату
CREATE TRIGGER IF NOT EXISTS trg_chats_update_audit
AFTER UPDATE ON chats
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, old_values, new_values, changed_at)
    VALUES ('UPDATE', 'chats', NEW.id,
        json_object('title', OLD.title, 'updated_at', OLD.updated_at),
        json_object('title', NEW.title, 'updated_at', NEW.updated_at),
        strftime('%s', 'now') * 1000);
END;

-- Аудит видалення повідомлення (каскад через chats)
CREATE TRIGGER IF NOT EXISTS trg_messages_delete_audit
AFTER DELETE ON messages
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, old_values, changed_at)
    VALUES ('DELETE', 'messages', OLD.id,
        json_object('chat_id', OLD.chat_id, 'role', OLD.role, 'text', OLD.text),
        strftime('%s', 'now') * 1000);
END;

-- Аудит вставки факту довгострокової пам'яті
CREATE TRIGGER IF NOT EXISTS trg_ltm_facts_insert_audit
AFTER INSERT ON ltm_facts
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, new_values, changed_at)
    VALUES ('INSERT', 'ltm_facts', CAST(NEW.id AS TEXT),
        json_object('text', NEW.text, 'chat_id', NEW.chat_id, 'ts', NEW.ts),
        strftime('%s', 'now') * 1000);
END;

-- Тригер перевірки переходу стану: заборона порожнього тексту факту
CREATE TRIGGER IF NOT EXISTS trg_ltm_facts_before_insert_check
BEFORE INSERT ON ltm_facts
FOR EACH ROW
WHEN LENGTH(TRIM(NEW.text)) < 8
BEGIN
    SELECT RAISE(ABORT, 'Текст факту занадто короткий (менше 8 символів)');
END;

-- ---------------------------------------------------------------------------
-- Функції / процедури (SQLite — тільки функції, процедури через скрипти)
-- ---------------------------------------------------------------------------

-- Косинусна схожість двох векторів (BLOB у форматі raw float32 LE)
CREATE FUNCTION IF NOT EXISTS cosine_similarity(vec_a BLOB, vec_b BLOB) RETURNS REAL
LANGUAGE sql AS $$
SELECT 0.0;  -- заглушка, реалізується на боку Qt (C++)
$$;

-- Функція отримання свідки chat-у
CREATE FUNCTION IF NOT EXISTS get_chat_summary(p_chat_id TEXT) RETURNS TEXT
LANGUAGE sql AS $$
SELECT mid_term_summary FROM chat_memory WHERE chat_id = p_chat_id;
$$;

-- Функція підрахунку фактів chat-у
CREATE FUNCTION IF NOT EXISTS count_facts_by_chat(p_chat_id TEXT) RETURNS INTEGER
LANGUAGE sql AS $$
SELECT COUNT(*) FROM ltm_facts WHERE chat_id = p_chat_id;
$$;

-- ---------------------------------------------------------------------------
-- Початкові ролі доступу
-- ---------------------------------------------------------------------------
INSERT OR IGNORE INTO access_roles(role_name, can_read, can_write, can_admin)
VALUES ('reader', 1, 0, 0),
       ('writer', 1, 1, 0),
       ('admin', 1, 1, 1);

-- ---------------------------------------------------------------------------
-- Початковий стан бэкфіллу
-- ---------------------------------------------------------------------------
INSERT OR IGNORE INTO backfill_state(id, backfill_index) VALUES (1, 0);
