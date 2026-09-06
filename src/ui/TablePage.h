#pragma once
#include <QAbstractTableModel>
#include <QLabel>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>
namespace bs::ui {
QString hex(std::uint64_t value, int width = 0);
// Lazy cell formatting keeps large tables from allocating an item per cell.
class TableModel : public QAbstractTableModel {
  public:
    using Getter = std::function<QVariant(int, int)>;
    explicit TableModel(QObject* parent) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : rows_; }
    int columnCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : int(headers_.size());
    }
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    void reset(QStringList headers, int rows, Getter getter);

  private:
    QStringList headers_;
    int rows_{};
    Getter getter_;
};
class TablePage : public QWidget {
  public:
    TablePage(const QString& title, const QString& description, QWidget* parent = nullptr);
    void setRows(QStringList headers, QList<QStringList> rows);
    void setSource(QStringList headers, int count, TableModel::Getter getter);
    void filter(const QString& text) { search->setText(text); }
    QVBoxLayout* layout;
    QLineEdit* search;
    QTableView* table;
    TableModel* model;
    QSortFilterProxyModel* proxy;
    QLabel* detail;
};
} // namespace bs::ui
