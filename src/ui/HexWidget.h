#pragma once
#include "core/AnalysisResult.h"
#include <QAbstractScrollArea>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>
#include <atomic>
namespace bs::ui {
class HexView : public QAbstractScrollArea {
  public:
    explicit HexView(QWidget* parent = nullptr);
    void setFile(std::shared_ptr<const BinaryFile> file);
    bool jump(std::uint64_t offset, std::uint64_t length = 1);
    void copyBytes(bool asText = false);
    std::uint64_t selectionEnd() const { return std::max(anchor_, cursor_); }
    std::function<void(const QString&)> selectionChanged;

  protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

  private:
    void updateScroll();
    void notifySelection();
    std::optional<std::uint64_t> byteAt(QPoint point) const;
    std::shared_ptr<const BinaryFile> file_;
    std::uint64_t anchor_{}, cursor_{};
    int charWidth_{}, rowHeight_{};
};
class HexWidget : public QWidget {
  public:
    HexWidget();
    ~HexWidget() override;
    void setAnalysis(std::shared_ptr<const AnalysisResult> result);
    bool jump(std::uint64_t offset, std::uint64_t length = 1);
    void searchBytes(const QString& query, bool text);

  private:
    void findNext();
    std::shared_ptr<const AnalysisResult> result_;
    HexView* view_;
    QLineEdit* query_;
    QComboBox* mode_;
    QLabel* status_;
    std::shared_ptr<std::atomic_bool> cancel_;
    std::uint64_t generation_{};
    QByteArray lastPattern_;
    std::uint64_t nextOffset_{};
};
} // namespace bs::ui
