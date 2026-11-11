#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <QObject>
#include <QUrl>
#include <memory>

#include "ttsengine.h"
#include "audioplayback.h"
#include "audiorecording.h"

class AudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    void startRecording();
    void stopRecording();
    QUrl getRecordedAudioUrl() const;

    void enqueueTextForTTS(const QString &text);
    void stop();

    bool isPlaying() const;
    void setStopRequested(bool stop) { m_audioPlayback->setStopRequested(stop); }

signals:
    void recordingStopped();
    void ttsFinished();
    void errorOccurred(const QString &message);

private:
    std::unique_ptr<TtsEngine> m_ttsEngine;
    std::unique_ptr<AudioPlayback> m_audioPlayback;
    std::unique_ptr<AudioRecording> m_audioRecording;
};

#endif // AUDIOENGINE_H