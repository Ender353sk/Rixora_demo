#ifndef CONSTANTS_H
#define CONSTANTS_H

// ---------- Память (MemoryManager) ----------

// Размер ОКНА краткосрочной памяти: сколько последних сообщений чата уходит
// в Gemini вместе с текущим промптом пользователя.
// Намеренно отделён от SUMMARIZATION_CHUNK_SIZE, чтобы размер двух слоёв
// памяти можно было менять независимо друг от друга.
constexpr int SHORT_TERM_WINDOW = 34;

// Размер чанка сообщений для среднесрочной суммаризации (свёртка с замещением).
constexpr int SUMMARIZATION_CHUNK_SIZE = 34;

// ---------- Модели Gemini ----------

// Модель генерации текста.
constexpr const char *GEMINI_CHAT_MODEL = "gemini-3.5-flash-lite";

// Модель эмбеддингов для текстовых задач.
constexpr const char *GEMINI_EMBED_MODEL = "gemini-embedding-001";

// Базовый адрес REST API Gemini.
constexpr const char *GEMINI_API_BASE = "https://generativelanguage.googleapis.com/v1beta/models/";

// ---------- NetworkManager / Извлечение фактов ----------

// Размер чанка сообщений текущего чата для извлечения долговременных фактов.
constexpr int EXTRACTION_CHUNK_SIZE = 12;

// Минимальное число сообщений после последнего извлечения фактов, перед запуском нового.
constexpr int MESSAGES_BEFORE_EXTRACTION = 6;

// Интервал (в количестве циклов извлечения) между бэкфиллами старых чатов.
constexpr int BACKFILL_INTERVAL = 18;

// Порог общего числа фактов в долгосрочной памяти, ниже которого при старте
// запускается одноразовый бэкфилл существующих чатов.
constexpr int BACKFILL_THRESHOLD = 25;

// ---------- LongTermMemory ----------

// Максимальное допустимое время жизни факта (дни) для формулы recency decay.
// Факты старше этого срока имеют экспоненциально малый вес при поиске.
constexpr double RECENCY_HALF_LIFE_DAYS = 45.0;

// Порог косинусной близости для дедупликации: если новый факт имеет сходство
// > этого порога с уже существующим — он пропускается как дубликат.
constexpr float DEDUP_SIMILARITY_THRESHOLD = 0.90f;

// ---------- Аудио (AudioEngine) ----------

// Максимальная длина одного куска текста (в символах), подаваемого модели TTS.
// Меньше обратного предела модели -> корректная озвучка без обрывов.
constexpr int TTS_MAX_CHUNK_CHARS = 200;

// Минимальная длина куска: совсем короткие обрывки ("1", "г.") склеиваются
// со следующим предложением, чтобы модель не читала их с искажениями.
constexpr int TTS_MIN_CHUNK_CHARS = 25;

// ---------- Memory Trimming ----------
// Буфер сообщений ПЕРЕД свёрнутой зоной.
// Защищает от инвалидации сводки при редких правках на границе.
// 10 сообщений = ~5 пар user/model = комфортный запас для редактирования.
constexpr int TRIM_BUFFER_SIZE = 10;

#endif // CONSTANTS_H
