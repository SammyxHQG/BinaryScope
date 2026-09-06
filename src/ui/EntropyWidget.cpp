#include "EntropyWidget.h"
#include <QPainter>
#include <QStyledItemDelegate>
namespace bs::ui {
namespace {
class EntropyDelegate : public QStyledItemDelegate {
  public:
    explicit EntropyDelegate(QObject* parent) : QStyledItemDelegate(parent) {}
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyledItemDelegate::paint(painter, option, index);
        const auto value = index.data().toDouble();
        const auto track = option.rect.adjusted(70, 11, -18, -11);
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor("#2b3545"));
        painter->drawRoundedRect(track, 3, 3);
        auto fill = track;
        fill.setWidth(int(track.width() * std::clamp(value / 8.0, 0.0, 1.0)));
        painter->setBrush(value > 7.2 ? QColor("#e9b96c") : QColor("#67d9c2"));
        painter->drawRoundedRect(fill, 3, 3);
        painter->restore();
    }
};
} // namespace
EntropyWidget::EntropyWidget()
    : TablePage("Entropy analysis", "Shannon entropy measures byte distribution from 0 to 8 bits per byte. "
                                    "Calculated over each section's raw file data.") {
    table->setItemDelegateForColumn(2, new EntropyDelegate(table));
}
void EntropyWidget::setAnalysis(std::shared_ptr<const AnalysisResult> result) {
    setSource({"Section", "Raw bytes", "Entropy (0 – 8 bits / byte)"}, int(result->pe.sections.size()),
              [result](int row, int col) -> QVariant {
                  const auto& s = result->pe.sections[row];
                  if (col == 0)
                      return QString::fromStdString(s.name);
                  if (col == 1)
                      return s.rawSize;
                  return QString::number(s.entropy, 'f', 3);
              });
    detail->setText("High entropy can be associated with compressed or encrypted data. It is not, by itself, "
                    "proof of packing or malicious behavior. Empty sections have an entropy of 0.");
}
} // namespace bs::ui
