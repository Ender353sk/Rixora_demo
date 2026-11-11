#include "audiooutput.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>

#include <algorithm>

namespace {
constexpr int kReferenceSampleRate = 16000;
constexpr int kWriteChunkFrames = 960;

int bytesPerSample(QAudioFormat::SampleFormat format)
{
    switch (format) {
    case QAudioFormat::UInt8: return 1;
    case QAudioFormat::Int16: return 2;
    case QAudioFormat::Int32: return 4;
    case QAudioFormat::Float: return 4;
    default: return 0;
    }
}

void writeSample(char *data, QAudioFormat::SampleFormat format, float sample)
{
    if (sample > 1.0f) sample = 1.0f;
    if (sample < -1.0f) sample = -1.0f;

    switch (format) {
    case QAudioFormat::UInt8:
        *reinterpret_cast<quint8 *>(data) =
            static_cast<quint8>(qBound(0, static_cast<int>(sample * 127.0f) + 128, 255));
        break;
    case QAudioFormat::Int16:
        *reinterpret_cast<qint16 *>(data) = static_cast<qint16>(sample * 32767.0f);
        break;
    case QAudioFormat::Int32:
        *reinterpret_cast<qint32 *>(data) =
            static_cast<qint32>(sample * 2147483647.0f);
        break;
    case QAudioFormat::Float:
        *reinterpret_cast<float *>(data) = sample;
        break;
    default:
        break;
    }
}
}

AudioOutput::AudioOutput(QObject *parent)
    : QObject(parent)
{
    m_pumpTimer.setInterval(10);
    m_pumpTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_pumpTimer, &QTimer::timeout, this, &AudioOutput::pump);
}

AudioOutput::~AudioOutput()
{
    stopSink();
}

void AudioOutput::setRenderCallback(RenderCallback callback)
{
    m_renderCallback = std::move(callback);
}

void AudioOutput::setVolume(qreal volume)
{
    m_volume = qBound<qreal>(0.0, volume, 1.0);
    if (m_sink) {
        m_sink->setVolume(m_volume);
    }
}

bool AudioOutput::startSink(int sampleRate)
{
    if (m_sink) return true;
    if (sampleRate <= 0) return false;

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) return false;

    QAudioFormat candidate;
    candidate.setChannelCount(1);
    candidate.setSampleFormat(QAudioFormat::Int16);

    const int candidateRates[] = {sampleRate, 48000, 44100, 16000};
    bool found = false;
    for (int rate : candidateRates) {
        if (rate <= 0) continue;
        candidate.setSampleRate(rate);
        if (device.isFormatSupported(candidate)) {
            m_format = candidate;
            found = true;
            break;
        }
    }

    if (!found) {
        m_format = device.preferredFormat();
    }

    if (bytesPerSample(m_format.sampleFormat()) == 0) {
        m_format.setSampleFormat(QAudioFormat::Int16);
    }
    if (m_format.channelCount() <= 0) {
        m_format.setChannelCount(1);
    }
    if (m_format.sampleRate() <= 0) {
        m_format.setSampleRate(48000);
    }

    m_sink = new QAudioSink(device, m_format, this);
    const int bytesPerFrame = bytesPerSample(m_format.sampleFormat()) * m_format.channelCount();
    // Небольшой буфер (~30 мс) снижает задержку между записью в sink и
    // реальным звуком — это напрямую уменьшает ошибку выравнивания AEC.
    const int bufferBytes = static_cast<int>(std::max<qsizetype>(
        2048, static_cast<qsizetype>(m_format.sampleRate()) * bytesPerFrame / 30));
    m_sink->setBufferSize(bufferBytes);
    m_sink->setVolume(m_volume);

    connect(m_sink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::IdleState && m_finishRequested && availableSamples() == 0) {
            finishNow();
        }
    });

    m_io = m_sink->start();
    if (!m_io) {
        stopSink();
        return false;
    }

    m_referenceResampler.init(m_format.sampleRate(), kReferenceSampleRate, 2);
    m_inputSampleRate = 0;
    m_inputResampler.reset();
    m_active = true;
    m_startedEmitted = false;
    m_pumpTimer.start();
    return true;
}

