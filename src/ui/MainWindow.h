#pragma once
#include "DisassemblyWidget.h"
#include "EntropyWidget.h"
#include "HexWidget.h"
#include "OverviewWidget.h"
#include "PEWidgets.h"
#include "StringsWidget.h"
#include <QFutureWatcher>
#include <QListWidget>
#include <QMainWindow>
#include <QProgressBar>
#include <QStackedWidget>
namespace bs::ui {
class MainWindow : public QMainWindow {
  public:
    MainWindow();
    void openPath(const QString& path);
    [[nodiscard]] bool analysisReady() const { return result_ && !watcher_.isRunning(); }

  protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

  private:
    void chooseFile();
    void showSearch();
    void showSettings();
    QListWidget* navigation_;
    QStackedWidget* pages_;
    OverviewWidget* overview_;
    HeaderWidget* headers_;
    SectionWidget* sections_;
    ImportWidget* imports_;
    ExportWidget* exports_;
    StringsWidget* strings_;
    HexWidget* hex_;
    DisassemblyWidget* disassembly_;
    EntropyWidget* entropy_;
    QProgressBar* progress_;
    QLabel* fileLabel_;
    QFutureWatcher<std::shared_ptr<AnalysisResult>> watcher_;
    std::shared_ptr<const AnalysisResult> result_;
    QString currentPath_;
};
} // namespace bs::ui
