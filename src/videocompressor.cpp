#include "videocompressor.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QClipboard>
#include <QApplication>
#include <QMimeData>
#include <QUrl>
#include <QFileDialog>
#include <QProcess>
#include <QRegularExpression>

VideoCompressor::VideoCompressor(QObject *parent)
    : QAbstractListModel(parent)
    , m_targetSizeMB(10)
    , m_isCompressing(false)
    , m_ffmpegAvailable(false)
    , m_currentVideoIndex(-1)
    , m_completedCount(0)
    , m_ffmpegProcess(nullptr)
    , m_progressTimer(new QTimer(this))
    , m_hardwareAccelerationEnabled(false)
    , m_hardwareAccelerationAvailable(false)
    , m_hardwareAccelerationType("None")
{
    m_tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/VideoCompressor";
    QDir().mkpath(m_tempDir);
    
    connect(m_progressTimer, &QTimer::timeout, this, &VideoCompressor::onFFmpegProgress);
    
    checkFFmpeg();
}

VideoCompressor::~VideoCompressor()
{
    cleanupTempFiles();
}

int VideoCompressor::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_videos.size();
}

QVariant VideoCompressor::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_videos.size())
        return QVariant();
        
    const VideoItem &item = m_videos[index.row()];
    
    switch (role) {
    case PathRole:
        return item.path;
    case FileNameRole:
        return item.fileName;
    case OriginalSizeRole:
        return item.originalSize;
    case StatusRole:
        return static_cast<int>(item.status);
    case StatusTextRole:
        return item.statusText;
    case ProgressRole:
        return item.progress;
    case ThumbnailRole:
        return item.thumbnail;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> VideoCompressor::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[PathRole] = "path";
    roles[FileNameRole] = "fileName";
    roles[OriginalSizeRole] = "originalSize";
    roles[StatusRole] = "status";
    roles[StatusTextRole] = "statusText";
    roles[ProgressRole] = "progress";
    roles[ThumbnailRole] = "thumbnail";
    return roles;
}

void VideoCompressor::setTargetSizeMB(int size)
{
    if (m_targetSizeMB != size) {
        m_targetSizeMB = size;
        emit targetSizeMBChanged();
    }
}

void VideoCompressor::addVideo(const QUrl &url)
{
    QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    addVideoFromPath(path);
}

void VideoCompressor::addVideoFromPath(const QString &path)
{
    if (!isVideoFile(path) || QFileInfo(path).exists() == false) {
        emit debugMessage("Rejected file (not video or doesn't exist): " + path, "warning");
        return;
    }
    
    // Check if already added
    for (const auto &item : m_videos) {
        if (item.path == path) {
            emit debugMessage("File already in list: " + QFileInfo(path).fileName(), "warning");
            return;
        }
    }
    
    VideoItem item;
    item.path = path;
    item.fileName = QFileInfo(path).fileName();
    item.fileSizeBytes = QFileInfo(path).size();
    item.originalSize = formatFileSize(item.fileSizeBytes);
    item.status = VideoStatus::Ready;
    item.statusText = "Ready";
    item.progress = 0;
    item.durationSeconds = getVideoDuration(path); // Get actual duration
    
    generateThumbnail(item);
    
    beginInsertRows(QModelIndex(), m_videos.size(), m_videos.size());
    m_videos.append(item);
    endInsertRows();
    
    emit totalCountChanged();
    
    QString durationText = item.durationSeconds > 0 ? 
        QString(" - %1 min").arg(QString::number(item.durationSeconds / 60.0, 'f', 1)) : "";
    emit debugMessage("Added video: " + item.fileName + " (" + item.originalSize + durationText + ")", "success");
}

void VideoCompressor::clearVideos()
{
    if (m_isCompressing) {
        return;
    }
    
    beginResetModel();
    m_videos.clear();
    m_completedCount = 0;
    endResetModel();
    
    emit totalCountChanged();
    emit completedCountChanged();
}

void VideoCompressor::removeVideo(int index)
{
    if (index < 0 || index >= m_videos.size() || m_isCompressing) {
        return;
    }
    
    beginRemoveRows(QModelIndex(), index, index);
    m_videos.removeAt(index);
    endRemoveRows();
    
    emit totalCountChanged();
}

