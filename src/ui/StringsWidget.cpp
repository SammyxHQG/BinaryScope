#include "StringsWidget.h"
#include <QApplication>
#include <QClipboard>
#include <QPushButton>
#include <QSettings>
#include <QtConcurrent>
namespace bs::ui {
QString stringText(const AnalysisResult& result, const ExtractedString& entry, std::size_t maxLength) {
    const auto bytes = result.file->bytes();
    const auto length = std::min<std::size_t>(entry.length, maxLength);
    if (entry.encoding == StringEncoding::Ascii)
        return QString::fromLatin1(reinterpret_cast<const char*>(bytes.data() + entry.offset),
                                   qsizetype(length));
    QString text;
    text.reserve(qsizetype(length));
    for (std::size_t i = 0; i < length; ++i) {
        const auto at = entry.offset + i * 2;
        text += QChar(ushort(bytes[at] | (ushort(bytes[at + 1]) << 8)));
    }
    return text;
}
StringsWidget::StringsWidget()
    : TablePage("Strings", "Readable ASCII and null-terminated UTF-16LE text at either byte alignment. "
                           "Wide-text detection uses conservative Unicode ranges.") {
    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel("Minimum length", this));
    minimum_ = new QSpinBox(this);
    minimum_->setRange(2, 1024);
    minimum_->setValue(QSettings().value("strings/minimum", 4).toInt());
    controls->addWidget(minimum_);
    encoding_ = new QComboBox(this);
    encoding_->addItems({"All encodings", "ASCII", "UTF-16LE"});
    encoding_->setCurrentIndex(std::clamp(QSettings().value("strings/encoding", 0).toInt(), 0, 2));
    controls->addWidget(encoding_);
    controls->addStretch();
    auto* copyFull = new QPushButton("Copy full string", this);
    controls->addWidget(copyFull);
    layout->insertLayout(3, controls);
    connect(copyFull, &QPushButton::clicked, this, [this] {
        const auto index = table->currentIndex();
        if (!index.isValid() || !visible_ || !result_)
            return;
        const auto row = proxy->mapToSource(index).row();
        if (row < 0 || std::size_t(row) >= visible_->size())
            return;
        const auto& entry = result_->strings.entries[(*visible_)[row]];
        if (entry.length > 8 * 1024 * 1024) {
            detail->setText("Full-string copy is limited to 8 million code units per selection.");
            return;
        }
        QApplication::clipboard()->setText(stringText(*result_, entry, entry.length));
        detail->setText("Complete string copied to clipboard.");
    });
    search->setMaxLength(4096);
    // The generic proxy filter is synchronous. Strings use a cancellable worker instead.
    disconnect(search, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterFixedString);
    table->setSortingEnabled(false);
    debounce_.setSingleShot(true);
    debounce_.setInterval(180);
    connect(search, &QLineEdit::textChanged, this, [this] {
        if (cancel_)
            cancel_->store(true);
        ++generation_;
        debounce_.start();
    });
    connect(&debounce_, &QTimer::timeout, this, &StringsWidget::refresh);
    connect(minimum_, &QSpinBox::valueChanged, this, [this](int value) {
        QSettings().setValue("strings/minimum", value);
        refresh();
    });
    connect(encoding_, &QComboBox::currentIndexChanged, this, [this](int value) {
        QSettings().setValue("strings/encoding", value);
        refresh();
    });
}
StringsWidget::~StringsWidget() {
    if (cancel_)
        cancel_->store(true);
}
void StringsWidget::setAnalysis(std::shared_ptr<const AnalysisResult> result) {
    result_ = std::move(result);
    visible_.reset();
    setRows({"Offset", "Encoding", "Length", "String"}, {});
    refresh();
}
void StringsWidget::refresh() {
    if (!result_)
        return;
    if (cancel_)
        cancel_->store(true);
    cancel_ = std::make_shared<std::atomic_bool>(false);
    const auto cancel = cancel_;
    const auto generation = ++generation_;
    const auto result = result_;
    const auto query = search->text();
    const int minimum = minimum_->value(), encoding = encoding_->currentIndex();
    detail->setText("Filtering strings in background...");
    auto* watcher = new QFutureWatcher<std::vector<std::size_t>>(this);
    connect(watcher, &QFutureWatcher<std::vector<std::size_t>>::finished, this,
            [this, watcher, result, generation] {
                watcher->deleteLater();
                if (generation != generation_)
                    return;
                try {
                    auto matches = std::make_shared<const std::vector<std::size_t>>(watcher->result());
                    visible_ = matches;
                    setSource({"Offset", "Encoding", "Length", "String (preview up to 4096 characters)"},
                              int(matches->size()), [result, matches](int row, int col) -> QVariant {
                                  const auto& entry = result->strings.entries[(*matches)[row]];
                                  switch (col) {
                                  case 0:
                                      return hex(entry.offset, 8);
                                  case 1:
                                      return entry.encoding == StringEncoding::Ascii ? "ASCII" : "UTF-16LE";
                                  case 2:
                                      return entry.length;
                                  default:
                                      return stringText(*result, entry);
                                  }
                              });
                    detail->setText(
                        QString("%1 matches / %2 indexed strings. %3")
                            .arg(matches->size())
                            .arg(result->strings.entries.size())
                            .arg(result->strings.limited
                                     ? "Index capped at 1,000,000 entries; ASCII is indexed first."
                                     : "Search checks complete strings. Use Copy full string for text longer "
                                       "than the preview."));
                } catch (const std::exception& e) {
                    detail->setText(QString::fromUtf8(e.what()));
                }
            });
    watcher->setFuture(QtConcurrent::run([result, query, minimum, encoding, cancel] {
        std::vector<std::size_t> matches;
        for (std::size_t i = 0; i < result->strings.entries.size(); ++i) {
            if (cancel->load())
                return matches;
            const auto& entry = result->strings.entries[i];
            if (entry.length < unsigned(minimum) ||
                (encoding == 1 && entry.encoding != StringEncoding::Ascii) ||
                (encoding == 2 && entry.encoding != StringEncoding::Utf16LE))
                continue;
            bool found = query.isEmpty();
            // Decode bounded overlapping windows instead of allocating an entire huge string.
            for (std::size_t at = 0; !found && at < entry.length; at += 4096) {
                if (cancel->load())
                    return matches;
                const auto width = entry.encoding == StringEncoding::Ascii ? 1 : 2;
                const ExtractedString window{entry.offset + at * width, std::uint32_t(entry.length - at),
                                             entry.encoding};
                found = stringText(*result, window, 4096 + std::size_t(query.size()))
                            .contains(query, Qt::CaseInsensitive);
            }
            if (found)
                matches.push_back(i);
        }
        return matches;
    }));
}
} // namespace bs::ui
