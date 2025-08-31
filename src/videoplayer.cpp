#include "videoplayer.h"
#include <QDebug>
#include <QFileInfo>
#include <QTimer>

VideoPlayer::VideoPlayer(QObject *parent)
    : QObject(parent)
    , m_mediaPlayer(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
    , m_initialLoad(true)
    , m_seeking(false)
{
    // Setup audio output
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    
    // Connect signals
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
            this, &VideoPlayer::playbackStateChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged,
            this, &VideoPlayer::durationChanged);
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged,
            this, &VideoPlayer::positionChanged);
    connect(m_audioOutput, &QAudioOutput::volumeChanged,
            this, &VideoPlayer::volumeChanged);
    connect(m_audioOutput, &QAudioOutput::mutedChanged,
            this, &VideoPlayer::mutedChanged);
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged,
            this, &VideoPlayer::handleMediaStatusChanged);
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred,
            this, &VideoPlayer::handleError);
    
    // Set playback rate to ensure smooth playback
    m_mediaPlayer->setPlaybackRate(1.0);
}

VideoPlayer::~VideoPlayer()
{
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
    }
}

QMediaPlayer::PlaybackState VideoPlayer::playbackState() const
{
    return m_mediaPlayer->playbackState();
}

qint64 VideoPlayer::duration() const
{
    return m_mediaPlayer->duration();
}

qint64 VideoPlayer::position() const
{
    return m_mediaPlayer->position();
}

qreal VideoPlayer::volume() const
{
    return m_audioOutput->volume();
}

bool VideoPlayer::isMuted() const
{
    return m_audioOutput->isMuted();
}

QVideoSink* VideoPlayer::videoSink() const
{
    return m_mediaPlayer->videoSink();
}

QString VideoPlayer::currentSource() const
{
    return m_currentSource;
}

void VideoPlayer::setPosition(qint64 position)
{
    if (!m_mediaPlayer) return;
    
    // Allow seeking if media is loaded, buffered, OR if we're currently seeking
    bool canSeek = (m_mediaPlayer->mediaStatus() == QMediaPlayer::LoadedMedia || 
                    m_mediaPlayer->mediaStatus() == QMediaPlayer::BufferedMedia ||
                    m_mediaPlayer->mediaStatus() == QMediaPlayer::BufferingMedia) &&
                   m_mediaPlayer->duration() > 0;
    
    if (!canSeek) {
        qDebug() << "Cannot seek: media not ready. Status:" << m_mediaPlayer->mediaStatus() 
                 << "Duration:" << m_mediaPlayer->duration();
        return;
    }
    
    // Prevent overlapping seeks
    if (m_seeking) {
        qDebug() << "Ignoring seek request - already seeking";
        return;
    }
    
    // Ensure position is within valid bounds
    qint64 clampedPosition = qMax(0LL, qMin(position, m_mediaPlayer->duration()));
    qint64 currentPosition = m_mediaPlayer->position();
    
    qDebug() << "Setting position to:" << clampedPosition 
             << "from current:" << currentPosition 
             << "duration:" << m_mediaPlayer->duration()
             << "status:" << m_mediaPlayer->mediaStatus();
    
    m_seeking = true;
    m_mediaPlayer->setPosition(clampedPosition);
    
    // Reset seeking flag after a delay
    QTimer::singleShot(200, this, [this]() {
        m_seeking = false;
        qDebug() << "Seek operation completed";
    });
    
    // Verify the position was set
    QTimer::singleShot(100, this, [this, clampedPosition]() {
        qint64 actualPosition = m_mediaPlayer->position();
        qDebug() << "Position verification - requested:" << clampedPosition 
                 << "actual:" << actualPosition;
    });
}

void VideoPlayer::setVolume(qreal volume)
{
    m_audioOutput->setVolume(volume);
}

void VideoPlayer::setMuted(bool muted)
{
    m_audioOutput->setMuted(muted);
}

void VideoPlayer::setVideoOutput(QObject* videoOutput)
{
    if (videoOutput) {
        // Set the MediaPlayer's video output to the provided VideoOutput
        m_mediaPlayer->setVideoOutput(videoOutput);
    }
}

void VideoPlayer::play()
{
    m_mediaPlayer->play();
}

void VideoPlayer::pause()
{
    m_mediaPlayer->pause();
}

void VideoPlayer::stop()
{
    // Set position to 0 first, then pause to keep the first frame visible
    m_mediaPlayer->setPosition(0);
    m_mediaPlayer->pause();
}

void VideoPlayer::loadVideo(const QUrl &url)
{
    qDebug() << "Loading video:" << url;
    m_initialLoad = true; // Reset initial load flag for new video
    m_mediaPlayer->setSource(url);
    m_currentSource = url.toString();
    emit currentSourceChanged();
}

void VideoPlayer::loadVideoFromPath(const QString &path)
{
    QFileInfo fileInfo(path);
    if (fileInfo.exists()) {
        loadVideo(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
    } else {
        emit error("File does not exist: " + path);
    }
}

void VideoPlayer::handleMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    qDebug() << "Media status changed:" << status;
    switch (status) {
    case QMediaPlayer::InvalidMedia:
        emit error("Invalid media format");
        break;
    case QMediaPlayer::LoadedMedia:
        qDebug() << "Media loaded successfully";
        // Only seek to position 0 and pause on initial load of a new video
        // Don't interfere with seeking operations during playback
        if (m_initialLoad) {
            qDebug() << "Initial load - setting position to 0 and pausing";
            m_mediaPlayer->setPosition(0);
            m_mediaPlayer->pause();
            m_initialLoad = false;
        } else {
            qDebug() << "Seek-triggered load - not resetting position";
        }
        break;
    case QMediaPlayer::BufferedMedia:
        qDebug() << "Media buffered and ready";
        break;
    case QMediaPlayer::EndOfMedia:
        qDebug() << "End of media reached";
        break;
    default:
        break;
    }
}

void VideoPlayer::handleError(QMediaPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error)
    qDebug() << "Media player error:" << errorString;
    emit this->error("Playback error: " + errorString);
}
