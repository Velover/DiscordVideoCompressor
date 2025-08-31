#ifndef CLIPBOARDMANAGER_H
#define CLIPBOARDMANAGER_H

#include <QObject>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QQmlEngine>

class ClipboardManager : public QObject
{
    Q_OBJECT

public:
    explicit ClipboardManager(QObject *parent = nullptr);

public slots:
    bool hasVideoUrl() const;
    QUrl getVideoUrl() const;
    QString getClipboardText() const;

signals:
    void videoUrlFound(const QUrl &url);
    void clipboardChanged();

private slots:
    void onClipboardChanged();

private:
    QClipboard *m_clipboard;
    bool isVideoUrl(const QString &text) const;
    bool isVideoFile(const QString &path) const;
};

#endif // CLIPBOARDMANAGER_H
