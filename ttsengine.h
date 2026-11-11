#ifndef TTSENGINE_H
#define TTSENGINE_H

#include <QObject>
#include <QString>

struct SherpaOnnxOfflineTts;
struct SherpaOnnxGeneratedAudio;

// Настройки модели TTS
struct TTSModelConfig {
    QString durationPredictor;
    QString textEncoder;
    QString vectorEstimator;
    QString vocoder;
    QString ttsJson;
    QString unicodeIndexer;
    QString voiceStyle;
};

// Движок TTS на базе Sherpa-ONNX Supertonic 3
class TtsEngine : public QObject
{
    Q_OBJECT
public:
    explicit TtsEngine(QObject *parent = nullptr);
    ~TtsEngine();

    bool init(const TTSModelConfig &config);
    const SherpaOnnxOfflineTts* engine() const { return m_ttsEngine; }

    // Генерация аудио из текста (статический метод для использования в QtConcurrent)
    static QString generateLocalVoice(const SherpaOnnxOfflineTts *engine,
                                      const QString &text,
                                      int fileIndex);

signals:
      void errorOccurred(const QString &message);

  private:
      const SherpaOnnxOfflineTts *m_ttsEngine = nullptr;
  };


#endif // TTSENGINE_H