#include "ttsengine.h"
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <sherpa-onnx/c-api/c-api.h>

TtsEngine::TtsEngine(QObject *parent)
    : QObject(parent)
{
}

TtsEngine::~TtsEngine()
{
    if (m_ttsEngine) {
        SherpaOnnxDestroyOfflineTts(const_cast<SherpaOnnxOfflineTts*>(m_ttsEngine));
        m_ttsEngine = nullptr;
    }
}

bool TtsEngine::init(const TTSModelConfig &config)
{
    if (m_ttsEngine) {
        SherpaOnnxDestroyOfflineTts(const_cast<SherpaOnnxOfflineTts*>(m_ttsEngine));
        m_ttsEngine = nullptr;
    }

    SherpaOnnxOfflineTtsConfig sherpaConfig;
    memset(&sherpaConfig, 0, sizeof(sherpaConfig));

    sherpaConfig.model.num_threads = 2;
    sherpaConfig.model.debug = 0;
    sherpaConfig.model.provider = "cpu";

    QByteArray dp = config.durationPredictor.toUtf8();
    QByteArray te = config.textEncoder.toUtf8();
    QByteArray ve = config.vectorEstimator.toUtf8();
    QByteArray vo = config.vocoder.toUtf8();
    QByteArray tj = config.ttsJson.toUtf8();
    QByteArray ui = config.unicodeIndexer.toUtf8();
    QByteArray vs = config.voiceStyle.toUtf8();

    sherpaConfig.model.supertonic.duration_predictor = dp.constData();
    sherpaConfig.model.supertonic.text_encoder       = te.constData();
    sherpaConfig.model.supertonic.vector_estimator   = ve.constData();
    sherpaConfig.model.supertonic.vocoder            = vo.constData();
    sherpaConfig.model.supertonic.tts_json           = tj.constData();
    sherpaConfig.model.supertonic.unicode_indexer    = ui.constData();
    sherpaConfig.model.supertonic.voice_style        = vs.constData();

    m_ttsEngine = SherpaOnnxCreateOfflineTts(&sherpaConfig);
    if (!m_ttsEngine) {
        qCritical() << "[TtsEngine] Ошибка инициализации Sherpa-ONNX Supertonic 3!";
        emit errorOccurred("Ошибка инициализации Sherpa-ONNX Supertonic 3");
        return false;
    }

    qDebug() << "[TtsEngine] Мультиязычный движок речи Supertonic 3 загружен.";
    return true;
}

QString TtsEngine::generateLocalVoice(const SherpaOnnxOfflineTts *engine,
                                      const QString &text,
                                      int fileIndex)
{
    if (!engine) return QString();

    const SherpaOnnxGeneratedAudio *audio = SherpaOnnxOfflineTtsGenerate(engine, text.toUtf8().constData(), 0, 1.0);
    if (!audio) return QString();

    QString filePath = QDir::tempPath() + QString("/rixora_tts_temp_%1.wav").arg(fileIndex);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);

out.writeRawData("RIFF", 4);
        out << static_cast<quint32>(36 + audio->n * 2);
        out.writeRawData("WAVE", 4);
        out.writeRawData("fmt ", 4);
        out << static_cast<quint32>(16);
        out << static_cast<quint16>(1);
        out << static_cast<quint16>(1);
        out << static_cast<quint32>(audio->sample_rate);
        out << static_cast<quint32>(audio->sample_rate * 2);
        out << static_cast<quint16>(2);
        out << static_cast<quint16>(16);
        out.writeRawData("data", 4);
        out << static_cast<quint32>(audio->n * 2);

        QVector<int16_t> pcmData(audio->n);
        for (int i = 0; i < audio->n; ++i) {
            float sample = audio->samples[i];
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            pcmData[i] = static_cast<int16_t>(sample * 32767.0f);
        }
        out.writeRawData(reinterpret_cast<const char*>(pcmData.constData()), static_cast<int>(audio->n * 2));
        file.close();
    }
    SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);
    return filePath;
}
