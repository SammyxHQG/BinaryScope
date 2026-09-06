#pragma once
#include "TablePage.h"
#include "core/AnalysisResult.h"
namespace bs::ui {
class OverviewWidget : public TablePage {
  public:
    explicit OverviewWidget(QWidget* parent = nullptr);
    void setAnalysis(std::shared_ptr<const AnalysisResult> result);
};
} // namespace bs::ui
