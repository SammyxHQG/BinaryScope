#pragma once
#include "TablePage.h"
#include "analysis/Disassembler.h"
#include "core/AnalysisResult.h"
#include <QComboBox>
#include <atomic>
namespace bs::ui {
class DisassemblyWidget : public TablePage {
  public:
    DisassemblyWidget();
    ~DisassemblyWidget() override;
    void setAnalysis(std::shared_ptr<const AnalysisResult> result);
    void entryPoint();

  private:
    void decode(std::uint32_t rva);
    QComboBox* section_;
    QLineEdit* address_;
    std::shared_ptr<const AnalysisResult> result_;
    std::shared_ptr<std::atomic_bool> cancel_;
    std::uint64_t generation_{};
};
} // namespace bs::ui
