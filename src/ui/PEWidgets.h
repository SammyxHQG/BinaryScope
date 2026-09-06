#pragma once
#include "TablePage.h"
#include "core/AnalysisResult.h"
namespace bs::ui {
class HeaderWidget : public TablePage {
  public:
    HeaderWidget();
    void setAnalysis(std::shared_ptr<const AnalysisResult> result);
};
class SectionWidget : public TablePage {
  public:
    SectionWidget();
    void setAnalysis(std::shared_ptr<const AnalysisResult> result);
};
class ImportWidget : public TablePage {
  public:
    ImportWidget();
    void setAnalysis(std::shared_ptr<const AnalysisResult> result);
};
class ExportWidget : public TablePage {
  public:
    ExportWidget();
    void setAnalysis(std::shared_ptr<const AnalysisResult> result);
};
} // namespace bs::ui
