#ifndef SPEECHAPI_H
#define SPEECHAPI_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class AudioEngine;

class SpeechApi : public QObject
{
    Q_OBJECT

public:
    SpeechApi(AudioEngine &audioEngine, QObject *parent = nullptr);

    void setGroqKey(const QString &key);
    void setLanguage(const QString &lang);
    void startRecording();
    void stopRecording();
    void abortCurrentReply();
    bool isBusy() const { return m_currentReply && m_currentReply->isRunning(); }

signals:
    void userVoiceRecognized(const QString &text);
    void responseReceived(const QString &response);
    void processingFinished();

private slots:
    void sendAudioToServer();

private:
    AudioEngine &m_audioEngine;
    QString m_groqKey;
    QString m_language = "ru";
    QNetworkAccessManager m_networkAccessManager;
    QNetworkReply *m_currentReply = nullptr;
};

#endif // SPEECHAPI_H