void VideoCompressor::startCompression()
{
    if (m_isCompressing || !m_ffmpegAvailable || m_videos.isEmpty()) {
        if (m_isCompressing) {
            emit debugMessage("Compression already in progress", "warning");
        } else if (!m_ffmpegAvailable) {
            emit debugMessage("FFmpeg not available", "error");
        } else {
            emit debugMessage("No videos to compress", "warning");
        }
        return;
    }
    
    emit debugMessage("Starting compression batch with " + QString::number(m_videos.size()) + " videos", "info");
    emit debugMessage("Target size: " + QString::number(m_targetSizeMB) + " MB", "info");
    
    m_isCompressing = true;
    m_currentVideoIndex = -1;
    m_completedCount = 0;
    
    emit isCompressingChanged();
    emit completedCountChanged();
    
    // Reset all videos to ready state
    for (int i = 0; i < m_videos.size(); ++i) {
        updateVideoStatus(i, VideoStatus::Ready, "Queued", 0);
    }
    
    processNextVideo();
}

void VideoCompressor::processNextVideo()
{
    m_currentVideoIndex++;
    
    if (m_currentVideoIndex >= m_videos.size()) {
        // All videos processed
        m_isCompressing = false;
        emit isCompressingChanged();
        emit compressionFinished();
        emit debugMessage("All videos processed successfully", "success");
        return;
    }
    
    VideoItem &item = m_videos[m_currentVideoIndex];
    emit debugMessage("Processing video " + QString::number(m_currentVideoIndex + 1) + "/" + 
                     QString::number(m_videos.size()) + ": " + item.fileName, "info");
    
    // Check if video is already small enough
    qint64 targetBytes = m_targetSizeMB * 1024 * 1024;
    if (item.fileSizeBytes <= targetBytes) {
        updateVideoStatus(m_currentVideoIndex, VideoStatus::AlreadyOptimal, "Already optimal size", 100);
        item.outputPath = item.path; // Use original file
        m_completedCount++;
        emit completedCountChanged();
        emit debugMessage("Video already optimal: " + item.fileName, "success");
        
        QTimer::singleShot(100, this, &VideoCompressor::processNextVideo);
        return;
    }
    
    // Check if we have valid duration
    if (item.durationSeconds <= 0) {
        updateVideoStatus(m_currentVideoIndex, VideoStatus::Error, "Invalid video duration", 0);
        emit debugMessage("ERROR: Could not determine duration for " + item.fileName, "error");
        QTimer::singleShot(100, this, &VideoCompressor::processNextVideo);
        return;
    }
    
    updateVideoStatus(m_currentVideoIndex, VideoStatus::Compressing, "Starting compression (Pass 1/2)...", 0);
    
    // Generate output path with temp prefix
    QString baseName = QFileInfo(item.path).baseName();
    QString tempPath = m_tempDir + "/" + baseName + "_temp.mp4";
    QString outputPath = m_tempDir + "/" + baseName + "_compressed.mp4";
    item.outputPath = outputPath;
    
    // Start with first pass
    m_isFirstPass = true;
    startFFmpegProcess(item, tempPath, true);
}

void VideoCompressor::setHardwareAccelerationEnabled(bool enabled)
{
    if (m_hardwareAccelerationEnabled != enabled) {
        m_hardwareAccelerationEnabled = enabled;
        emit hardwareAccelerationEnabledChanged();
        
        if (enabled && m_hardwareAccelerationAvailable) {
            emit debugMessage("Hardware acceleration enabled: " + m_hardwareAccelerationType, "success");
        } else if (enabled && !m_hardwareAccelerationAvailable) {
            emit debugMessage("Hardware acceleration requested but not available", "warning");
            m_hardwareAccelerationEnabled = false;
            emit hardwareAccelerationEnabledChanged();
        } else {
            emit debugMessage("Hardware acceleration disabled", "info");
        }
    }
}

