#include <QApplication>
#include <QFile>
#include <QIcon>
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("FarhanDB");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("FarhanDB");
    app.setWindowIcon(QIcon(":/icons/farhandb.ico"));

    QFile f(":/styles/light.qss");
    if (f.open(QFile::ReadOnly)) {
        app.setStyleSheet(f.readAll());
        f.close();
    }

    MainWindow window;
    window.show();

    return app.exec();
}
