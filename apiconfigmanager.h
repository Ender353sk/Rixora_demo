#ifndef APICONFIGMANAGER_H
#define APICONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QSettings>

class ApiConfigManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ getLanguage WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool perChatMemory READ perChatMemory WRITE setPerChatMemory NOTIFY perChatMemoryChanged)
    Q_PROPERTY(bool speechLanguageAuto READ speechLanguageAuto WRITE setSpeechLanguageAuto NOTIFY speechLanguageAutoChanged)

public:
    explicit ApiConfigManager(QObject *parent = nullptr);

    Q_INVOKABLE void saveApiKeys(const QString &geminiKey, const QString &groqKey);
    Q_INVOKABLE bool loadSavedKeys();
    QString getGeminiKey() const;
    QString getGroqKey() const;

    Q_INVOKABLE void setLanguage(const QString &langCode);
    Q_INVOKABLE QString getLanguage() const;
    Q_INVOKABLE QString getGreeting() const;

    bool perChatMemory() const;
    void setPerChatMemory(bool on);

    bool speechLanguageAuto() const;
    void setSpeechLanguageAuto(bool on);

signals:
    void languageChanged();
    void perChatMemoryChanged();
    void speechLanguageAutoChanged();

private:
    QString m_geminiKey;
    QString m_groqKey;
    QString m_currentLanguage;
    bool m_perChatMemory = false;
    bool m_speechLanguageAuto = false;
};

#endif // APICONFIGMANAGER_H