void VideoCompressor::checkFFmpeg()
{
    emit debugMessage("Checking FFmpeg availability...", "info");
    
    QProcess process;
    process.start("ffmpeg", QStringList() << "-version");
    process.waitForFinished(3000);
    
    m_ffmpegAvailable = (process.exitCode() == 0);
    emit ffmpegAvailableChanged();
    
    if (m_ffmpegAvailable) {
        QString version = QString::fromUtf8(process.readAllStandardOutput()).split('\n').first();
        emit debugMessage("FFmpeg found: " + version, "success");
        
        // Check hardware acceleration after confirming FFmpeg works
        checkHardwareAcceleration();
    } else {
        emit debugMessage("FFmpeg not found in PATH", "error");
        m_hardwareAccelerationAvailable = false;
        m_hardwareAccelerationType = "None";
        emit hardwareAccelerationAvailableChanged();
        emit hardwareAccelerationTypeChanged();
    }
}

void VideoCompressor::checkHardwareAcceleration()
{
    emit debugMessage("Detecting hardware acceleration capabilities...", "info");
    
    // Reset hardware acceleration state
    m_hardwareAccelerationAvailable = false;
    m_hardwareAccelerationType = "None";
    
    // Check for NVIDIA NVENC (CUDA)
    if (testCudaEncoding()) {
        m_hardwareAccelerationAvailable = true;
        m_hardwareAccelerationType = "NVIDIA NVENC (CUDA)";
        emit debugMessage("NVIDIA NVENC hardware acceleration detected", "success");
    }
    // Check for Intel QuickSync if CUDA not available
    else if (testQuickSyncEncoding()) {
        m_hardwareAccelerationAvailable = true;
        m_hardwareAccelerationType = "Intel QuickSync";
        emit debugMessage("Intel QuickSync hardware acceleration detected", "success");
    }
    else {
        emit debugMessage("No hardware acceleration available - will use software encoding", "warning");
    }
    
    // Auto-enable if available
    if (m_hardwareAccelerationAvailable) {
        m_hardwareAccelerationEnabled = true;
        emit hardwareAccelerationEnabledChanged();
    }
    
    emit hardwareAccelerationAvailableChanged();
    emit hardwareAccelerationTypeChanged();
}

bool VideoCompressor::testCudaEncoding()
{
    // First check if NVENC encoders are available
    QProcess checkProcess;
    checkProcess.start("ffmpeg", QStringList() << "-hide_banner" << "-encoders");
    checkProcess.waitForFinished(5000);
    
    QString encoderOutput = QString::fromUtf8(checkProcess.readAllStandardOutput());
    if (!encoderOutput.contains("h264_nvenc")) {
        emit debugMessage("NVENC encoders not found in FFmpeg build", "info");
        return false;
    }
    
    // Test actual CUDA functionality with a quick encode
    QProcess testProcess;
    QString testFile = m_tempDir + "/test_cuda_temp.mp4";
    
    QStringList testArgs;
    testArgs << "-y" << "-f" << "lavfi" 
             << "-i" << "testsrc=duration=1:size=320x240:rate=1"
             << "-c:v" << "h264_nvenc" 
             << "-t" << "1"
             << testFile;
    
    testProcess.start("ffmpeg", testArgs);
    testProcess.waitForFinished(10000);
    
    bool testPassed = (testProcess.exitCode() == 0);
    
    if (!testPassed) {
        QString errorOutput = QString::fromUtf8(testProcess.readAllStandardError());
        emit debugMessage("CUDA test failed: " + errorOutput.split('\n').last().trimmed(), "info");
    }
    
    // Cleanup test file
    QFile::remove(testFile);
    
    return testPassed;
}

bool VideoCompressor::testQuickSyncEncoding()
{
    // Check if QuickSync encoders are available
    QProcess checkProcess;
    checkProcess.start("ffmpeg", QStringList() << "-hide_banner" << "-encoders");
    checkProcess.waitForFinished(5000);
    
    QString encoderOutput = QString::fromUtf8(checkProcess.readAllStandardOutput());
    if (!encoderOutput.contains("h264_qsv")) {
        emit debugMessage("QuickSync encoders not found in FFmpeg build", "info");
        return false;
    }
    
    // Test actual QuickSync functionality
    QProcess testProcess;
    QString testFile = m_tempDir + "/test_qsv_temp.mp4";
    
    QStringList testArgs;
    testArgs << "-y" << "-f" << "lavfi" 
             << "-i" << "testsrc=duration=1:size=320x240:rate=1"
             << "-c:v" << "h264_qsv" 
             << "-t" << "1"
             << testFile;
    
    testProcess.start("ffmpeg", testArgs);
    testProcess.waitForFinished(10000);
    
    bool testPassed = (testProcess.exitCode() == 0);
    
    if (!testPassed) {
        QString errorOutput = QString::fromUtf8(testProcess.readAllStandardError());
        emit debugMessage("QuickSync test failed: " + errorOutput.split('\n').last().trimmed(), "info");
    }
    
    // Cleanup test file
    QFile::remove(testFile);
    
    return testPassed;
}

