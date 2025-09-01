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
#include <QBuffer>
#include <QRegularExpression>
#include <QPainter>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

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
    , m_installProcess(nullptr) // Initialize install process
{
    m_tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/VideoCompressor";
    
    // Clean up temp files from previous sessions on startup
    cleanupTempFiles();
    
    // Create fresh temp directory
    QDir().mkpath(m_tempDir);
    
    connect(m_progressTimer, &QTimer::timeout, this, &VideoCompressor::onFFmpegProgress);
    
    checkFFmpeg();
}

VideoCompressor::~VideoCompressor()
{
    // Don't cleanup temp files on exit - they may be needed for clipboard access
    // cleanupTempFiles(); // Removed this line
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
        // Convert QPixmap to base64 data URL for QML Image component
        if (!item.thumbnail.isNull()) {
            QByteArray byteArray;
            QBuffer buffer(&byteArray);
            buffer.open(QIODevice::WriteOnly);
            item.thumbnail.save(&buffer, "PNG");
            QString base64 = QString("data:image/png;base64,%1").arg(QString(byteArray.toBase64()));
            return base64;
        }
        return QString();
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
    if (m_isCompressing) {
        emit debugMessage("Compression already in progress", "warning");
        return;
    }
    
    if (!m_ffmpegAvailable) {
        emit debugMessage("Cannot start compression: FFmpeg/FFprobe not available", "error");
        emit debugMessage("Please install FFmpeg and restart the application", "error");
        return;
    }
    
    if (m_videos.isEmpty()) {
        emit debugMessage("No videos to compress", "warning");
        return;
    }
    
    // Clean up any old temp files before starting new compression
    emit debugMessage("Cleaning up old temporary files before compression...", "info");
    cleanupTempFiles();
    QDir().mkpath(m_tempDir); // Recreate temp directory
    
    // Re-check FFmpeg availability before starting
    emit debugMessage("Re-checking FFmpeg availability before compression...", "info");
    
    QProcess testProcess;
    testProcess.start("ffmpeg", QStringList() << "-version");
    bool canStart = testProcess.waitForStarted(3000);
    if (canStart) {
        testProcess.waitForFinished(3000);
    }
    
    if (!canStart || testProcess.exitCode() != 0) {
        emit debugMessage("FFmpeg check failed before compression", "error");
        emit debugMessage("FFmpeg may have been uninstalled or PATH changed", "error");
        m_ffmpegAvailable = false;
        emit ffmpegAvailableChanged();
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
    
    // Check both ffmpeg and ffprobe
    QProcess ffmpegProcess;
    ffmpegProcess.start("ffmpeg", QStringList() << "-version");
    bool ffmpegStarted = ffmpegProcess.waitForStarted(3000);
    if (ffmpegStarted) {
        ffmpegProcess.waitForFinished(5000);
    }
    
    QProcess ffprobeProcess;
    ffprobeProcess.start("ffprobe", QStringList() << "-version");
    bool ffprobeStarted = ffprobeProcess.waitForStarted(3000);
    if (ffprobeStarted) {
        ffprobeProcess.waitForFinished(5000);
    }
    
    bool ffmpegAvailable = ffmpegStarted && (ffmpegProcess.exitCode() == 0);
    bool ffprobeAvailable = ffprobeStarted && (ffprobeProcess.exitCode() == 0);
    
    emit debugMessage("FFmpeg process started: " + QString(ffmpegStarted ? "Yes" : "No"), "info");
    emit debugMessage("FFprobe process started: " + QString(ffprobeStarted ? "Yes" : "No"), "info");
    
    if (ffmpegStarted) {
        emit debugMessage("FFmpeg exit code: " + QString::number(ffmpegProcess.exitCode()), "info");
        if (ffmpegProcess.exitCode() != 0) {
            QString ffmpegError = QString::fromUtf8(ffmpegProcess.readAllStandardError());
            emit debugMessage("FFmpeg error output: " + ffmpegError, "warning");
        }
    }
    
    if (ffprobeStarted) {
        emit debugMessage("FFprobe exit code: " + QString::number(ffprobeProcess.exitCode()), "info");
        if (ffprobeProcess.exitCode() != 0) {
            QString ffprobeError = QString::fromUtf8(ffprobeProcess.readAllStandardError());
            emit debugMessage("FFprobe error output: " + ffprobeError, "warning");
        }
    }
    
    bool previouslyAvailable = m_ffmpegAvailable;
    m_ffmpegAvailable = ffmpegAvailable && ffprobeAvailable;
    emit ffmpegAvailableChanged();
    
    if (m_ffmpegAvailable) {
        QString ffmpegVersion = QString::fromUtf8(ffmpegProcess.readAllStandardOutput()).split('\n').first();
        QString ffprobeVersion = QString::fromUtf8(ffprobeProcess.readAllStandardOutput()).split('\n').first();
        
        emit debugMessage("FFmpeg found: " + ffmpegVersion, "success");
        emit debugMessage("FFprobe found: " + ffprobeVersion, "success");
        
        // If FFmpeg was just installed (previously not available)
        if (!previouslyAvailable) {
            emit debugMessage("FFmpeg installation detected successfully!", "success");
        }
        
        // Check hardware acceleration after confirming FFmpeg works
        checkHardwareAcceleration();
    } else {
        if (!ffmpegAvailable) {
            if (!ffmpegStarted) {
                emit debugMessage("FFmpeg not found in PATH - process failed to start", "error");
                emit debugMessage("If you just installed FFmpeg, try restarting the application", "info");
            } else {
                emit debugMessage("FFmpeg found but returned error code " + QString::number(ffmpegProcess.exitCode()), "error");
            }
        }
        if (!ffprobeAvailable) {
            if (!ffprobeStarted) {
                emit debugMessage("FFprobe not found in PATH - process failed to start", "error");
                emit debugMessage("If you just installed FFmpeg, try restarting the application", "info");
            } else {
                emit debugMessage("FFprobe found but returned error code " + QString::number(ffprobeProcess.exitCode()), "error");
            }
        }
        emit debugMessage("Both FFmpeg and FFprobe are required for video processing", "error");
        emit debugMessage("Click 'Install' to automatically install FFmpeg", "info");
        
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

void VideoCompressor::installFFmpegWithElevation()
{
    if (m_installProcess) {
        emit debugMessage("Installation already in progress", "warning");
        return;
    }
    
    emit debugMessage("Starting FFmpeg installation with administrator privileges...", "info");
    emit debugMessage("A User Account Control dialog will appear - please click 'Yes' to continue", "info");
    
    // Find the install-ffmpeg.ps1 script
    QString scriptPath = QCoreApplication::applicationDirPath() + "/install-ffmpeg.ps1";
    
    // Check if script exists
    if (!QFileInfo::exists(scriptPath)) {
        emit debugMessage("ERROR: install-ffmpeg.ps1 not found at: " + scriptPath, "error");
        emit debugMessage("Please ensure the installation script is in the same folder as the executable", "error");
        return;
    }
    
    emit debugMessage("Found installation script: " + scriptPath, "info");
    
#ifdef Q_OS_WIN
    // Use Windows API to run PowerShell with elevation
    emit debugMessage("Requesting administrator elevation using Windows API...", "info");
    
    QString arguments = QString("-ExecutionPolicy Bypass -File \"%1\"").arg(scriptPath);
    
    HINSTANCE result = ShellExecuteA(
        NULL,
        "runas",                    // Triggers UAC elevation
        "powershell.exe",           // Program to execute
        arguments.toLocal8Bit().data(), // Command line arguments
        NULL,                       // Working directory (use current)
        SW_SHOWNORMAL              // Show window normally
    );
    
    // ShellExecute returns a value > 32 on success
    if (reinterpret_cast<intptr_t>(result) > 32) {
        emit debugMessage("PowerShell script launched with elevation successfully", "success");
        emit debugMessage("Please wait for the installation to complete in the elevated window", "info");
        
        // Since we can't directly monitor the elevated process, 
        // we'll check FFmpeg availability after a delay
        QTimer::singleShot(10000, this, [this]() {
            emit debugMessage("Checking if FFmpeg installation completed...", "info");
            checkFFmpeg();
            if (m_ffmpegAvailable) {
                emit debugMessage("FFmpeg installation detected successfully!", "success");
            } else {
                emit debugMessage("FFmpeg not yet detected. Installation may still be in progress or was cancelled.", "warning");
                emit debugMessage("You can manually check the installation or restart the application after installation completes.", "info");
            }
        });
        
    } else {
        // Handle ShellExecute errors
        DWORD error = GetLastError();
        QString errorMsg;
        
        switch (error) {
        case ERROR_CANCELLED:
            errorMsg = "Installation cancelled by user (UAC dialog declined)";
            break;
        case ERROR_FILE_NOT_FOUND:
            errorMsg = "PowerShell not found on the system";
            break;
        case ERROR_ACCESS_DENIED:
            errorMsg = "Access denied - unable to elevate privileges";
            break;
        default:
            errorMsg = QString("Windows API error: %1").arg(error);
            break;
        }
        
        emit debugMessage("Failed to launch elevated installation: " + errorMsg, "error");
        emit debugMessage("You can try running the install-ffmpeg.ps1 script manually as administrator", "info");
    }
#else
    // Fallback for non-Windows systems
    emit debugMessage("Elevated installation only supported on Windows", "error");
    emit debugMessage("Please install FFmpeg manually on this platform", "info");
#endif
}

void VideoCompressor::onInstallProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_installProcess) {
        return;
    }
    
    QString output = QString::fromUtf8(m_installProcess->readAllStandardOutput());
    QString error = QString::fromUtf8(m_installProcess->readAllStandardError());
    
    if (!output.isEmpty()) {
        emit debugMessage("Installation output: " + output, "info");
    }
    
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        emit debugMessage("FFmpeg installation completed successfully!", "success");
        emit debugMessage("Checking FFmpeg availability...", "info");
        
        // Wait a moment then check FFmpeg availability
        QTimer::singleShot(2000, this, [this]() {
            checkFFmpeg();
            if (m_ffmpegAvailable) {
                emit debugMessage("FFmpeg installation verified successfully!", "success");
            } else {
                emit debugMessage("FFmpeg installed but not yet detected - you may need to restart the application", "warning");
            }
        });
    } else {
        emit debugMessage("FFmpeg installation failed", "error");
        emit debugMessage("Exit code: " + QString::number(exitCode) + ", Status: " + 
                         (exitStatus == QProcess::NormalExit ? "Normal" : "Crashed"), "error");
        
        if (!error.isEmpty()) {
            emit debugMessage("Installation error: " + error, "error");
        }
        
        if (exitCode == 1223) {
            emit debugMessage("Installation was cancelled by user (UAC dialog)", "warning");
        }
    }
    
    // Cleanup
    m_installProcess->deleteLater();
    m_installProcess = nullptr;
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

void VideoCompressor::cleanupTempFiles()
{
    QDir tempDir(m_tempDir);
    if (tempDir.exists()) {
        int fileCount = tempDir.entryList(QDir::Files | QDir::NoDotAndDotDot).count();
        if (fileCount > 0) {
            emit debugMessage("Cleaning up " + QString::number(fileCount) + " temporary files...", "info");
        }
        tempDir.removeRecursively();
    }
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

void VideoCompressor::generateThumbnail(VideoItem &item)
{
    // Always check FFmpeg availability first
    if (!m_ffmpegAvailable) {
        emit debugMessage("Cannot generate thumbnail: FFmpeg not available", "warning");
        createPlaceholderThumbnail(item);
        // Find the item index and update the model
        for (int i = 0; i < m_videos.size(); ++i) {
            if (m_videos[i].path == item.path) {
                QModelIndex idx = index(i);
                emit dataChanged(idx, idx, {ThumbnailRole});
                break;
            }
        }
        return;
    }
    
    // Test if FFmpeg actually works before using it
    QProcess testProcess;
    testProcess.start("ffmpeg", QStringList() << "-version");
    bool canStart = testProcess.waitForStarted(2000);
    if (canStart) {
        testProcess.waitForFinished(2000);
    }
    
    if (!canStart || testProcess.exitCode() != 0) {
        emit debugMessage("FFmpeg test failed during thumbnail generation", "warning");
        createPlaceholderThumbnail(item);
        // Find the item index and update the model
        for (int i = 0; i < m_videos.size(); ++i) {
            if (m_videos[i].path == item.path) {
                QModelIndex idx = index(i);
                emit dataChanged(idx, idx, {ThumbnailRole});
                break;
            }
        }
        return;
    }
    
    QString thumbnailPath = m_tempDir + "/" + QFileInfo(item.path).baseName() + "_thumb.jpg";
    
    // Use FFmpeg to extract a frame at 10% of video duration
    QProcess thumbnailProcess;
    QStringList args;
    
    // Calculate seek time (10% of duration, or 5 seconds if duration unknown)
    double seekTime = item.durationSeconds > 0 ? item.durationSeconds * 0.1 : 5.0;
    
    args << "-i" << item.path
         << "-ss" << QString::number(seekTime, 'f', 2)
         << "-vframes" << "1"
         << "-q:v" << "2"  // High quality
         << "-vf" << "scale=120:68:force_original_aspect_ratio=decrease,pad=120:68:(ow-iw)/2:(oh-ih)/2:black"
         << "-y" << thumbnailPath;
    
    emit debugMessage("Generating thumbnail for: " + item.fileName, "info");
    
    thumbnailProcess.start("ffmpeg", args);
    if (thumbnailProcess.waitForFinished(10000)) {
        if (thumbnailProcess.exitCode() == 0 && QFileInfo::exists(thumbnailPath)) {
            QPixmap thumbnail;
            if (thumbnail.load(thumbnailPath)) {
                item.thumbnail = thumbnail;
                emit debugMessage("Thumbnail generated successfully for: " + item.fileName, "success");
            } else {
                emit debugMessage("Failed to load generated thumbnail for: " + item.fileName, "warning");
                createPlaceholderThumbnail(item);
            }
            // Clean up temporary thumbnail file
            QFile::remove(thumbnailPath);
        } else {
            QString errorOutput = QString::fromUtf8(thumbnailProcess.readAllStandardError());
            emit debugMessage("Thumbnail generation failed for " + item.fileName + ": " + errorOutput, "warning");
            createPlaceholderThumbnail(item);
        }
    } else {
        emit debugMessage("Thumbnail generation timed out for: " + item.fileName, "warning");
        createPlaceholderThumbnail(item);
    }
    
    // Always update the model after thumbnail generation (success or failure)
    for (int i = 0; i < m_videos.size(); ++i) {
        if (m_videos[i].path == item.path) {
            m_videos[i].thumbnail = item.thumbnail; // Update the actual item in the list
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {ThumbnailRole});
            break;
        }
    }
}

void VideoCompressor::createPlaceholderThumbnail(VideoItem &item)
{
    QPixmap placeholder(120, 68);
    placeholder.fill(QColor(64, 64, 64)); // Dark gray background
    
    // Draw a simple video icon
    QPainter painter(&placeholder);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw play button triangle
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(QBrush(Qt::white));
    
    QPolygon triangle;
    triangle << QPoint(45, 25) << QPoint(45, 43) << QPoint(60, 34);
    painter.drawPolygon(triangle);
    
    // Draw file extension text
    QString extension = QFileInfo(item.path).suffix().toUpper();
    if (!extension.isEmpty()) {
        painter.setPen(Qt::lightGray);
        painter.setFont(QFont("Arial", 8, QFont::Bold));
        painter.drawText(QRect(65, 25, 50, 18), Qt::AlignCenter, extension);
    }
    
    item.thumbnail = placeholder;
}

double VideoCompressor::getVideoDuration(const QString &filePath)
{
    if (!m_ffmpegAvailable) {
        emit debugMessage("Cannot get video duration: FFmpeg/FFprobe not available", "error");
        return 0.0;
    }
    
    QProcess process;
    QStringList args;
    args << "-v" << "quiet" 
         << "-show_entries" << "format=duration" 
         << "-of" << "csv=p=0" 
         << filePath;
    
    emit debugMessage("Getting duration for: " + QFileInfo(filePath).fileName(), "info");
    emit debugMessage("FFprobe command: ffprobe " + args.join(" "), "info");
    
    process.start("ffprobe", args);
    bool started = process.waitForStarted(5000);
    
    if (!started) {
        emit debugMessage("Failed to start FFprobe process for duration detection", "error");
        emit debugMessage("FFprobe might not be installed or not in PATH", "error");
        return 0.0;
    }
    
    if (!process.waitForFinished(10000)) {
        emit debugMessage("FFprobe timed out for: " + filePath, "error");
        process.kill();
        return 0.0;
    }
    
    if (process.exitCode() != 0) {
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());
        emit debugMessage("FFprobe failed for " + filePath + " (exit code: " + QString::number(process.exitCode()) + ")", "error");
        emit debugMessage("FFprobe error: " + errorOutput, "error");
        return 0.0;
    }
    
    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    bool ok;
    double duration = output.toDouble(&ok);
    
    if (ok && duration > 0) {
        emit debugMessage("Duration detected: " + QString::number(duration, 'f', 1) + " seconds", "success");
        return duration;
    } else {
        emit debugMessage("Invalid duration value from FFprobe: '" + output + "'", "error");
        return 0.0;
    }
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