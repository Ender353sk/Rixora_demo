#ifndef GEMINIADAPTER_H
#define GEMINIADAPTER_H

#include <QJsonArray>

// Адаптер истории диалога под требования Gemini API.
//
// Gemini требует, чтобы содержимое (contents) в запросе generateContent:
//   - начиналось с роли "user";
//   - строго чередовало роли "user" / "model";
//   - заканчивалось сообщением пользователя.
// Нормализация здесь — чисто транспортный слой конкретной LLM, поэтому она вынесена
// из MemoryManager, который не знает о нюансах конкретной платформы.
class GeminiHistoryAdapter
{
public:
    // Приводит историю к формату, принимаемому Gemini. Неприводимая история
    // (например, единственная model-реплика) возвращается как пустой массив.
    static QJsonArray normalizeHistoryForGemini(QJsonArray history);
};

#endif // GEMINIADAPTER_H