QString VideoCompressor::getHardwareEncoderName()
{
    if (!m_hardwareAccelerationEnabled || !m_hardwareAccelerationAvailable) {
        return "libx264"; // Software encoder
    }
    
    if (m_hardwareAccelerationType.contains("NVENC")) {
        return "h264_nvenc";
    } else if (m_hardwareAccelerationType.contains("QuickSync")) {
        return "h264_qsv";
    }
    
    return "libx264"; // Fallback to software
}

QString VideoCompressor::getHardwareAcceleratorFlag()
{
    if (!m_hardwareAccelerationEnabled || !m_hardwareAccelerationAvailable) {
        return "";
    }
    
    if (m_hardwareAccelerationType.contains("NVENC")) {
        return "cuda";
    } else if (m_hardwareAccelerationType.contains("QuickSync")) {
        return "qsv";
    }
    
    return "";
}

void VideoCompressor::startFFmpegProcess(const VideoItem &item, const QString &outputPath, bool isFirstPass)
{
    // Cleanup previous process
    if (m_ffmpegProcess) {
        m_ffmpegProcess->deleteLater();
    }
    
    m_ffmpegProcess = new QProcess(this);
    connect(m_ffmpegProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VideoCompressor::onFFmpegFinished);
    
    // Connect to capture FFmpeg output for progress tracking
    connect(m_ffmpegProcess, &QProcess::readyReadStandardError, this, [this, isFirstPass]() {
        if (m_ffmpegProcess) {
            QByteArray data = m_ffmpegProcess->readAllStandardError();
            QString output = QString::fromUtf8(data);
            
            // Parse progress from FFmpeg output
            QRegularExpression timeRegex(R"(time=(\d+):(\d+):(\d+\.\d+))");
            QRegularExpressionMatch match = timeRegex.match(output);
            
            if (match.hasMatch() && m_currentVideoIndex >= 0 && m_currentVideoIndex < m_videos.size()) {
                double hours = match.captured(1).toDouble();
                double minutes = match.captured(2).toDouble();
                double seconds = match.captured(3).toDouble();
                double currentTime = hours * 3600 + minutes * 60 + seconds;
                
                const VideoItem &item = m_videos[m_currentVideoIndex];
                if (item.durationSeconds > 0) {
                    int baseProgress = isFirstPass ? 0 : 50; // First pass: 0-50%, Second pass: 50-100%
                    int passProgress = (currentTime / item.durationSeconds) * 50;
                    int totalProgress = qMin(95, baseProgress + passProgress);
                    
                    QString statusText = isFirstPass ? 
                        QString("Pass 1/2: %1%").arg(passProgress * 2) :
                        QString("Pass 2/2: %1%").arg(passProgress * 2);
                    
                    updateVideoStatus(m_currentVideoIndex, VideoStatus::Compressing, statusText, totalProgress);
                }
            }
            
            // Log significant FFmpeg messages
            QStringList lines = output.split('\n');
            for (const QString &line : lines) {
                QString trimmed = line.trimmed();
                if (!trimmed.isEmpty() && 
                    !trimmed.startsWith("frame=") && 
                    !trimmed.startsWith("fps=") &&
                    !trimmed.contains("time=") &&
                    trimmed.length() > 10) {
                    emit debugMessage("FFmpeg: " + trimmed, "info");
                }
            }
        }
    });
    
    // Build FFmpeg arguments properly with hardware acceleration support
    QStringList args;
    int videoBitrate = calculateOptimalBitrate(item.durationSeconds, m_targetSizeMB);
    QString encoderName = getHardwareEncoderName();
    QString hwAccelFlag = getHardwareAcceleratorFlag();
    
    // Add hardware acceleration flag if available
    if (!hwAccelFlag.isEmpty() && !isFirstPass) {
        args << "-hwaccel" << hwAccelFlag;
    }
    
    if (isFirstPass) {
        // First pass: analysis only, output to NULL/NUL
        QString nullOutput = 
#ifdef Q_OS_WIN
            "NUL";
#else
            "/dev/null";
#endif
        args << "-i" << item.path
             << "-c:v" << encoderName
             << "-b:v" << QString("%1k").arg(videoBitrate)
             << "-c:a" << "aac"
             << "-b:a" << "128k"
             << "-pass" << "1"
             << "-f" << "mp4"
             << "-y" << nullOutput;
    } else {
        // Second pass: actual encoding with optimized settings
        args << "-i" << item.path
             << "-c:v" << encoderName
             << "-b:v" << QString("%1k").arg(videoBitrate)
             << "-c:a" << "aac"
             << "-b:a" << "128k"
             << "-pass" << "2"
             << "-movflags" << "+faststart"
             << "-y" << outputPath;
    }
    
    QString passType = isFirstPass ? "first" : "second";
    QString accelInfo = m_hardwareAccelerationEnabled ? QString(" (HW: %1)").arg(m_hardwareAccelerationType) : " (Software)";
    emit debugMessage("Starting " + passType + " pass for: " + item.fileName + accelInfo, "info");
    
    // Log the command for debugging
    QString debugCmd = "ffmpeg";
    for (const QString &arg : args) {
        if (arg.contains(' ') || arg.contains('\\') || arg.contains('/')) {
            debugCmd += " \"" + arg + "\"";
        } else {
            debugCmd += " " + arg;
        }
    }
    emit debugMessage("FFmpeg command: " + debugCmd, "info");
    
    m_ffmpegProcess->start("ffmpeg", args);
}

