#include <QApplication>
#include <QPushButton>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QPushButton btn("Hello, Qt (MinGW)!");
    btn.resize(220, 60);
    btn.show();
    return app.exec();
}
