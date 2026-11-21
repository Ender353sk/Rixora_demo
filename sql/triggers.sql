-- ============================================================================
-- Тригери бази даних підсистеми довготривалої пам'яті Ріксори (SQLite)
-- Курсова робота з дисципліни «Бази даних»
-- ============================================================================

-- ---------------------------------------------------------------------------
-- Тригер 1: trg_chats_insert_audit
-- Подія: AFTER INSERT на chats
-- Призначення: Аудит створення нового чату. Записує в audit_log подію INSERT
-- з новими значеннями (title, created_at).
-- Обґрунтування: Дозволяє відслідковувати створення чатів для адміністративного
-- контролю та відновлення стану.
-- ---------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_chats_insert_audit
AFTER INSERT ON chats
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, new_values, changed_at)
    VALUES ('INSERT', 'chats', NEW.id,
            json_object('title', NEW.title, 'created_at', NEW.created_at),
            strftime('%s', 'now') * 1000);
END;

-- ---------------------------------------------------------------------------
-- Тригер 2: trg_chats_update_audit
-- Подія: AFTER UPDATE на chats
-- Призначення: Аудит оновлення чату (зміна заголовка, лічильника повідомлень,
-- часу оновлення). Записує старі та нові значення.
-- Обґрунтування: Контроль змін метаданих чатів, можливість відкоту або аналізу.
-- ---------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_chats_update_audit
AFTER UPDATE ON chats
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, old_values, new_values, changed_at)
    VALUES ('UPDATE', 'chats', NEW.id,
            json_object('title', OLD.title, 'updated_at', OLD.updated_at,
                        'message_count', OLD.message_count),
            json_object('title', NEW.title, 'updated_at', NEW.updated_at,
                        'message_count', NEW.message_count),
            strftime('%s', 'now') * 1000);
END;

-- ---------------------------------------------------------------------------
-- Тригер 3: trg_messages_delete_audit
-- Подія: AFTER DELETE на messages
-- Призначення: Аудит видалення повідомлення. Записує старі значення (chat_id,
-- role, text) для відслідкування втрачених даних.
-- Обґрунтування: Повідомлення можуть видалятися користувачем або каскадно при
-- видаленні чату — аудит дозволяє відновити стан.
-- ---------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_messages_delete_audit
AFTER DELETE ON messages
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, old_values, changed_at)
    VALUES ('DELETE', 'messages', OLD.id,
            json_object('chat_id', OLD.chat_id, 'role', OLD.role, 'text', OLD.text),
            strftime('%s', 'now') * 1000);
END;

-- ---------------------------------------------------------------------------
-- Тригер 4: trg_ltm_facts_insert_audit
-- Подія: AFTER INSERT на ltm_facts
-- Призначення: Аудит додавання нового факту довгострокової пам'яті. Записує
-- текст факту, chat_id та час створення.
-- Обґрунтування: Факти є ключовими даними системи — їх створення повинно
-- фіксуватися для відладки та перевірки цілісності.
-- ---------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_ltm_facts_insert_audit
AFTER INSERT ON ltm_facts
BEGIN
    INSERT INTO audit_log(action, table_name, record_id, new_values, changed_at)
    VALUES ('INSERT', 'ltm_facts', CAST(NEW.id AS TEXT),
            json_object('text', NEW.text, 'chat_id', NEW.chat_id, 'ts', NEW.ts),
            strftime('%s', 'now') * 1000);
END;

-- ---------------------------------------------------------------------------
-- Тригер 5: trg_ltm_facts_before_insert_check
-- Подія: BEFORE INSERT на ltm_facts (для кожного рядка)
-- Умова: LENGTH(TRIM(NEW.text)) < 8
-- Призначення: Заборона вставки факту з текстом коротше 8 символів.
-- Обґрунтування: Факти занадто короткі не несуть сенсової інформації і тільки
-- забрудняють довгострокову пам'ять. Це бізнес-правило предметної області.
-- Дія: RAISE(ABORT, ...) — скасовує операцію вставки.
-- ---------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_ltm_facts_before_insert_check
BEFORE INSERT ON ltm_facts
FOR EACH ROW
WHEN LENGTH(TRIM(NEW.text)) < 8
BEGIN
    SELECT RAISE(ABORT, 'Текст факту занадто короткий (менше 8 символів)');
END;

-- ---------------------------------------------------------------------------
-- Тригер 6: trg_messages_insert_update_count
-- Подія: AFTER INSERT на messages
-- Призначення: Автоматично оновлює message_count у таблиці chats після
-- додавання нового повідомлення.
-- Обґрунтування: Підтримує актуальність лічильника повідомлень без додаткових
-- викликів з боку додатку. Це приклад денормалізації (лічильник дублюється
-- з chats.messages), але обҕортюється швидким доступом до статистики.
-- ---------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_messages_insert_update_count
AFTER INSERT ON messages
BEGIN
    UPDATE chats
    SET message_count = message_count + 1,
        updated_at = strftime('%s', 'now') * 1000
    WHERE id = NEW.chat_id;
END;

-- ---------------------------------------------------------------------------
-- Тригер 7: trg_messages_delete_update_count
-- Подія: AFTER DELETE на messages
-- Призначення: Зменшує message_count у chats після видалення повідомлення.
-- Обґрунтування: Підтримує консистентність лічильника при видаленні.
-- ---------------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_messages_delete_update_count
AFTER DELETE ON messages
BEGIN
    UPDATE chats
    SET message_count = MAX(0, message_count - 1),
        updated_at = strftime('%s', 'now') * 1000
    WHERE id = OLD.chat_id;
END;

-- ============================================================================
-- Підсумок тригерів: 7 об'єктів (понад вимоги 2+)
-- 1. trg_chats_insert_audit — аудит створення чату
-- 2. trg_chats_update_audit — аудит оновлення чату
-- 3. trg_messages_delete_audit — аудит видалення повідомлення
-- 4. trg_ltm_facts_insert_audit — аудит створення факту
-- 5. trg_ltm_facts_before_insert_check — перевірка довжини тексту факту
-- 6. trg_messages_insert_update_count — оновлення лічильника при вставці
-- 7. trg_messages_delete_update_count — оновлення лічильника при видаленні
--
-- Різні обґрунтовані дії: аудит (1-4), перевірка переходу стану / валідація (5),
-- підтримка денормалізованої агрегації (6-7).
-- ============================================================================
