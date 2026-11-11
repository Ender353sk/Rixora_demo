#include "speechapi.h"
#include "audioengine.h"
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QDebug>

SpeechApi::SpeechApi(AudioEngine &audioEngine, QObject *parent)
    : QObject(parent),
    m_audioEngine(audioEngine)
{
    connect(&m_audioEngine, &AudioEngine::recordingStopped,
            this, &SpeechApi::sendAudioToServer);
}

void SpeechApi::setGroqKey(const QString &key) {
    m_groqKey = key;
}

void SpeechApi::setLanguage(const QString &lang) {
    m_language = lang;
    qDebug() << "[SpeechApi] Язык распознавания речи:" << m_language;
}

void SpeechApi::startRecording() {
    m_audioEngine.startRecording();
}

void SpeechApi::stopRecording() {
    m_audioEngine.stopRecording();
}

void SpeechApi::abortCurrentReply() {
    if (m_currentReply && m_currentReply->isRunning()) {
        m_currentReply->abort();
    }
}

void SpeechApi::sendAudioToServer() {
    QUrl audioFileUrl = m_audioEngine.getRecordedAudioUrl();
    QFile *file = new QFile(audioFileUrl.toLocalFile());
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        emit processingFinished();
        return;
    }

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart modelPart;
    modelPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"model\""));
    modelPart.setBody("whisper-large-v3");
    multiPart->append(modelPart);

    QHttpPart promptPart;
    promptPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"prompt\""));
    promptPart.setBody("Оригинальная речь. Не переводить. Пожалуйста, определяй и сохраняй только оригинальный язык, включая русский, украинский, корейский, японский, немецкий, испанский, французкий, китайский и английский.");
    multiPart->append(promptPart);

    if (!m_language.isEmpty()) {
        QHttpPart languagePart;
        languagePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"language\""));
        languagePart.setBody(m_language.toUtf8());
        multiPart->append(languagePart);
    }

    QHttpPart audioPart;
    audioPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"file\"; filename=\"voice.wav\""));
    audioPart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(audioPart);

    QNetworkRequest request(QUrl("https://api.groq.com/openai/v1/audio/transcriptions"));
    request.setRawHeader("Authorization", ("Bearer " + m_groqKey).toUtf8());

    QNetworkReply *reply = m_networkAccessManager.post(request, multiPart);
    m_currentReply = reply;
    multiPart->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_currentReply == reply) m_currentReply = nullptr;
        reply->deleteLater();

        if (reply->error() == QNetworkReply::OperationCanceledError) return;

        if (reply->error() == QNetworkReply::NoError) {
            QString recognizedText = QJsonDocument::fromJson(reply->readAll()).object()["text"].toString();
            if (!recognizedText.isEmpty()) {
                emit userVoiceRecognized(recognizedText);
            } else {
                emit processingFinished();
            }
        } else {
            emit responseReceived("Ошибка расшифровки: " + reply->errorString());
            emit processingFinished();
        }
    });
}
