#include "audioengine.h"
#include "ttsengine.h"
#include "audioplayback.h"
#include "audiorecording.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>

namespace {
// Уборка осиротевших WAV озвучки. Нужна и на старте, и при выходе: при сбое
// или жёстком закрытии деструктор не выполняется, и временные файлы копились
// бы в TEMP неограниченно — на много лет использования это десятки мегабайт.
void cleanupTtsTempFiles()
{
    QDir dir(QDir::tempPath());
    const QStringList stale = dir.entryList({QString("rixora_tts_temp_*.wav")}, QDir::Files);
    for (const QString &f : stale) {
        QFile::remove(dir.filePath(f));
    }
}
} // namespace

AudioEngine::AudioEngine(QObject *parent) : QObject(parent)
{
    // ВАЖНО: до создания любого AudioPlayback — иначе удалили бы его файлы.
    cleanupTtsTempFiles();

    m_ttsEngine = std::make_unique<TtsEngine>(this);
    m_audioPlayback = std::make_unique<AudioPlayback>(*m_ttsEngine, this);
    m_audioRecording = std::make_unique<AudioRecording>(this);

    // Настройка модели синтеза Supertonic 3
    TTSModelConfig config;
    QString basePath = QCoreApplication::applicationDirPath() + "/";
    config.durationPredictor = basePath + "models/supertonic3/duration_predictor.int8.onnx";
    config.textEncoder       = basePath + "models/supertonic3/text_encoder.int8.onnx";
    config.vectorEstimator   = basePath + "models/supertonic3/vector_estimator.int8.onnx";
    config.vocoder           = basePath + "models/supertonic3/vocoder.int8.onnx";
    config.ttsJson           = basePath + "models/supertonic3/tts.json";
    config.unicodeIndexer    = basePath + "models/supertonic3/unicode_indexer.bin";
    config.voiceStyle        = basePath + "models/supertonic3/voice.bin";

    // Модель синтеза поднимается ЗДЕСЬ, на старте, и больше не переинициализируется.
    // Ленивая загрузка (по первому обращению к синтезу) была отменена: она
    // блокировала GUI-поток ровно в том месте, где NetworkManager отдаёт
    // ответ в чат, и окно перерисовывалось на несколько секунд позже.
    // Теперь тяжёлая загрузка происходит до показа окна, а enqueueTextForTTS
    // только ставит задачу в QtConcurrent и возвращается мгновенно.
    m_ttsEngine->init(config);

    // Проброс сигналов
    connect(m_audioPlayback.get(), &AudioPlayback::ttsFinished, this, &AudioEngine::ttsFinished);
    connect(m_audioRecording.get(), &AudioRecording::recordingStopped, this, &AudioEngine::recordingStopped);

    // Раньше ошибка инициализации движка речи никуда не уходила: сигнал есть,
    // но подписан на него никто не был. При отсутствующих моделях озвучка
    // молча не работала — кнопка голосового ответа просто ничего не делала.
    connect(m_ttsEngine.get(), &TtsEngine::errorOccurred, this, &AudioEngine::errorOccurred);
}

AudioEngine::~AudioEngine()
{
    // Шаблон намеренно узкий, чтобы не задеть чужие файлы в temp.
    cleanupTtsTempFiles();
}

void AudioEngine::startRecording()
{
    m_audioRecording->startRecording();
}

void AudioEngine::stopRecording()
{
    m_audioRecording->stopRecording();
}

QUrl AudioEngine::getRecordedAudioUrl() const
{
    return m_audioRecording->getRecordedAudioUrl();
}

void AudioEngine::enqueueTextForTTS(const QString &text)
{
    m_audioPlayback->enqueueTextForTTS(text);
}

void AudioEngine::stop()
{
    m_audioPlayback->stop();
}

bool AudioEngine::isPlaying() const
{
    return m_audioPlayback->isPlaying();
}
