#include "audiorecording.h"
#include <QMediaCaptureSession>
#include <QAudioInput>
#include <QMediaRecorder>
#include <QMediaFormat>

AudioRecording::AudioRecording(QObject *parent)
    : QObject(parent)
{
    m_session = new QMediaCaptureSession(this);
    m_audioInput = new QAudioInput(this);
    m_session->setAudioInput(m_audioInput);
    m_recorder = new QMediaRecorder(this);
    m_session->setRecorder(m_recorder);

    connect(m_recorder, &QMediaRecorder::recorderStateChanged, this, [this](QMediaRecorder::RecorderState state) {
        if (state == QMediaRecorder::StoppedState) {
            emit recordingStopped();
        }
    });
}

AudioRecording::~AudioRecording() = default;

void AudioRecording::startRecording()
{
    QMediaFormat format;
    format.setFileFormat(QMediaFormat::Wave);
    format.setAudioCodec(QMediaFormat::AudioCodec::Wave);
    m_recorder->setMediaFormat(format);
    m_recorder->setAudioSampleRate(16000);
    m_recorder->setAudioChannelCount(1);
    m_recorder->record();
}

void AudioRecording::stopRecording()
{
    m_recorder->stop();
}

QUrl AudioRecording::getRecordedAudioUrl() const
{
    return m_recorder->actualLocation();
}
