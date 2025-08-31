#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QObject>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoSink>
#include <QUrl>
#include <QQmlEngine>

class VideoPlayer : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QMediaPlayer::PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QVideoSink* videoSink READ videoSink CONSTANT)
    Q_PROPERTY(QString currentSource READ currentSource NOTIFY currentSourceChanged)

public:
    explicit VideoPlayer(QObject *parent = nullptr);
    ~VideoPlayer();
    
    // Property getters
    QMediaPlayer::PlaybackState playbackState() const;
    qint64 duration() const;
    qint64 position() const;
    qreal volume() const;
    bool isMuted() const;
    QVideoSink* videoSink() const;
    QString currentSource() const;
    
    // Property setters
    void setPosition(qint64 position);
    void setVolume(qreal volume);
    void setMuted(bool muted);
    
    Q_INVOKABLE void setVideoOutput(QObject* videoOutput);

public slots:
    void play();
    void pause();
    void stop();
    void loadVideo(const QUrl &url);
    void loadVideoFromPath(const QString &path);

signals:
    void playbackStateChanged();
    void durationChanged();
    void positionChanged();
    void volumeChanged();
    void mutedChanged();
    void currentSourceChanged();
    void error(const QString &errorString);

private slots:
    void handleMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void handleError(QMediaPlayer::Error error, const QString &errorString);

private:
    QMediaPlayer *m_mediaPlayer;
    QAudioOutput *m_audioOutput;
    QString m_currentSource;
    bool m_initialLoad;
    bool m_seeking;
};

#endif // VIDEOPLAYER_H
