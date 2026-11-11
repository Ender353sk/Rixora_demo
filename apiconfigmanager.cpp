#include "apiconfigmanager.h"
#include <QDebug>

ApiConfigManager::ApiConfigManager(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_currentLanguage = settings.value("app/language", "ru").toString();
    m_perChatMemory = settings.value("memory/perChat", false).toBool();
    m_speechLanguageAuto = settings.value("speech/languageAuto", false).toBool();
}

void ApiConfigManager::saveApiKeys(const QString &geminiKey, const QString &groqKey) {
    m_geminiKey = geminiKey;
    m_groqKey = groqKey;

    QSettings settings;
    settings.setValue("api/gemini", m_geminiKey);
    settings.setValue("api/groq", m_groqKey);
    qDebug() << "[ApiConfig] Ключи API сохранены.";
}

bool ApiConfigManager::loadSavedKeys() {
    QSettings settings;
    m_geminiKey = settings.value("api/gemini", "").toString();
    m_groqKey = settings.value("api/groq", "").toString();
    return (!m_geminiKey.isEmpty() && !m_groqKey.isEmpty());
}

QString ApiConfigManager::getGeminiKey() const { return m_geminiKey; }
QString ApiConfigManager::getGroqKey() const { return m_groqKey; }

void ApiConfigManager::setLanguage(const QString &langCode) {
    if (m_currentLanguage != langCode) {
        m_currentLanguage = langCode;
        QSettings settings;
        settings.setValue("app/language", m_currentLanguage);
        qDebug() << "[ApiConfig] Язык интерфейса изменен на:" << m_currentLanguage;
        emit languageChanged();
    }
}

QString ApiConfigManager::getLanguage() const {
    return m_currentLanguage;
}

bool ApiConfigManager::perChatMemory() const {
    return m_perChatMemory;
}

void ApiConfigManager::setPerChatMemory(bool on) {
    if (m_perChatMemory == on) return;
    m_perChatMemory = on;
    QSettings settings;
    settings.setValue("memory/perChat", m_perChatMemory);
    qDebug() << "[ApiConfig] Персонализация: память только для этого чата ="
             << (m_perChatMemory ? "включена" : "выключена (глобальная)");
    emit perChatMemoryChanged();
}

bool ApiConfigManager::speechLanguageAuto() const {
    return m_speechLanguageAuto;
}

void ApiConfigManager::setSpeechLanguageAuto(bool on) {
    if (m_speechLanguageAuto == on) return;
    m_speechLanguageAuto = on;
    QSettings settings;
    settings.setValue("speech/languageAuto", m_speechLanguageAuto);
    qDebug() << "[ApiConfig] Автоопределение языка речи ="
             << (m_speechLanguageAuto ? "включено (без подсказки Whisper)" : "выключено (язык интерфейса)");
    emit speechLanguageAutoChanged();
}

QString ApiConfigManager::getGreeting() const {
    if (m_currentLanguage == "en") return "Hello! I am Rixora. What shall we talk about?)";
    if (m_currentLanguage == "es") return "¡Hola! Soy Rixora. ¿De qué vamos a hablar?)";
    if (m_currentLanguage == "ko") return "안녕하세요! 저는 릭소라입니다. 무슨 이야기를 나눌까요?)";
    if (m_currentLanguage == "de") return "Hallo! Ich bin Rixora. Worüber sollen wir sprechen?)";
    if (m_currentLanguage == "uk") return "Привіт! Я Ріксора. Про що поговоримо?)";
    if (m_currentLanguage == "fr") return "Bonjour ! Je suis Rixora. De quoi allons-nous parler ?)";
    if (m_currentLanguage == "ja") return "こんにちは！リクソラです。何について話しましょうか？)";

    return "Привет! Я Риксора. О чём поговорим?";
}
