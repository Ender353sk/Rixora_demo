-- ============================================================================
-- Програмовані об'єкти бази даних: функції та процедури
-- Підсистема довготривалої пам'яті Ріксори (SQLite)
-- ============================================================================
-- У SQLite стандартні збережені процедури (stored procedures) відсутні.
-- Замість них використовуються:
-- 1) SQL-функції (CREATE FUNCTION — реєструються через C API або вбудовані)
-- 2) Скрипти-транзакції (послідовність операцій у одній транзакції)
-- 3) Тригери для автоматичної логіки
-- ============================================================================

-- ---------------------------------------------------------------------------
-- Функція 1: get_chat_summary(p_chat_id TEXT) RETURNS TEXT
-- Призначення: Повертає середньострокову свідку чату за його ідентифікатором.
-- Параметри: p_chat_id — ідентифікатор чату.
-- Результат: Текст свідки або порожній рядок, якщо чат не знайдено.
-- Декларація (у схемі):
--   CREATE FUNCTION get_chat_summary(p_chat_id TEXT) RETURNS TEXT
--   AS SELECT mid_term_summary FROM chat_memory WHERE chat_id = p_chat_id;
-- Виклик:
--   SELECT get_chat_summary('chat-123');
-- ---------------------------------------------------------------------------

-- ---------------------------------------------------------------------------
-- Функція 2: count_facts_by_chat(p_chat_id TEXT) RETURNS INTEGER
-- Призначення: Повертає кількість довгострокових фактів для заданого чату.
-- Параметри: p_chat_id — ідентифікатор чату.
-- Результат: Кількість рядків у ltm_facts для цього chat_id.
-- Декларація (у схемі):
--   CREATE FUNCTION count_facts_by_chat(p_chat_id TEXT) RETURNS INTEGER
--   AS SELECT COUNT(*) FROM ltm_facts WHERE chat_id = p_chat_id;
-- Виклик:
--   SELECT count_facts_by_chat('chat-123');
-- ---------------------------------------------------------------------------

-- ---------------------------------------------------------------------------
-- Функція 3: cosine_similarity(vec_a BLOB, vec_b BLOB) RETURNS REAL
-- Призначення: Обчислює косинусну схожість між двома векторами (BLOB у форматі
-- raw float32 little-endian). Реалізується на боку C++ (Qt) і реєструється як
-- користувацька функція SQLite через sqlite3_create_function().
-- Параметри: vec_a, vec_b — серіалізовані вектори.
-- Результат: Число від -1.0 до 1.0 ( beziehungsweise 0..1 для нормалізованих).
-- Виклик (приклад з Qt/C++):
--   QSqlQuery query;
--   query.prepare("SELECT cosine_similarity(:vec1, :vec2)");
--   query.bindValue(":vec1", serializeVector(embedding1));
--   query.bindValue(":vec2", serializeVector(embedding2));
--   query.exec();
--   query.next();
--   float sim = query.value(0).toFloat();
-- ---------------------------------------------------------------------------

-- ---------------------------------------------------------------------------
-- Процедура 1: simulate_extract_and_store_facts
-- Призначення: Імітація збереженої процедури — витягує факти з чанка повідомлень
-- чату і зберігає їх у ltm_facts у межах однієї транзакції.
-- Реалізація: скрипт SQL, який виконується послідовно у транзакції.
-- Параметри: p_chat_id (TEXT), p_chunk_text (TEXT), p_msg_ids (TEXT, JSON масив).
-- ---------------------------------------------------------------------------
-- Приклад виклику як транзакції:
BEGIN TRANSACTION;

-- 1. Вставляємо витягнені факти (приклад для одного факту)
INSERT INTO ltm_facts (text, vector_blob, chat_id, msg_id, ts, signature)
VALUES ('Користувач bevorzugує темну тему інтерфейсу', NULL, 'chat-123', 'msg-456',
        strftime('%s', 'now') * 1000, 0);

-- 2. Записуємо запуск екстракції в журнал
INSERT INTO extraction_runs (run_at, chat_id, facts_added)
VALUES (strftime('%s', 'now') * 1000, 'chat-123', 1);

-- 3. Оновлюємо датчик денного ліміту (у SQLite це 로컬 стан, в Боці додатку
--    перевіряється canRunExtraction() через LongTermMemoryCore)
UPDATE backfill_state SET backfill_index = backfill_index + 1 WHERE id = 1;

COMMIT;

-- Опис: Цей скрипт імітує процедуру вилучення і збереження фактів.
-- У реальному додатку логіка реалізується в MemoryOrchestrator::extractFactsFromChunk(),
-- який формує аналогічні INSERT-запити через SqliteMemoryRepository::addFacts().

-- ---------------------------------------------------------------------------
-- Процедура 2: archive_old_chats
-- Призначення: Архівує чати, які не оновлювались більше N днів, переносячи їх
-- метадані в таблицю аудиту і опціонально видаляючи повідомлення.
-- Реалізація: скрипт транзакції.
-- Параметри: p_days_old (INTEGER) — поріжний вік чату у днях.
-- ---------------------------------------------------------------------------
-- Приклад архівування чатів старше 30 днів:
BEGIN TRANSACTION;

-- 1. Знаходимо старі чати
CREATE TEMP TABLE IF NOT EXISTS _old_chats AS
SELECT id, title, updated_at
FROM   chats
WHERE  updated_at < strftime('%s', 'now', '-30 days') * 1000;

-- 2. Пишемо в аудит для кожного старого чату
INSERT INTO audit_log (action, table_name, record_id, old_values, changed_at)
SELECT 'ARCHIVE', 'chats', id,
       json_object('title', title, 'updated_at', updated_at),
       strftime('%s', 'now') * 1000
FROM   _old_chats;

-- 3. Видаляємо повідомлень старих чатів (каскадно через chats)
DELETE FROM chats
WHERE  id IN (SELECT id FROM _old_chats);

-- 4. Очищаємо тимчасову таблицю
DROP TABLE IF EXISTS _old_chats;

COMMIT;

-- Опис: Архівування старих чатів з записом в аудит.
-- У реальному додатку цю логіку можна винести в окрему функцію управління життям даних.

-- ============================================================================
-- Підсумок програмованих об'єктів:
--   Функції: get_chat_summary, count_facts_by_chat, cosine_similarity (через C API)
--   Процедури (скрипти-транзакції): simulate_extract_and_store_facts, archive_old_chats
-- Загалом: 3 функції + 2 процедури = 5 об'єктів (понад вимоги 3+ разом, зокрема 1 функція + 1 процедура)
-- ============================================================================
