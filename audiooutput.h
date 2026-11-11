#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <QObject>
#include <QAudioFormat>
#include <QTimer>
#include <QVector>

#include <functional>

#include "audioresampler.h"

class QAudioSink;
class QIODevice;

class AudioOutput : public QObject
{
    Q_OBJECT
public:
    using RenderCallback = std::function<void(const float *samples, int count)>;

    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput() override;

    void setRenderCallback(RenderCallback callback);
    void setVolume(qreal volume);

    void enqueue(const QVector<float> &samples, int sampleRate);
    void finish();
    void stop();

    bool isActive() const { return m_active; }
    bool isPlaying() const;

signals:
    void started();
    void stopped();
    void finished();

private slots:
    void pump();

private:
    bool startSink(int sampleRate);
    void stopSink();
    void finishNow();
    int availableSamples() const;

    QAudioSink *m_sink = nullptr;
    QIODevice *m_io = nullptr;
    QAudioFormat m_format;
    QTimer m_pumpTimer;

    QVector<float> m_queue;
    int m_readIndex = 0;

    AudioResampler m_referenceResampler;
    AudioResampler m_inputResampler;
    int m_inputSampleRate = 0;
    RenderCallback m_renderCallback;

    qreal m_volume = 1.0;
    bool m_active = false;
    bool m_finishRequested = false;
    bool m_startedEmitted = false;
};

#endif // AUDIOOUTPUT_H