void VideoCompressor::onFFmpegFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (m_currentVideoIndex < 0 || m_currentVideoIndex >= m_videos.size()) {
        return;
    }
    
    VideoItem &item = m_videos[m_currentVideoIndex];
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        if (m_isFirstPass) {
            // First pass completed, start second pass
            m_isFirstPass = false;
            updateVideoStatus(m_currentVideoIndex, VideoStatus::Compressing, "Starting pass 2/2...", 50);
            
            QString baseName = QFileInfo(item.path).baseName();
            QString outputPath = m_tempDir + "/" + baseName + "_compressed.mp4";
            
            QTimer::singleShot(500, this, [this, item, outputPath]() {
                startFFmpegProcess(item, outputPath, false);
            });
            return;
        } else {
            // Second pass completed
            QFileInfo outputInfo(item.outputPath);
            if (outputInfo.exists()) {
                double sizeReduction = (1.0 - (double)outputInfo.size() / item.fileSizeBytes) * 100;
                updateVideoStatus(m_currentVideoIndex, VideoStatus::Completed, 
                                QString("Compressed to %1").arg(formatFileSize(outputInfo.size())), 100);
                m_completedCount++;
                emit completedCountChanged();
                
                emit debugMessage("Compression completed: " + item.fileName + 
                                " (" + formatFileSize(item.fileSizeBytes) + " → " + 
                                formatFileSize(outputInfo.size()) + ", " + 
                                QString::number(sizeReduction, 'f', 1) + "% reduction)", "success");
            } else {
                updateVideoStatus(m_currentVideoIndex, VideoStatus::Error, "Output file not created", 0);
                emit debugMessage("Compression failed: Output file not created for " + item.fileName, "error");
            }
        }
    } else {
        QString passType = m_isFirstPass ? "first" : "second";
        updateVideoStatus(m_currentVideoIndex, VideoStatus::Error, 
                         QString("Pass %1 failed").arg(m_isFirstPass ? "1" : "2"), 0);
        emit debugMessage("FFmpeg " + passType + " pass failed for " + item.fileName + 
                         " (Exit code: " + QString::number(exitCode) + ")", "error");
    }
    
    // Clean up pass files after each video
    cleanupPassFiles();
    
    QTimer::singleShot(500, this, &VideoCompressor::processNextVideo);
}

