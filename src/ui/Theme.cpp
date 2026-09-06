#include "Theme.h"
#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QStyleFactory>
namespace bs::ui {
void applyTheme(QApplication& app) {
    QPixmap mark(64, 64);
    mark.fill(Qt::transparent);
    QPainter painter(&mark);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#1b3032"));
    painter.drawRoundedRect(0, 0, 64, 64, 14, 14);
    painter.setPen(QPen(QColor("#67d9c2"), 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPolyline(QPolygonF{QPointF(23, 18), QPointF(14, 18), QPointF(14, 46), QPointF(23, 46)});
    painter.drawPolyline(QPolygonF{QPointF(41, 18), QPointF(50, 18), QPointF(50, 46), QPointF(41, 46)});
    painter.drawLine(37, 20, 27, 44);
    painter.end();
    app.setWindowIcon(QIcon(mark));
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont("Segoe UI", 10));
    app.setStyleSheet(R"(
      QWidget { background:#11151c; color:#d9e1ed; }
      QMainWindow, QStackedWidget { background:#11151c; }
      QMenuBar, QToolBar, QStatusBar { background:#171d27; border:0; }
      QToolBar { spacing:12px; padding:10px 18px; border-bottom:1px solid #2a3240; }
      QToolButton, QPushButton { padding:8px 16px; border:1px solid #303b4b; border-radius:5px; background:#202938; }
      QToolButton:hover, QPushButton:hover { background:#2b3a4d; }
      QPushButton#primary { background:#67d9c2; color:#102621; font-weight:600; border:0; }
      QPushButton#primary:hover { background:#8fe8d6; }
      QWidget#sidebar, QWidget#sidebar QLabel, QListWidget { background:#171d27; }
      QWidget#sidebar { border-right:1px solid #2a3240; }
      QListWidget { border:0; outline:0; font-size:14px; }
      QListWidget::item { padding:12px; margin:2px 0; border-radius:5px; }
      QListWidget::item:selected { background:#243d3c; color:#84ebd5; border-left:3px solid #67d9c2; }
      QListWidget::item:hover { background:#222d3c; }
      QLabel#brand { font-size:21px; font-weight:700; border:0; }
      QLabel#eyebrow { font-size:9px; color:#8291a6; border:0; }
      QLabel#muted { color:#92a0b4; border:0; }
      QLabel#pageTitle { font-size:26px; font-weight:600; }
      QLabel#heroTitle { font-size:34px; font-weight:600; }
      QLabel#heroSubtitle { font-size:15px; color:#92a0b4; line-height:1.6; }
      QLabel#heroIcon { font-family:Consolas; font-size:42px; color:#67d9c2; padding-bottom:30px; }
      QLineEdit, QComboBox, QSpinBox { background:#1a2230; border:1px solid #303b4b; border-radius:5px; padding:8px; selection-background-color:#315e5b; }
      QLineEdit:focus { border:1px solid #67d9c2; }
      QTableView { background:#141a24; alternate-background-color:#181f2b; border:1px solid #293342; selection-background-color:#2b4a4b; selection-color:#cafff3; font-family:Consolas; font-size:12px; }
      QHeaderView::section { background:#202938; color:#aab9cf; border:0; border-bottom:1px solid #344154; padding:10px; font-family:'Segoe UI'; }
      QScrollBar:vertical { background:#171d27; width:12px; }
      QScrollBar::handle:vertical { background:#3b485d; min-height:28px; border-radius:5px; }
      QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
      QProgressBar { background:#202938; border:0; height:6px; }
      QProgressBar::chunk { background:#67d9c2; }
      QMenu { background:#202938; border:1px solid #344154; padding:4px; }
      QMenu::item:selected { background:#31514e; }
    )");
}
} // namespace bs::ui
