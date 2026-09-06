#include "TablePage.h"
#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QMenu>
#include <QShortcut>
#include <algorithm>
namespace bs::ui {
namespace {
class NumericProxy : public QSortFilterProxyModel {
  public:
    explicit NumericProxy(QObject* parent) : QSortFilterProxyModel(parent) {}

  protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override {
        const auto a = left.data().toString(), b = right.data().toString();
        if (a.startsWith("0x") && b.startsWith("0x")) {
            bool validA = false, validB = false;
            const auto av = a.toULongLong(&validA, 16), bv = b.toULongLong(&validB, 16);
            if (validA && validB)
                return av < bv;
        }
        return QSortFilterProxyModel::lessThan(left, right);
    }
};
} // namespace
QString hex(std::uint64_t value, int width) {
    return "0x" + QString::number(value, 16).toUpper().rightJustified(width, '0');
}
QVariant TableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rows_ || !getter_)
        return {};
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
        return getter_(index.row(), index.column());
    return {};
}
QVariant TableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < headers_.size())
        return headers_[section];
    return {};
}
void TableModel::reset(QStringList headers, int rows, Getter getter) {
    beginResetModel();
    headers_ = std::move(headers);
    rows_ = rows;
    getter_ = std::move(getter);
    endResetModel();
}
TablePage::TablePage(const QString& title, const QString& description, QWidget* parent) : QWidget(parent) {
    layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(16);
    auto* heading = new QLabel(title, this);
    heading->setObjectName("pageTitle");
    layout->addWidget(heading);
    auto* caption = new QLabel(description, this);
    caption->setObjectName("muted");
    caption->setWordWrap(true);
    layout->addWidget(caption);
    search = new QLineEdit(this);
    search->setPlaceholderText("Filter this view...");
    search->setClearButtonEnabled(true);
    layout->addWidget(search);
    model = new TableModel(this);
    proxy = new NumericProxy(this);
    proxy->setSourceModel(model);
    proxy->setFilterKeyColumn(-1);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    table = new QTableView(this);
    table->setModel(proxy);
    table->setAlternatingRowColors(true);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setSortingEnabled(true);
    table->sortByColumn(-1, Qt::AscendingOrder);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(32);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultSectionSize(170);
    table->setShowGrid(false);
    layout->addWidget(table, 1);
    detail = new QLabel("Select cells and press Ctrl+C to copy.", this);
    detail->setObjectName("muted");
    detail->setWordWrap(true);
    detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(detail);
    connect(search, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterFixedString);
    const auto copy = [this] {
        auto indexes = table->selectionModel()->selectedIndexes();
        std::sort(indexes.begin(), indexes.end(), [](const auto& a, const auto& b) {
            return a.row() == b.row() ? a.column() < b.column() : a.row() < b.row();
        });
        QString text;
        int previous = -1;
        for (const auto& index : indexes) {
            if (!text.isEmpty())
                text += previous == index.row() ? '\t' : '\n';
            text += index.data().toString();
            previous = index.row();
        }
        if (!text.isEmpty())
            QApplication::clipboard()->setText(text);
    };
    auto* shortcut = new QShortcut(QKeySequence::Copy, table);
    connect(shortcut, &QShortcut::activated, this, copy);
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(table, &QWidget::customContextMenuRequested, this, [this, copy](QPoint point) {
        QMenu menu(this);
        menu.addAction("Copy selected cells", copy);
        menu.exec(table->viewport()->mapToGlobal(point));
    });
}
void TablePage::setRows(QStringList headers, QList<QStringList> rows) {
    const int count = int(rows.size());
    setSource(std::move(headers), count,
              [rows = std::move(rows)](int row, int column) -> QVariant { return rows[row].value(column); });
}
void TablePage::setSource(QStringList headers, int count, TableModel::Getter getter) {
    model->reset(std::move(headers), count, std::move(getter));
    table->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    proxy->sort(-1);
}
} // namespace bs::ui