void AudioOutput::stopSink()
{
    m_pumpTimer.stop();

    if (m_sink) {
        m_sink->stop();
        m_sink->deleteLater();
        m_sink = nullptr;
    }
    m_io = nullptr;

    m_active = false;
    m_startedEmitted = false;
    m_finishRequested = false;
    m_queue.clear();
    m_readIndex = 0;
    m_referenceResampler.reset();
    m_inputSampleRate = 0;
    m_inputResampler.reset();
}

void AudioOutput::finishNow()
{
    if (!m_sink && !m_active) return;

    m_pumpTimer.stop();
    if (m_sink) {
        m_sink->stop();
        m_sink->deleteLater();
        m_sink = nullptr;
    }
    m_io = nullptr;
    m_active = false;
    m_finishRequested = false;
    m_startedEmitted = false;
    m_queue.clear();
    m_readIndex = 0;
    m_referenceResampler.reset();
    m_inputSampleRate = 0;
    m_inputResampler.reset();

    emit stopped();
    emit finished();
}

int AudioOutput::availableSamples() const
{
    return static_cast<int>(std::max<qsizetype>(0, m_queue.size() - m_readIndex));
}

void AudioOutput::enqueue(const QVector<float> &samples, int sampleRate)
{
    if (samples.isEmpty() || sampleRate <= 0) return;

    if (!m_sink && !startSink(sampleRate)) {
        return;
    }

    QVector<float> data = samples;
    if (sampleRate != m_format.sampleRate()) {
        if (m_inputSampleRate != sampleRate || !m_inputResampler.isReady()) {
            if (!m_inputResampler.init(sampleRate, m_format.sampleRate(), 2)) {
                return;
            }
            m_inputSampleRate = sampleRate;
        }
        data = m_inputResampler.process(samples.constData(), samples.size());
    }
    if (data.isEmpty()) return;

    m_finishRequested = false;
    m_queue.append(data);
    pump();
}

void AudioOutput::finish()
{
    if (!m_active && availableSamples() == 0) {
        emit finished();
        return;
    }

    m_finishRequested = true;
    pump();

    if (availableSamples() == 0
        && (!m_sink || m_sink->state() == QAudio::IdleState)) {
        finishNow();
    }
}

void AudioOutput::stop()
{
    stopSink();
    emit stopped();
}

void AudioOutput::pump()
{
    if (!m_sink || !m_io) return;

    if (!m_startedEmitted) {
        m_startedEmitted = true;
        emit started();
    }

    const int bytesPerSampleValue = bytesPerSample(m_format.sampleFormat());
    const int channels = m_format.channelCount();
    const int bytesPerFrame = bytesPerSampleValue * channels;
    if (bytesPerFrame <= 0) return;

    while (availableSamples() > 0) {
        const qsizetype bytesFree = m_sink->bytesFree();
        if (bytesFree < bytesPerFrame) break;

        const int writableBySpace = static_cast<int>(bytesFree / bytesPerFrame);
        const int frames = std::min({availableSamples(), writableBySpace, kWriteChunkFrames});
        if (frames <= 0) break;

        QByteArray pcm;
        pcm.resize(frames * bytesPerFrame);
        char *out = pcm.data();
        for (int i = 0; i < frames; ++i) {
            const float sample = m_queue[m_readIndex + i];
            char *frameData = out + i * bytesPerFrame;
            for (int channel = 0; channel < channels; ++channel) {
                writeSample(frameData + channel * bytesPerSampleValue,
                            m_format.sampleFormat(), sample);
            }
        }

        const qint64 written = m_io->write(pcm.constData(), pcm.size());
        if (written <= 0) break;

        const int writtenFrames = static_cast<int>(written / bytesPerFrame);
        if (writtenFrames <= 0) break;

        if (m_renderCallback && m_referenceResampler.isReady()) {
            const QVector<float> reference = m_referenceResampler.process(
                m_queue.constData() + m_readIndex, writtenFrames);
            if (!reference.isEmpty()) {
                m_renderCallback(reference.constData(), reference.size());
            }
        }

        m_readIndex += writtenFrames;

        if (writtenFrames < frames) {
            break;
        }
    }

    if (m_readIndex > 8192) {
        m_queue.remove(0, m_readIndex);
        m_readIndex = 0;
    }

    if (m_finishRequested && availableSamples() == 0
        && m_sink->state() == QAudio::IdleState) {
        finishNow();
    }
}

bool AudioOutput::isPlaying() const
{
    if (!m_active) return false;
    if (availableSamples() > 0) return true;
    return m_sink && m_sink->state() == QAudio::ActiveState;
}
