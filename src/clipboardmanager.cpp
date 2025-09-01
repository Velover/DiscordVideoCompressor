#include "clipboardmanager.h"
#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>

ClipboardManager::ClipboardManager(QObject *parent)
    : QObject(parent)
    , m_clipboard(QApplication::clipboard())
    , m_autoDetectionEnabled(true) // Enable auto-detection initially
{
    connect(m_clipboard, &QClipboard::dataChanged,
            this, &ClipboardManager::onClipboardChanged);
}

bool ClipboardManager::hasVideoUrl() const
{
    const QMimeData *mimeData = m_clipboard->mimeData();
    
    // Check for URLs
    if (mimeData->hasUrls()) {
        for (const QUrl &url : mimeData->urls()) {
            if (url.isLocalFile()) {
                if (isVideoFile(url.toLocalFile())) {
                    return true;
                }
            } else if (isVideoUrl(url.toString())) {
                return true;
            }
        }
    }
    
    // Check for text that might be a video URL or file path
    if (mimeData->hasText()) {
        QString text = mimeData->text().trimmed();
        if (isVideoUrl(text) || isVideoFile(text)) {
            return true;
        }
    }
    
    return false;
}

QUrl ClipboardManager::getVideoUrl() const
{
    const QMimeData *mimeData = m_clipboard->mimeData();
    
    // Check for URLs first
    if (mimeData->hasUrls()) {
        for (const QUrl &url : mimeData->urls()) {
            if (url.isLocalFile()) {
                if (isVideoFile(url.toLocalFile())) {
                    return url;
                }
            } else if (isVideoUrl(url.toString())) {
                return url;
            }
        }
    }
    
    // Check for text
    if (mimeData->hasText()) {
        QString text = mimeData->text().trimmed();
        if (isVideoFile(text)) {
            return QUrl::fromLocalFile(text);
        } else if (isVideoUrl(text)) {
            return QUrl(text);
        }
    }
    
    return QUrl();
}

QString ClipboardManager::getClipboardText() const
{
    return m_clipboard->text();
}

void ClipboardManager::disableAutoDetection()
{
    m_autoDetectionEnabled = false;
    qDebug() << "Auto-detection disabled - will only detect on manual Ctrl+V";
}

void ClipboardManager::onClipboardChanged()
{
    emit clipboardChanged();
    
    // Only auto-detect if enabled (only during launch phase)
    if (m_autoDetectionEnabled && hasVideoUrl()) {
        QUrl url = getVideoUrl();
        if (url.isValid()) {
            emit videoUrlFound(url);
        }
    }
}

bool ClipboardManager::isVideoUrl(const QString &text) const
{
    // Check for common video streaming URLs
    QStringList videoHosts = {
        "youtube.com", "youtu.be", "vimeo.com", "dailymotion.com",
        "twitch.tv", "netflix.com", "amazon.com", "hulu.com"
    };
    
    for (const QString &host : videoHosts) {
        if (text.contains(host, Qt::CaseInsensitive)) {
            return true;
        }
    }
    
    // Check for direct video URLs
    QStringList videoExtensions = {
        ".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", 
        ".webm", ".m4v", ".3gp", ".ogv", ".m3u8"
    };
    
    for (const QString &ext : videoExtensions) {
        if (text.endsWith(ext, Qt::CaseInsensitive)) {
            return true;
        }
    }
    
    return false;
}

bool ClipboardManager::isVideoFile(const QString &path) const
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        return false;
    }
    
    QStringList videoExtensions = {
        "mp4", "avi", "mkv", "mov", "wmv", "flv", 
        "webm", "m4v", "3gp", "ogv", "mpg", "mpeg",
        "ts", "m2ts", "asf", "rm", "rmvb"
    };
    
    QString suffix = fileInfo.suffix().toLower();
    return videoExtensions.contains(suffix);
}
