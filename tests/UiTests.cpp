#include "core/AnalysisService.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include <QApplication>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent>
using namespace bs;
using namespace bs::ui;
class UiTests : public QObject {
    Q_OBJECT
  private slots:
    void hashesAndReadableErrors() {
        const std::vector<std::uint8_t> abc{'a', 'b', 'c'};
        const auto hash = HashCalculator::calculate(abc);
        QCOMPARE(hash.sha256, QString("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        QCOMPARE(hash.md5, QString("900150983cd24fb0d6963f7d28e17f72"));
        auto future = QtConcurrent::run([](QPromise<std::shared_ptr<AnalysisResult>>& p) {
            AnalysisService::analyze(p, "Z:/does-not-exist/binaryscope.exe");
        });
        future.waitForFinished();
        QVERIFY(future.result()->error.contains("Cannot open file"));
        QTemporaryDir directory;
        QFile empty(directory.filePath("empty.exe"));
        QVERIFY(empty.open(QIODevice::WriteOnly));
        empty.close();
        auto emptyFuture =
            QtConcurrent::run([path = empty.fileName()](QPromise<std::shared_ptr<AnalysisResult>>& p) {
                AnalysisService::analyze(p, path);
            });
        emptyFuture.waitForFinished();
        QVERIFY(emptyFuture.result()->error.contains("empty"));
    }
    void applicationWorkflow() {
        MainWindow window;
        window.resize(1280, 820);
        window.show();
        auto* navigation = window.findChild<QListWidget*>();
        QVERIFY(navigation);
        QVERIFY(!navigation->isEnabled());
        const auto screenshotDir = qEnvironmentVariable("BINARYSCOPE_SCREENSHOT_DIR");
        auto screenshot = [&](const QString& name) {
            if (!screenshotDir.isEmpty()) {
                QDir().mkpath(screenshotDir);
                QTest::qWait(80);
                QVERIFY(window.grab().save(screenshotDir + "/" + name + ".png"));
            }
        };
        screenshot("welcome");
        window.openPath(QCoreApplication::applicationFilePath());
        QTRY_VERIFY_WITH_TIMEOUT(window.analysisReady(), 20000);
        QVERIFY(navigation->isEnabled());
        auto* pages = window.findChild<QStackedWidget*>();
        QVERIFY(pages);
        QCOMPARE(pages->count(), 10);
        auto* overview = dynamic_cast<OverviewWidget*>(pages->widget(1));
        QVERIFY(overview);
        QCOMPARE(overview->model->rowCount(), 14);
        auto* imports = dynamic_cast<ImportWidget*>(pages->widget(4));
        QVERIFY(imports);
        QVERIFY(imports->model->rowCount() > 0);
        auto* sections = dynamic_cast<SectionWidget*>(pages->widget(3));
        QVERIFY(sections);
        QCOMPARE(sections->model->index(0, 0).data().toString(), QString(".text"));
        QCOMPARE(sections->proxy->index(0, 0).data().toString(), QString(".text"));
        sections->table->setCurrentIndex(sections->proxy->index(0, 0));
        QVERIFY(sections->detail->text().contains("EXECUTE"));
        auto* strings = dynamic_cast<StringsWidget*>(pages->widget(6));
        QVERIFY(strings);
        QTRY_VERIFY_WITH_TIMEOUT(strings->model->rowCount() > 0, 10000);
        auto* disassembly = dynamic_cast<DisassemblyWidget*>(pages->widget(8));
        QVERIFY(disassembly);
        QTRY_VERIFY_WITH_TIMEOUT(!disassembly->detail->text().contains("background"), 10000);
        QVERIFY(disassembly->model->rowCount() > 0);
        const QStringList names{"overview", "headers", "sections",    "imports", "exports",
                                "strings",  "hex",     "disassembly", "entropy"};
        for (int i = 0; i < 9; ++i) {
            navigation->setCurrentRow(i);
            QCOMPARE(pages->currentIndex(), i + 1);
            screenshot(names[i]);
        }
        // The inspected file is this test executable: a literal sentinel would be present in it.
        navigation->setCurrentRow(5);
        strings->filter(QUuid::createUuid().toString());
        QTRY_VERIFY_WITH_TIMEOUT(strings->detail->text().startsWith("0 matches"), 10000);
        strings->filter("BinaryScope");
        QTRY_VERIFY_WITH_TIMEOUT(strings->detail->text().contains("matches") &&
                                     !strings->detail->text().startsWith("0 matches"),
                                 10000);
        auto* hex = dynamic_cast<HexWidget*>(pages->widget(7));
        QVERIFY(hex);
        navigation->setCurrentRow(6);
        QVERIFY(hex->jump(0, 2));
        HexView* view = nullptr;
        for (auto* child : hex->findChildren<QAbstractScrollArea*>())
            if (auto* found = dynamic_cast<HexView*>(child))
                view = found;
        QVERIFY(view);
        view->copyBytes();
        QCOMPARE(QApplication::clipboard()->text(), QString("4D 5A"));
        QVERIFY(!hex->jump(UINT64_MAX));
        hex->searchBytes("4D 5A", false);
        QTRY_COMPARE_WITH_TIMEOUT(view->selectionEnd(), std::uint64_t(1), 5000);
        // Ctrl+F opens the actual global search dialog and navigates to a file offset.
        bool dialogUsed = false;
        QTimer::singleShot(100, &window, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            auto* mode = dialog->findChild<QComboBox*>();
            auto* query = dialog->findChild<QLineEdit*>();
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            if (!mode || !query || !buttons) {
                dialog->reject();
                return;
            }
            mode->setCurrentIndex(5);
            query->setText("0x100");
            dialogUsed = true;
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        QTest::keyClick(&window, Qt::Key_F, Qt::ControlModifier);
        QVERIFY(dialogUsed);
        QCOMPARE(view->selectionEnd(), std::uint64_t(0x100));
        window.openPath(QCoreApplication::applicationFilePath());
        QTRY_VERIFY_WITH_TIMEOUT(window.analysisReady(), 20000);
        window.close();
        QVERIFY(!QSettings().value("window/geometry").toByteArray().isEmpty());
    }
    void largeSnapshotRemainsResponsive() {
        QTemporaryDir temporary;
        const auto path = temporary.filePath("large.exe");
        QVERIFY(QFile::copy(QCoreApplication::applicationFilePath(), path));
        QFile file(path);
        QVERIFY(file.open(QIODevice::Append));
        const auto overlayStart = file.size();
        const QByteArray block(1024 * 1024, 'A');
        for (int i = 0; i < 64; ++i)
            QCOMPARE(file.write(block), qint64(block.size()));
        QCOMPARE(file.write("BoundaryNeedle\0", 15), qint64(15));
        file.close();
        MainWindow window;
        window.show();
        int heartbeats = 0;
        QTimer heartbeat;
        connect(&heartbeat, &QTimer::timeout, &window, [&] { ++heartbeats; });
        heartbeat.start(10);
        window.openPath(path);
        QTRY_VERIFY_WITH_TIMEOUT(window.analysisReady(), 30000);
        QVERIFY(heartbeats > 5);
        auto* pages = window.findChild<QStackedWidget*>();
        auto* strings = dynamic_cast<StringsWidget*>(pages->widget(6));
        QVERIFY(strings);
        strings->filter("BoundaryNeedle");
        QTRY_VERIFY_WITH_TIMEOUT(
            strings->model->rowCount() > 0 && !strings->detail->text().contains("background"), 15000);
        auto* hex = dynamic_cast<HexWidget*>(pages->widget(7));
        QVERIFY(hex);
        QVERIFY(hex->jump(std::uint64_t(overlayStart) + 64 * 1024 * 1024, 14));
        HexView* view = nullptr;
        for (auto* child : hex->findChildren<QAbstractScrollArea*>())
            if (auto* found = dynamic_cast<HexView*>(child))
                view = found;
        QVERIFY(view);
        view->copyBytes(true);
        QCOMPARE(QApplication::clipboard()->text(), QString("BoundaryNeedle"));
        window.close();
    }
};
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTemporaryDir settings;
    // Qt's offscreen plugin does not enumerate Windows fonts automatically.
    if (QApplication::platformName() == "offscreen") {
        for (const auto* font : {"segoeui.ttf", "segoeuib.ttf", "consola.ttf"})
            QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR") + "/Fonts/" + font);
    }
    app.setOrganizationName("BinaryScopeTests");
    app.setApplicationName("BinaryScopeTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    applyTheme(app);
    UiTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "UiTests.moc"
