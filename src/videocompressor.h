#ifndef VIDEOCOMPRESSOR_H
#define VIDEOCOMPRESSOR_H

#include <QObject>
#include <QUrl>
#include <QQmlEngine>
#include <QAbstractListModel>
#include <QProcess>
#include <QTimer>
#include <QFileInfo>
#include <QPixmap>

enum class VideoStatus {
    Ready,
    Analyzing,
    AlreadyOptimal,
    Compressing,
    Completed,
    Error
};

struct VideoItem {
    QString path;
    QString originalSize;
    QString fileName;
    qint64 fileSizeBytes;
    VideoStatus status;
    QString statusText;
    int progress;
    QString outputPath;
    QPixmap thumbnail;
    double durationSeconds; // Add duration for bitrate calculation
};

class VideoCompressor : public QAbstractListModel
{
    Q_OBJECT
    
    Q_PROPERTY(int targetSizeMB READ targetSizeMB WRITE setTargetSizeMB NOTIFY targetSizeMBChanged)
    Q_PROPERTY(bool isCompressing READ isCompressing NOTIFY isCompressingChanged)
    Q_PROPERTY(bool ffmpegAvailable READ ffmpegAvailable NOTIFY ffmpegAvailableChanged)
    Q_PROPERTY(int completedCount READ completedCount NOTIFY completedCountChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(bool hardwareAccelerationEnabled READ hardwareAccelerationEnabled WRITE setHardwareAccelerationEnabled NOTIFY hardwareAccelerationEnabledChanged)
    Q_PROPERTY(bool hardwareAccelerationAvailable READ hardwareAccelerationAvailable NOTIFY hardwareAccelerationAvailableChanged)
    Q_PROPERTY(QString hardwareAccelerationType READ hardwareAccelerationType NOTIFY hardwareAccelerationTypeChanged)

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        FileNameRole,
        OriginalSizeRole,
        StatusRole,
        StatusTextRole,
        ProgressRole,
        ThumbnailRole
    };

    explicit VideoCompressor(QObject *parent = nullptr);
    ~VideoCompressor();
    
    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    // Properties
    int targetSizeMB() const { return m_targetSizeMB; }
    void setTargetSizeMB(int size);
    bool isCompressing() const { return m_isCompressing; }
    bool ffmpegAvailable() const { return m_ffmpegAvailable; }
    int completedCount() const { return m_completedCount; }
    int totalCount() const { return m_videos.size(); }
    bool hardwareAccelerationEnabled() const { return m_hardwareAccelerationEnabled; }
    void setHardwareAccelerationEnabled(bool enabled);
    bool hardwareAccelerationAvailable() const { return m_hardwareAccelerationAvailable; }
    QString hardwareAccelerationType() const { return m_hardwareAccelerationType; }

public slots:
    void addVideo(const QUrl &url);
    void addVideoFromPath(const QString &path);
    void clearVideos();
    void removeVideo(int index);
    void startCompression();
    void copyToClipboard();
    void saveToFolder(const QUrl &folderUrl);
    void checkFFmpeg();
    void installFFmpeg();
    void installFFmpegWithElevation(); // Add new method for elevated installation

signals:
    void targetSizeMBChanged();
    void isCompressingChanged();
    void ffmpegAvailableChanged();
    void completedCountChanged();
    void totalCountChanged();
    void compressionFinished();
    void ffmpegInstallationRequested();
    void error(const QString &message);
    void debugMessage(const QString &message, const QString &type = "info");
    void hardwareAccelerationEnabledChanged();
    void hardwareAccelerationAvailableChanged();
    void hardwareAccelerationTypeChanged();

private slots:
    void processNextVideo();
    void onFFmpegFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onFFmpegProgress();
    void onInstallProcessFinished(int exitCode, QProcess::ExitStatus exitStatus); // Add new slot

private:
    QList<VideoItem> m_videos;
    int m_targetSizeMB;
    bool m_isCompressing;
    bool m_ffmpegAvailable;
    int m_currentVideoIndex;
    int m_completedCount;
    QProcess *m_ffmpegProcess;
    QTimer *m_progressTimer;
    QString m_tempDir;
    bool m_isFirstPass; // Track if we're in first or second pass
    bool m_hardwareAccelerationEnabled;
    bool m_hardwareAccelerationAvailable;
    QString m_hardwareAccelerationType;
    QProcess *m_installProcess; // Add install process tracker
    
    void generateThumbnail(VideoItem &item);
    void createPlaceholderThumbnail(VideoItem &item); // Add new method
    QString formatFileSize(qint64 bytes);
    bool isVideoFile(const QString &path);
    void updateVideoStatus(int index, VideoStatus status, const QString &statusText, int progress = 0);
    QString getFFmpegCommand(const VideoItem &item, const QString &outputPath, bool isFirstPass = false);
    void cleanupTempFiles();
    double getVideoDuration(const QString &filePath); // Add duration detection
    int calculateOptimalBitrate(double durationSeconds, int targetSizeMB); // Add bitrate calculation
    void cleanupPassFiles(); // Add cleanup for pass files
    void startFFmpegProcess(const VideoItem &item, const QString &outputPath, bool isFirstPass);
    void checkHardwareAcceleration(); // Add hardware acceleration detection
    bool testCudaEncoding(); // Add CUDA test method
    bool testQuickSyncEncoding(); // Add QuickSync test method
    QString getHardwareEncoderName(); // Get appropriate hardware encoder
    QString getHardwareAcceleratorFlag(); // Get hardware accelerator flag
};

#endif // VIDEOCOMPRESSOR_H