void VideoCompressor::onFFmpegProgress()
{
    // Simple progress simulation - FFmpeg progress parsing would be more complex
    if (m_currentVideoIndex >= 0 && m_currentVideoIndex < m_videos.size()) {
        VideoItem &item = m_videos[m_currentVideoIndex];
        if (item.status == VideoStatus::Compressing && item.progress < 90) {
            item.progress += 10;
            QModelIndex idx = index(m_currentVideoIndex);
            emit dataChanged(idx, idx, {ProgressRole});
        }
    }
}

void VideoCompressor::copyToClipboard()
{
    QStringList completedPaths;
    for (const auto &item : m_videos) {
        if (item.status == VideoStatus::Completed || item.status == VideoStatus::AlreadyOptimal) {
            completedPaths.append(item.outputPath);
        }
    }
    
    if (completedPaths.isEmpty()) {
        emit error("No completed videos to copy");
        return;
    }
    
    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *mimeData = new QMimeData();
    
    QList<QUrl> urls;
    for (const QString &path : completedPaths) {
        urls.append(QUrl::fromLocalFile(path));
    }
    
    mimeData->setUrls(urls);
    clipboard->setMimeData(mimeData);
    
    emit debugMessage("Copied " + QString::number(completedPaths.size()) + " videos to clipboard", "success");
    emit debugMessage("Paths copied: " + completedPaths.join(", "), "info");
}

void VideoCompressor::saveToFolder(const QUrl &folderUrl)
{
    QString folderPath = folderUrl.toLocalFile();
    emit debugMessage("Saving videos to folder: " + folderPath, "info");
    
    if (folderPath.isEmpty()) {
        emit error("Invalid folder path");
        return;
    }
    
    QDir targetDir(folderPath);
    if (!targetDir.exists()) {
        emit error("Target folder does not exist");
        return;
    }
    
    int copiedCount = 0;
    for (const auto &item : m_videos) {
        if (item.status == VideoStatus::Completed || item.status == VideoStatus::AlreadyOptimal) {
            QString targetPath = targetDir.filePath(QFileInfo(item.outputPath).fileName());
            if (QFile::copy(item.outputPath, targetPath)) {
                copiedCount++;
            }
        }
    }
    
    if (copiedCount > 0) {
        emit debugMessage("Successfully saved " + QString::number(copiedCount) + " videos to " + folderPath, "success");
    } else {
        emit error("No videos were copied");
        emit debugMessage("Failed to save videos - no completed videos found", "error");
    }
}

void VideoCompressor::installFFmpeg()
{
    emit ffmpegInstallationRequested();
}

QString VideoCompressor::formatFileSize(qint64 bytes)
{
    const qint64 kb = 1024;
    const qint64 mb = kb * 1024;
    const qint64 gb = mb * 1024;
    
    if (bytes >= gb) {
        return QString::number(bytes / gb, 'f', 1) + " GB";
    } else if (bytes >= mb) {
        return QString::number(bytes / mb, 'f', 1) + " MB";
    } else if (bytes >= kb) {
        return QString::number(bytes / kb, 'f', 1) + " KB";
    } else {
        return QString::number(bytes) + " B";
    }
}

bool VideoCompressor::isVideoFile(const QString &path)
{
    QStringList videoExtensions = {
        "mp4", "avi", "mkv", "mov", "wmv", "flv", 
        "webm", "m4v", "3gp", "ogv", "mpg", "mpeg",
        "ts", "m2ts", "asf", "rm", "rmvb"
    };
    
    QString suffix = QFileInfo(path).suffix().toLower();
    return videoExtensions.contains(suffix);
}

void VideoCompressor::updateVideoStatus(int index, VideoStatus status, const QString &statusText, int progress)
{
    if (index < 0 || index >= m_videos.size()) {
        return;
    }
    
    VideoItem &item = m_videos[index];
    item.status = status;
    item.statusText = statusText;
    item.progress = progress;
    
    QModelIndex idx = this->index(index);
    emit dataChanged(idx, idx, {StatusRole, StatusTextRole, ProgressRole});
}

