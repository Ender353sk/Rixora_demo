#ifndef MEMORYCONTEXTBUILDER_H
#define MEMORYCONTEXTBUILDER_H

#include <QObject>
#include <QString>
#include <QVector>

class LongTermMemory;
class MemoryManager;
class ChatManager;
class ApiConfigManager;

struct MemoryHit;

class MemoryContextBuilder : public QObject
{
    Q_OBJECT
public:
    explicit MemoryContextBuilder(LongTermMemory &longTerm,
                                  MemoryManager &memoryManager,
                                  ChatManager &chatManager,
                                  ApiConfigManager &config,
                                  QObject *parent = nullptr);

    QString buildMemoryContext(const QString &userText, const QVector<float> &queryVector);

private:
    LongTermMemory &m_longTerm;
    MemoryManager &m_memoryManager;
    ChatManager &m_chatManager;
    ApiConfigManager &m_config;
};

#endif // MEMORYCONTEXTBUILDER_H