#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include <QApplication>
#include <QTimer>
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("BinaryScope");
    app.setOrganizationName("BinaryScope");
    app.setApplicationVersion("0.1.0");
    bs::ui::applyTheme(app);
    bs::ui::MainWindow window;
    window.show();
    if (app.arguments().size() > 1)
        window.openPath(app.arguments()[1]);
    if (qEnvironmentVariableIsSet("BINARYSCOPE_SMOKE_TEST"))
        QTimer::singleShot(2500, &app, &QApplication::quit);
    return app.exec();
}
