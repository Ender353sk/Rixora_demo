#ifndef AUDIOPLAYBACK_H
#define AUDIOPLAYBACK_H

#include <QObject>
#include <QUrl>
#include <QList>
#include <QString>
#include <QFutureWatcher>

class QMediaPlayer;
class QAudioOutput;
class TtsEngine;

class AudioPlayback : public QObject
{
    Q_OBJECT
public:
    explicit AudioPlayback(TtsEngine &ttsEngine, QObject *parent = nullptr);
    ~AudioPlayback() override;

    void enqueueTextForTTS(const QString &text);
    void playNextAudio();
    void stop();
    bool isPlaying() const;
    void setStopRequested(bool stop);

signals:
    void ttsFinished();

private:
    void processNextSentence();
    // Освобождение ресурсов без эмиссии сигналов — используется деструктором.
    void releaseResources();

    TtsEngine &m_ttsEngine;
    QFutureWatcher<QString> m_ttsWatcher;

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;

    QList<QString> m_audioQueue;
    QList<QString> m_sentenceQueue;
    QString m_lastPlayedFile;

    bool m_isSynthesizing = false;
    int m_audioFileCounter = 0;
    bool m_stopRequested = false;
};

#endif // AUDIOPLAYBACK_H