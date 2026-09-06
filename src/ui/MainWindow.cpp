#include "MainWindow.h"
#include "core/AnalysisService.h"
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QtConcurrent>
namespace bs::ui {
MainWindow::MainWindow() {
    setWindowTitle("BinaryScope");
    resize(1280, 820);
    setMinimumSize(900, 600);
    setAcceptDrops(true);
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    auto* fileMenu = menuBar()->addMenu("&File");
    auto* open = toolbar->addAction("Open binary");
    open->setShortcut(QKeySequence::Open);
    fileMenu->addAction(open);
    connect(open, &QAction::triggered, this, &MainWindow::chooseFile);
    auto* reload = toolbar->addAction("Reload");
    reload->setShortcut(QKeySequence("Ctrl+R"));
    fileMenu->addAction(reload);
    connect(reload, &QAction::triggered, this, [this] {
        if (!currentPath_.isEmpty())
            openPath(currentPath_);
    });
    toolbar->addSeparator();
    auto* search = toolbar->addAction("Search");
    search->setShortcut(QKeySequence::Find);
    connect(search, &QAction::triggered, this, &MainWindow::showSearch);
    auto* settings = toolbar->addAction("Settings");
    connect(settings, &QAction::triggered, this, &MainWindow::showSettings);
    auto* body = new QWidget(this);
    auto* horizontal = new QHBoxLayout(body);
    horizontal->setContentsMargins(0, 0, 0, 0);
    horizontal->setSpacing(0);
    auto* sidebar = new QWidget(body);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(210);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(16, 24, 16, 16);
    auto* brand = new QLabel("BINARY<span style='color:#67d9c2'>SCOPE</span>", sidebar);
    brand->setObjectName("brand");
    side->addWidget(brand);
    auto* caption = new QLabel("STATIC BINARY INSPECTION", sidebar);
    caption->setObjectName("eyebrow");
    side->addWidget(caption);
    side->addSpacing(30);
    navigation_ = new QListWidget(sidebar);
    navigation_->addItems({"Overview", "PE Headers", "Sections", "Imports", "Exports", "Strings",
                           "Hex Viewer", "Disassembly", "Entropy"});
    side->addWidget(navigation_);
    fileLabel_ = new QLabel("NO FILE LOADED\n\nPE32 / PE32+", sidebar);
    fileLabel_->setTextFormat(Qt::PlainText);
    fileLabel_->setObjectName("muted");
    fileLabel_->setWordWrap(true);
    side->addWidget(fileLabel_);
    horizontal->addWidget(sidebar);
    pages_ = new QStackedWidget(body);
    horizontal->addWidget(pages_, 1);
    setCentralWidget(body);
    auto* welcome = new QWidget(pages_);
    auto* welcomeLayout = new QVBoxLayout(welcome);
    welcomeLayout->setContentsMargins(80, 80, 80, 80);
    welcomeLayout->addStretch();
    auto* badge = new QLabel("[  B / S  ]", welcome);
    badge->setObjectName("heroIcon");
    welcomeLayout->addWidget(badge);
    auto* title = new QLabel("Every binary has a story.", welcome);
    title->setObjectName("heroTitle");
    welcomeLayout->addWidget(title);
    auto* sub = new QLabel("Explore the structure beneath the executable.\nHeaders, sections, symbols and "
                           "bytes — in one focused workspace.",
                           welcome);
    sub->setObjectName("heroSubtitle");
    sub->setWordWrap(true);
    welcomeLayout->addWidget(sub);
    welcomeLayout->addSpacing(22);
    auto* button = new QPushButton("Open Binary    Ctrl+O", welcome);
    button->setObjectName("primary");
    button->setFixedSize(240, 48);
    connect(button, &QPushButton::clicked, this, &MainWindow::chooseFile);
    welcomeLayout->addWidget(button);
    auto* drop = new QLabel("or drop an .exe or .dll anywhere here", welcome);
    drop->setObjectName("muted");
    welcomeLayout->addWidget(drop);
    welcomeLayout->addStretch();
    pages_->addWidget(welcome);
    overview_ = new OverviewWidget(pages_);
    pages_->addWidget(overview_);
    headers_ = new HeaderWidget;
    sections_ = new SectionWidget;
    imports_ = new ImportWidget;
    exports_ = new ExportWidget;
    for (auto* page : std::vector<TablePage*>{headers_, sections_, imports_, exports_})
        pages_->addWidget(page);
    strings_ = new StringsWidget;
    hex_ = new HexWidget;
    pages_->addWidget(strings_);
    pages_->addWidget(hex_);
    disassembly_ = new DisassemblyWidget;
    entropy_ = new EntropyWidget;
    pages_->addWidget(disassembly_);
    pages_->addWidget(entropy_);
    navigation_->setEnabled(false);
    connect(navigation_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (result_ && row >= 0 && row < 9)
            pages_->setCurrentIndex(row + 1);
    });
    progress_ = new QProgressBar(this);
    progress_->setRange(0, 0);
    progress_->setFixedWidth(180);
    progress_->hide();
    statusBar()->addPermanentWidget(progress_);
    statusBar()->showMessage("Ready  •  Files are never executed");
    if (QSettings().value("window/remember", true).toBool())
        restoreGeometry(QSettings().value("window/geometry").toByteArray());
    connect(&watcher_, &QFutureWatcher<std::shared_ptr<AnalysisResult>>::progressRangeChanged, progress_,
            &QProgressBar::setRange);
    connect(&watcher_, &QFutureWatcher<std::shared_ptr<AnalysisResult>>::progressValueChanged, progress_,
            &QProgressBar::setValue);
    connect(&watcher_, &QFutureWatcher<std::shared_ptr<AnalysisResult>>::progressTextChanged, statusBar(),
            [this](const QString& text) { statusBar()->showMessage(text); });
    connect(&watcher_, &QFutureWatcher<std::shared_ptr<AnalysisResult>>::finished, this, [this] {
        progress_->hide();
        try {
            const auto loaded = watcher_.result();
            if (!loaded->error.isEmpty())
                throw std::runtime_error(loaded->error.toStdString());
            result_ = loaded;
            currentPath_ = QString::fromStdWString(result_->file->path().wstring());
            overview_->setAnalysis(result_);
            headers_->setAnalysis(result_);
            sections_->setAnalysis(result_);
            imports_->setAnalysis(result_);
            exports_->setAnalysis(result_);
            strings_->setAnalysis(result_);
            hex_->setAnalysis(result_);
            disassembly_->setAnalysis(result_);
            entropy_->setAnalysis(result_);
            pages_->setCurrentWidget(overview_);
            navigation_->setEnabled(true);
            const int page = QSettings().value("window/remember", true).toBool()
                                 ? std::clamp(QSettings().value("window/page", 0).toInt(), 0, 8)
                                 : 0;
            navigation_->setCurrentRow(page);
            pages_->setCurrentIndex(page + 1);
            fileLabel_->setText(QString::fromStdWString(result_->file->path().filename().wstring()) + "\n\n" +
                                QString::fromStdString(result_->pe.architecture()));
            setWindowTitle(fileLabel_->text().section('\n', 0, 0) + " — BinaryScope");
            statusBar()->showMessage("Analysis complete  •  Static inspection");
        } catch (const std::exception& e) {
            statusBar()->showMessage("Analysis failed");
            QMessageBox::critical(this, "Cannot analyze binary", QString::fromUtf8(e.what()));
        }
    });
}
void MainWindow::chooseFile() {
    const auto path =
        QFileDialog::getOpenFileName(this, "Open binary", QSettings().value("files/lastDirectory").toString(),
                                     "Windows binaries (*.exe *.dll *.sys);;All files (*)");
    if (!path.isEmpty())
        openPath(path);
}
void MainWindow::openPath(const QString& path) {
    if (watcher_.isRunning()) {
        statusBar()->showMessage("Analysis is in progress. Please wait before opening another file.");
        return;
    }
    QSettings().setValue("files/lastDirectory", QFileInfo(path).absolutePath());
    progress_->setRange(0, 0);
    progress_->show();
    statusBar()->showMessage("Reading headers and calculating fingerprints...");
    watcher_.setFuture(QtConcurrent::run([path](QPromise<std::shared_ptr<AnalysisResult>>& promise) {
        AnalysisService::analyze(promise, path);
    }));
}
void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls() && event->mimeData()->urls().size() == 1 &&
        event->mimeData()->urls().first().isLocalFile())
        event->acceptProposedAction();
}
void MainWindow::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasUrls()) {
        openPath(event->mimeData()->urls().first().toLocalFile());
        event->acceptProposedAction();
    }
}
void MainWindow::closeEvent(QCloseEvent* event) {
    QSettings().setValue("window/geometry", saveGeometry());
    if (result_)
        QSettings().setValue("window/page", navigation_->currentRow());
    QMainWindow::closeEvent(event);
}
} // namespace bs::ui
