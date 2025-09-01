#include <QApplication>
#include <QQmlApplicationEngine>
#include <QIcon>
#include <QDir>
#include <QQuickStyle>
#include <QQmlContext>
#include "videocompressor.h"
#include "clipboardmanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("Video Compressor");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("VideoCompressor");
    
    // Use Material style for better UI
    QQuickStyle::setStyle("Material");
    
    // Register QML types
    qmlRegisterType<VideoCompressor>("VideoCompressor", 1, 0, "VideoCompressor");
    qmlRegisterType<ClipboardManager>("VideoCompressor", 1, 0, "ClipboardManager");
    
    // Create QML engine
    QQmlApplicationEngine engine;
    
    // Create instances to be used in QML
    VideoCompressor videoCompressor;
    ClipboardManager clipboardManager;
    
    // Make instances available to QML
    engine.rootContext()->setContextProperty("videoCompressor", &videoCompressor);
    engine.rootContext()->setContextProperty("clipboardManager", &clipboardManager);
    
    // Auto-detect videos from clipboard on startup ONLY
    if (clipboardManager.hasVideoUrl()) {
        QList<QUrl> urls = clipboardManager.getAllVideoUrls();
        for (const QUrl &url : urls) {
            if (url.isValid()) {
                videoCompressor.addVideo(url);
                qDebug() << "Auto-added video from clipboard on startup:" << url.toString();
            }
        }
    }
    
    // Disable auto-detection after launch - only respond to manual Ctrl+V
    clipboardManager.disableAutoDetection();
    
    // Load main QML file
    const QUrl url(QStringLiteral("qrc:/qml/VideoCompressorWindow.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    
    engine.load(url);
    
    return app.exec();
}
