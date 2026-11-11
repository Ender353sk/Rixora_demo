#include "audioresampler.h"

#include <speex/speex_resampler.h>

AudioResampler::~AudioResampler()
{
    if (m_state) {
        speex_resampler_destroy(m_state);
        m_state = nullptr;
    }
}

bool AudioResampler::init(int inputRate, int outputRate, int quality)
{
    if (inputRate <= 0 || outputRate <= 0) return false;
    if (inputRate == outputRate) {
        if (m_state) {
            speex_resampler_destroy(m_state);
            m_state = nullptr;
        }
        m_inputRate = inputRate;
        m_outputRate = outputRate;
        return true;
    }

    if (m_state && m_inputRate == inputRate && m_outputRate == outputRate) {
        speex_resampler_reset_mem(m_state);
        return true;
    }

    if (m_state) {
        speex_resampler_destroy(m_state);
        m_state = nullptr;
    }

    int error = 0;
    m_state = speex_resampler_init(1, inputRate, outputRate,
                                   qBound(0, quality, 10), &error);
    if (!m_state || error != RESAMPLER_ERR_SUCCESS) {
        if (m_state) {
            speex_resampler_destroy(m_state);
            m_state = nullptr;
        }
        return false;
    }

    m_inputRate = inputRate;
    m_outputRate = outputRate;
    return true;
}

void AudioResampler::reset()
{
    if (m_state) {
        speex_resampler_reset_mem(m_state);
    }
}

QVector<float> AudioResampler::process(const float *samples, int count)
{
    if (!samples || count <= 0) return {};

    if (m_inputRate == m_outputRate) {
        return QVector<float>(samples, samples + count);
    }
    if (!m_state) return {};

    const int maxOutput = static_cast<int>(
        (static_cast<qint64>(count) * m_outputRate + m_inputRate - 1) / m_inputRate) + 256;
    QVector<float> output(maxOutput);

    spx_uint32_t inputLength = static_cast<spx_uint32_t>(count);
    spx_uint32_t outputLength = static_cast<spx_uint32_t>(maxOutput);
    const int error = speex_resampler_process_float(
        m_state, 0, samples, &inputLength, output.data(), &outputLength);

    if (error != RESAMPLER_ERR_SUCCESS) {
        return {};
    }

    output.resize(static_cast<int>(outputLength));
    return output;
}
