#include <QApplication>
#include <QQmlApplicationEngine>
#include <QIcon>
#include <QDir>
#include <QQuickStyle>
#include <QQmlContext>
#include "videoplayer.h"
#include "clipboardmanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("Video Player");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("VideoPlayer");
    
    // Use Material style for better UI
    QQuickStyle::setStyle("Material");
    
    // Register QML types
    qmlRegisterType<VideoPlayer>("VideoPlayer", 1, 0, "VideoPlayer");
    qmlRegisterType<ClipboardManager>("VideoPlayer", 1, 0, "ClipboardManager");
    
    // Create QML engine
    QQmlApplicationEngine engine;
    
    // Create instances to be used in QML
    VideoPlayer videoPlayer;
    ClipboardManager clipboardManager;
    
    // Make instances available to QML
    engine.rootContext()->setContextProperty("videoPlayer", &videoPlayer);
    engine.rootContext()->setContextProperty("clipboardManager", &clipboardManager);
    
    // Auto-load video from clipboard on startup if available
    if (clipboardManager.hasVideoUrl()) {
        QUrl url = clipboardManager.getVideoUrl();
        if (url.isValid()) {
            videoPlayer.loadVideo(url);
        }
    }
    
    // Load main QML file
    const QUrl url(QStringLiteral("qrc:/qml/VideoPlayerWindow.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    
    engine.load(url);
    
    return app.exec();
}
