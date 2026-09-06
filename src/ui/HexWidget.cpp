#include "HexWidget.h"
#include "TablePage.h"
#include "analysis/ByteSearch.h"
#include <QApplication>
#include <QClipboard>
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QtConcurrent>
namespace bs::ui {
HexView::HexView(QWidget* parent) : QAbstractScrollArea(parent) {
    setFont(QFont("Consolas", 11));
    charWidth_ = fontMetrics().horizontalAdvance('0');
    rowHeight_ = fontMetrics().height() + 8;
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(200);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, viewport(), qOverload<>(&QWidget::update));
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, viewport(), qOverload<>(&QWidget::update));
}
void HexView::setFile(std::shared_ptr<const BinaryFile> file) {
    file_ = std::move(file);
    anchor_ = cursor_ = 0;
    updateScroll();
    verticalScrollBar()->setValue(0);
    viewport()->update();
}
void HexView::updateScroll() {
    const auto rows = file_ ? (file_->bytes().size() + 15) / 16 : 0;
    const int visible = std::max(1, viewport()->height() / rowHeight_ - 1);
    verticalScrollBar()->setPageStep(visible);
    verticalScrollBar()->setRange(0, std::max(0, int(rows) - visible));
    horizontalScrollBar()->setRange(0, std::max(0, charWidth_ * 80 + 24 - viewport()->width()));
}
void HexView::resizeEvent(QResizeEvent* event) {
    QAbstractScrollArea::resizeEvent(event);
    updateScroll();
}
void HexView::paintEvent(QPaintEvent*) {
    QPainter p(viewport());
    p.fillRect(viewport()->rect(), QColor("#141a24"));
    p.setFont(font());
    p.translate(-horizontalScrollBar()->value(), 0);
    const int hexX = 12 + charWidth_ * 12, asciiX = hexX + charWidth_ * 50;
    p.fillRect(horizontalScrollBar()->value(), 0, viewport()->width(), rowHeight_, QColor("#202938"));
    p.setPen(QColor("#95a7be"));
    p.drawText(12, rowHeight_ - 8, "OFFSET");
    for (int c = 0; c < 16; ++c)
        p.drawText(hexX + c * 3 * charWidth_, rowHeight_ - 8,
                   QString::number(c, 16).toUpper().rightJustified(2, '0'));
    p.drawText(asciiX, rowHeight_ - 8, "ASCII");
    if (!file_)
        return;
    const auto bytes = file_->bytes();
    for (int row = 0; row <= viewport()->height() / rowHeight_; ++row) {
        const auto offset = std::uint64_t(verticalScrollBar()->value() + row) * 16;
        if (offset >= bytes.size())
            break;
        const int y = (row + 1) * rowHeight_;
        if (row % 2)
            p.fillRect(horizontalScrollBar()->value(), y, viewport()->width(), rowHeight_, QColor("#181f2b"));
        p.setPen(QColor("#7b90aa"));
        p.drawText(12, y + rowHeight_ - 8, QString::number(offset, 16).toUpper().rightJustified(8, '0'));
        for (int c = 0; c < 16 && offset + c < bytes.size(); ++c) {
            const auto at = offset + c;
            const auto b = bytes[at];
            const bool selected = at >= std::min(anchor_, cursor_) && at <= std::max(anchor_, cursor_);
            if (selected) {
                p.fillRect(hexX + c * 3 * charWidth_ - 2, y, charWidth_ * 3, rowHeight_, QColor("#305c58"));
                p.fillRect(asciiX + c * charWidth_, y, charWidth_, rowHeight_, QColor("#305c58"));
            }
            p.setPen(selected ? QColor("#b4ffed") : b == 0 ? QColor("#566579") : QColor("#d6e1ef"));
            p.drawText(hexX + c * 3 * charWidth_, y + rowHeight_ - 8,
                       QString::number(b, 16).toUpper().rightJustified(2, '0'));
            p.drawText(asciiX + c * charWidth_, y + rowHeight_ - 8,
                       QString(QChar(b >= 32 && b < 127 ? b : '.')));
        }
    }
}
std::optional<std::uint64_t> HexView::byteAt(QPoint point) const {
    if (!file_ || point.y() < rowHeight_)
        return {};
    const int x = point.x() + horizontalScrollBar()->value(), hexX = 12 + charWidth_ * 12,
              asciiX = hexX + charWidth_ * 50;
    int col = -1;
    if (x >= hexX && x < hexX + 48 * charWidth_)
        col = (x - hexX) / (3 * charWidth_);
    else if (x >= asciiX && x < asciiX + 16 * charWidth_)
        col = (x - asciiX) / charWidth_;
    if (col < 0 || col > 15)
        return {};
    const auto offset = std::uint64_t(verticalScrollBar()->value() + point.y() / rowHeight_ - 1) * 16 + col;
    return offset < file_->bytes().size() ? std::optional<std::uint64_t>(offset) : std::nullopt;
}
void HexView::notifySelection() {
    viewport()->update();
    if (selectionChanged)
        selectionChanged(
            QString("Selected %1 – %2  ·  %3 bytes  ·  Ctrl+C copies hex; Ctrl+Shift+C copies text")
                .arg(hex(std::min(anchor_, cursor_)), hex(std::max(anchor_, cursor_)))
                .arg(std::max(anchor_, cursor_) - std::min(anchor_, cursor_) + 1));
}
void HexView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        if (const auto at = byteAt(event->pos())) {
            cursor_ = *at;
            if (!(event->modifiers() & Qt::ShiftModifier))
                anchor_ = cursor_;
            setFocus();
            notifySelection();
        }
}
void HexView::mouseMoveEvent(QMouseEvent* event) {
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if (event->pos().y() < rowHeight_)
        verticalScrollBar()->setValue(verticalScrollBar()->value() - 1);
    if (event->pos().y() > viewport()->height() - rowHeight_)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + 1);
    if (const auto at = byteAt(event->pos())) {
        cursor_ = *at;
        notifySelection();
    }
}
bool HexView::jump(std::uint64_t offset, std::uint64_t length) {
    if (!file_ || offset >= file_->bytes().size() || !length || length > file_->bytes().size() - offset)
        return false;
    anchor_ = offset;
    cursor_ = offset + length - 1;
    verticalScrollBar()->setValue(int(offset / 16));
    notifySelection();
    setFocus();
    return true;
}
void HexView::copyBytes(bool asText) {
    if (!file_)
        return;
    const auto first = std::min(anchor_, cursor_), length = std::max(anchor_, cursor_) - first + 1;
    if (length > 16 * 1024 * 1024) {
        if (selectionChanged)
            selectionChanged("Copy is limited to 16 MiB per selection. Select a smaller range.");
        return;
    }
    const QByteArray bytes(reinterpret_cast<const char*>(file_->bytes().data() + first), qsizetype(length));
    QApplication::clipboard()->setText(asText ? QString::fromLatin1(bytes)
                                              : QString::fromLatin1(bytes.toHex(' ').toUpper()));
}
void HexView::keyPressEvent(QKeyEvent* event) {
    if (!file_)
        return;
    if (event->matches(QKeySequence::Copy)) {
        copyBytes();
        return;
    }
    if (event->key() == Qt::Key_C && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        copyBytes(true);
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        anchor_ = 0;
        cursor_ = file_->bytes().size() - 1;
        notifySelection();
        return;
    }
    std::int64_t next = static_cast<std::int64_t>(cursor_);
    switch (event->key()) {
    case Qt::Key_Left:
        --next;
        break;
    case Qt::Key_Right:
        ++next;
        break;
    case Qt::Key_Up:
        next -= 16;
        break;
    case Qt::Key_Down:
        next += 16;
        break;
    case Qt::Key_PageUp:
        next -= verticalScrollBar()->pageStep() * 16;
        break;
    case Qt::Key_PageDown:
        next += verticalScrollBar()->pageStep() * 16;
        break;
    case Qt::Key_Home:
        next = (event->modifiers() & Qt::ControlModifier) ? 0 : next / 16 * 16;
        break;
    case Qt::Key_End:
        next = (event->modifiers() & Qt::ControlModifier) ? std::int64_t(file_->bytes().size() - 1)
                                                          : next / 16 * 16 + 15;
        break;
    default:
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    cursor_ = std::uint64_t(std::clamp<std::int64_t>(next, 0, std::int64_t(file_->bytes().size() - 1)));
    if (!(event->modifiers() & Qt::ShiftModifier))
        anchor_ = cursor_;
    const int row = int(cursor_ / 16);
    if (row < verticalScrollBar()->value())
        verticalScrollBar()->setValue(row);
    else if (row >= verticalScrollBar()->value() + verticalScrollBar()->pageStep())
        verticalScrollBar()->setValue(row - verticalScrollBar()->pageStep() + 1);
    notifySelection();
}
HexWidget::HexWidget() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(16);
    auto* title = new QLabel("Hex viewer", this);
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    auto* description = new QLabel(
        "The complete file snapshot, byte for byte. Select across rows using drag or Shift + arrow keys.",
        this);
    description->setObjectName("muted");
    layout->addWidget(description);
    auto* controls = new QHBoxLayout;
    auto* offset = new QLineEdit(this);
    offset->setPlaceholderText("Offset (hex, e.g. 0x200)");
    auto* go = new QPushButton("Jump", this);
    controls->addWidget(offset);
    controls->addWidget(go);
    mode_ = new QComboBox(this);
    mode_->addItems({"Hex bytes", "Text (UTF-8)", "Text (UTF-16LE)"});
    controls->addWidget(mode_);
    query_ = new QLineEdit(this);
    query_->setPlaceholderText("4D 5A or text");
    controls->addWidget(query_, 1);
    auto* find = new QPushButton("Find next", this);
    controls->addWidget(find);
    layout->addLayout(controls);
    view_ = new HexView(this);
    layout->addWidget(view_, 1);
    status_ = new QLabel("Offsets are hexadecimal. Searches wrap at the end of the file.", this);
    status_->setObjectName("muted");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    view_->selectionChanged = [this](const QString& text) { status_->setText(text); };
    const auto jumpTo = [this, offset] {
        bool ok = false;
        const auto value = offset->text().trimmed().toULongLong(&ok, 16);
        if (!ok || !jump(value))
            status_->setText("Invalid offset: enter a hexadecimal offset inside the file.");
    };
    connect(go, &QPushButton::clicked, this, jumpTo);
    connect(offset, &QLineEdit::returnPressed, this, jumpTo);
    connect(find, &QPushButton::clicked, this, &HexWidget::findNext);
    connect(query_, &QLineEdit::returnPressed, this, &HexWidget::findNext);
}
HexWidget::~HexWidget() {
    if (cancel_)
        cancel_->store(true);
}
bool HexWidget::jump(std::uint64_t offset, std::uint64_t length) {
    if (cancel_)
        cancel_->store(true);
    ++generation_;
    return view_->jump(offset, length);
}
void HexWidget::setAnalysis(std::shared_ptr<const AnalysisResult> result) {
    if (cancel_)
        cancel_->store(true);
    ++generation_;
    result_ = std::move(result);
    view_->setFile(result_->file);
    lastPattern_.clear();
    nextOffset_ = 0;
}
void HexWidget::searchBytes(const QString& query, bool text) {
    mode_->setCurrentIndex(text ? 1 : 0);
    query_->setText(query);
    findNext();
}
void HexWidget::findNext() {
    if (!result_)
        return;
    QByteArray pattern;
    if (mode_->currentIndex() == 0) {
        QString cleaned = query_->text();
        cleaned.remove(QRegularExpression("\\s"));
        if (cleaned.isEmpty() || cleaned.size() % 2 || cleaned.contains(QRegularExpression("[^0-9a-fA-F]"))) {
            status_->setText("Enter complete hex byte pairs, for example: 4D 5A 90 00.");
            return;
        }
        pattern = QByteArray::fromHex(cleaned.toLatin1());
    } else if (mode_->currentIndex() == 1)
        pattern = query_->text().toUtf8();
    else {
        for (const QChar c : query_->text()) {
            pattern.append(char(c.unicode() & 255));
            pattern.append(char(c.unicode() >> 8));
        }
    }
    if (pattern.isEmpty()) {
        status_->setText("Enter a non-empty search value.");
        return;
    }
    if (pattern != lastPattern_)
        nextOffset_ = 0;
    lastPattern_ = pattern;
    if (cancel_)
        cancel_->store(true);
    cancel_ = std::make_shared<std::atomic_bool>(false);
    const auto cancel = cancel_;
    const auto generation = ++generation_;
    const auto result = result_;
    const auto from = nextOffset_;
    status_->setText("Searching in background...");
    auto* watcher = new QFutureWatcher<std::optional<std::uint64_t>>(this);
    connect(watcher, &QFutureWatcher<std::optional<std::uint64_t>>::finished, this,
            [this, watcher, generation, pattern] {
                watcher->deleteLater();
                if (generation != generation_)
                    return;
                try {
                    const auto match = watcher->result();
                    if (match) {
                        view_->jump(*match, std::uint64_t(pattern.size()));
                        nextOffset_ = *match + 1;
                    } else
                        status_->setText("No matching byte sequence found.");
                } catch (const std::exception& e) {
                    status_->setText(QString::fromUtf8(e.what()));
                }
            });
    watcher->setFuture(QtConcurrent::run([result, pattern, from, cancel] {
        const std::span<const std::uint8_t> needle(reinterpret_cast<const std::uint8_t*>(pattern.constData()),
                                                   std::size_t(pattern.size()));
        auto match = ByteSearch::find(result->file->bytes(), needle, from, cancel.get());
        if (!match && from && !cancel->load())
            match = ByteSearch::find(result->file->bytes(), needle, 0, cancel.get());
        return match;
    }));
}
} // namespace bs::ui
