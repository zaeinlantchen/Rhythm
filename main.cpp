#include "Rhythm.h"

#include <QApplication>
#include <QFile>

static void loadStyleSheet(QApplication &app, const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法加载样式表:" << path;
        return;
    }
    app.setStyleSheet(QString::fromUtf8(f.readAll()));
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    loadStyleSheet(a, ":/style/Rhythm.qss");
    Rhythm w;
    w.show();
    return QApplication::exec();
}
