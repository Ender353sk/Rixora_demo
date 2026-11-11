#include "audioplayback.h"
#include "ttsengine.h"
#include "textutils.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>
#include <QFile>
#include <QtConcurrent>
#include <QFutureWatcher>

AudioPlayback::AudioPlayback(TtsEngine &ttsEngine, QObject *parent)
    : QObject(parent), m_ttsEngine(ttsEngine)
{
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_audioOutput->setVolume(1.0);

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this]() {
        if (m_player->playbackState() == QMediaPlayer::StoppedState) {
            if (!m_lastPlayedFile.isEmpty()) {
                QFile::remove(m_lastPlayedFile);
                m_lastPlayedFile.clear();
            }
            if (!m_stopRequested) {
                playNextAudio();
            }
        }
    });

    connect(&m_ttsWatcher, &QFutureWatcher<QString>::finished, this, [this]() {
        m_isSynthesizing = false;

        if (m_stopRequested) return;

        QString audioPath = m_ttsWatcher.result();
        if (!audioPath.isEmpty()) {
            m_audioQueue.append(audioPath);
            if (m_player->playbackState() != QMediaPlayer::PlayingState) {
                playNextAudio();
            }
        }
        processNextSentence();
    });
}

AudioPlayback::~AudioPlayback()
{
    // Деструктор НЕ эмитит ttsFinished. Сигнал уходит в NetworkManager, где
    // его обрабатывают GeminiApi/SpeechApi, — а на момент разрушения они могут
    // быть уже уничтожены (use-after-free). Поэтому здесь только освобождение
    // ресурсов, без строки emit ttsFinished().
    releaseResources();
}

void AudioPlayback::enqueueTextForTTS(const QString &text)
{
    if (!m_lastPlayedFile.isEmpty()) {
        QFile::remove(m_lastPlayedFile);
        m_lastPlayedFile.clear();
    }

    for (const QString &f : m_audioQueue) QFile::remove(f);
    m_audioQueue.clear();
    m_sentenceQueue.clear();
    m_isSynthesizing = false;
    m_stopRequested = false;

    m_sentenceQueue = TextUtils::splitIntoTtsChunks(text);
    processNextSentence();
}

void AudioPlayback::processNextSentence()
{
    if (m_stopRequested || m_isSynthesizing || m_sentenceQueue.isEmpty()) {
        return;
    }

    m_isSynthesizing = true;
    QString nextText = m_sentenceQueue.takeFirst();
    int currentIndex = m_audioFileCounter++;

    QFuture<QString> future = QtConcurrent::run(TtsEngine::generateLocalVoice, m_ttsEngine.engine(), nextText, currentIndex);
    m_ttsWatcher.setFuture(future);
}

void AudioPlayback::playNextAudio()
{
    if (m_stopRequested) return;

    if (m_audioQueue.isEmpty()) {
        if (m_sentenceQueue.isEmpty() && !m_isSynthesizing) {
            emit ttsFinished();
        }
        return;
    }

    QString nextFile = m_audioQueue.takeFirst();
    m_lastPlayedFile = nextFile;
    m_player->setSource(QUrl::fromLocalFile(nextFile));
    m_player->play();
}

void AudioPlayback::stop()
{
    releaseResources();
    emit ttsFinished();
}

void AudioPlayback::releaseResources()
{
    m_stopRequested = true;
    m_sentenceQueue.clear();
    for (const QString &f : m_audioQueue) QFile::remove(f);
    m_audioQueue.clear();
    // Плеер нужно остановить ДО удаления файла: на Windows файл, открытый
    // проигрывателем, удалить нельзя.
    if (m_player && m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->stop();
    }
    if (!m_lastPlayedFile.isEmpty()) {
        QFile::remove(m_lastPlayedFile);
        m_lastPlayedFile.clear();
    }
}

bool AudioPlayback::isPlaying() const
{
    return m_player && m_player->playbackState() == QMediaPlayer::PlayingState;
}

void AudioPlayback::setStopRequested(bool stop)
{
    m_stopRequested = stop;
}
