#ifndef AUDIORECORDING_H
#define AUDIORECORDING_H

#include <QObject>
#include <QUrl>

class AudioRecording : public QObject
{
    Q_OBJECT
public:
    explicit AudioRecording(QObject *parent = nullptr);
    ~AudioRecording() override;

    void startRecording();
    void stopRecording();
    QUrl getRecordedAudioUrl() const;

signals:
    void recordingStopped();

private:
    class QMediaCaptureSession *m_session = nullptr;
    class QAudioInput *m_audioInput = nullptr;
    class QMediaRecorder *m_recorder = nullptr;
};

#endif // AUDIORECORDING_H