QString VideoCompressor::getFFmpegCommand(const VideoItem &item, const QString &outputPath, bool isFirstPass)
{
    // This method is now only used for debugging/logging purposes
    // The actual command building is done in startFFmpegProcess
    int videoBitrate = calculateOptimalBitrate(item.durationSeconds, m_targetSizeMB);
    
    if (isFirstPass) {
        QString nullOutput = 
#ifdef Q_OS_WIN
            "NUL";
#else
            "/dev/null";
#endif
        return QString("-i \"%1\" -c:v libx264 -b:v %2k -c:a aac -b:a 128k -pass 1 -f mp4 -y \"%3\"")
               .arg(item.path)
               .arg(videoBitrate)
               .arg(nullOutput);
    } else {
        return QString("-i \"%1\" -c:v libx264 -b:v %2k -c:a aac -b:a 128k -pass 2 -movflags +faststart -y \"%3\"")
               .arg(item.path)
               .arg(videoBitrate)
               .arg(outputPath);
    }
}

double VideoCompressor::getVideoDuration(const QString &filePath)
{
    QProcess process;
    QStringList args;
    args << "-v" << "quiet" 
         << "-show_entries" << "format=duration" 
         << "-of" << "csv=p=0" 
         << filePath;
    
    process.start("ffprobe", args);
    if (!process.waitForFinished(10000)) {
        return 0.0;
    }
    
    if (process.exitCode() != 0) {
        return 0.0;
    }
    
    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    bool ok;
    double duration = output.toDouble(&ok);
    return ok ? duration : 0.0;
}

int VideoCompressor::calculateOptimalBitrate(double durationSeconds, int targetSizeMB)
{
    if (durationSeconds <= 0) {
        return 500; // Fallback bitrate
    }
    
    // Formula: Total Bitrate (kbps) = (Target Size in MB × 8000) / Video Duration (seconds)
    // Using 8000 instead of 8388.608 to ensure we stay under the target
    // Video Bitrate = Total Bitrate - Audio Bitrate (128 kbps)
    
    double totalBitrate = (targetSizeMB * 8000.0) / durationSeconds;
    int videoBitrate = qMax(100, (int)(totalBitrate - 128)); // Minimum 100k, subtract audio bitrate
    
    // Cap maximum bitrate for quality reasons
    videoBitrate = qMin(videoBitrate, 5000);
    
    // Apply safety margin - reduce by 5% to ensure we stay under target
    videoBitrate = (int)(videoBitrate * 0.95);
    
    emit debugMessage(QString("Calculated bitrate for %1 min video: %2 kbps (target: %3 MB, safety margin applied)")
                     .arg(QString::number(durationSeconds / 60.0, 'f', 1))
                     .arg(videoBitrate)
                     .arg(targetSizeMB), "info");
    
    return videoBitrate;
}

void VideoCompressor::cleanupPassFiles()
{
    // Clean up FFmpeg two-pass log files
    QDir tempDir(m_tempDir);
    QStringList passFiles = tempDir.entryList(QStringList() << "ffmpeg2pass-*.log*", QDir::Files);
    
    for (const QString &file : passFiles) {
        QString fullPath = tempDir.filePath(file);
        if (QFile::remove(fullPath)) {
            emit debugMessage("Cleaned up pass file: " + file, "info");
        }
    }
    
    // Also check current directory for pass files
    QDir currentDir(".");
    QStringList currentPassFiles = currentDir.entryList(QStringList() << "ffmpeg2pass-*.log*", QDir::Files);
    
    for (const QString &file : currentPassFiles) {
        if (QFile::remove(file)) {
            emit debugMessage("Cleaned up pass file: " + file, "info");
        }
    }
}

void VideoCompressor::generateThumbnail(VideoItem &item)
{
    // For now, use a placeholder thumbnail
    // In a real implementation, you'd extract a frame from the video
    QPixmap placeholder(120, 68);
    placeholder.fill(Qt::darkGray);
    item.thumbnail = placeholder;
}

void VideoCompressor::cleanupTempFiles()
{
    QDir tempDir(m_tempDir);
    if (tempDir.exists()) {
        tempDir.removeRecursively();
    }
}