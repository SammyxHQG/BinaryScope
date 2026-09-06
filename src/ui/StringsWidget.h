#pragma once
#include "TablePage.h"
#include "core/AnalysisResult.h"
#include <QComboBox>
#include <QFutureWatcher>
#include <QSpinBox>
#include <QTimer>
#include <atomic>
namespace bs::ui {
class StringsWidget : public TablePage {
  public:
    StringsWidget();
    ~StringsWidget() override;
    void setAnalysis(std::shared_ptr<const AnalysisResult> result);
    void setMinimumLength(int length) { minimum_->setValue(length); }

  private:
    void refresh();
    std::shared_ptr<const AnalysisResult> result_;
    QComboBox* encoding_;
    QSpinBox* minimum_;
    QTimer debounce_;
    std::shared_ptr<std::atomic_bool> cancel_;
    std::uint64_t generation_{};
    std::shared_ptr<const std::vector<std::size_t>> visible_;
};
QString stringText(const AnalysisResult& result, const ExtractedString& entry, std::size_t maxLength = 4096);
} // namespace bs::ui
