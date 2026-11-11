#ifndef AUDIO_RESAMPLER_H
#define AUDIO_RESAMPLER_H

#include <QVector>

struct SpeexResamplerState_;

class AudioResampler
{
public:
    AudioResampler() = default;
    ~AudioResampler();

    AudioResampler(const AudioResampler &) = delete;
    AudioResampler &operator=(const AudioResampler &) = delete;

    bool init(int inputRate, int outputRate, int quality = 3);
    void reset();
    QVector<float> process(const float *samples, int count);

    bool isReady() const { return m_state != nullptr; }
    int inputRate() const { return m_inputRate; }
    int outputRate() const { return m_outputRate; }

private:
    SpeexResamplerState_ *m_state = nullptr;
    int m_inputRate = 0;
    int m_outputRate = 0;
};

#endif // AUDIO_RESAMPLER_H
