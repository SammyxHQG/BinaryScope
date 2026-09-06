#pragma once
#include "TablePage.h"
#include "core/AnalysisResult.h"
namespace bs::ui {
class EntropyWidget : public TablePage {
  public:
    EntropyWidget();
    void setAnalysis(std::shared_ptr<const AnalysisResult> result);
};
} // namespace bs::ui
