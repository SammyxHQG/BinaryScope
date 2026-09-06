#include "OverviewWidget.h"
#include <QDateTime>
#include <QFileInfo>
#include <QTimeZone>
namespace bs::ui {
OverviewWidget::OverviewWidget(QWidget* parent)
    : TablePage("Binary overview",
                "Identity, image metadata and cryptographic fingerprints of the file snapshot.", parent) {}
void OverviewWidget::setAnalysis(std::shared_ptr<const AnalysisResult> result) {
    const auto& p = result->pe;
    const auto path = QString::fromStdWString(result->file->path().wstring());
    QString timestamp = p.timestamp == 0 || p.timestamp == 0xffffffff
                            ? "Not specified"
                            : QDateTime::fromSecsSinceEpoch(p.timestamp, QTimeZone::UTC)
                                  .toString("yyyy-MM-dd HH:mm:ss 'UTC'");
    setRows({"Property", "Value"},
            {{"Filename", QFileInfo(path).fileName()},
             {"Full path", path},
             {"File size", QLocale().formattedDataSize(qint64(result->file->bytes().size())) + " (" +
                               QString::number(result->file->bytes().size()) + " bytes)"},
             {"SHA-256", result->hashes.sha256},
             {"MD5", result->hashes.md5},
             {"Architecture", QString::fromStdString(p.architecture())},
             {"Binary format", p.is64 ? "PE32+" : "PE32"},
             {"PE type", p.characteristics & 0x2000 ? "Dynamic-link library" : "Executable image"},
             {"Image base", hex(p.imageBase)},
             {"Entry point (VA)", p.entryRva ? hex(p.imageBase + p.entryRva) : "Not specified"},
             {"Entry point (RVA)", hex(p.entryRva)},
             {"COFF timestamp", timestamp},
             {"Number of sections", QString::number(p.sections.size())},
             {"Subsystem", QString::fromStdString(p.subsystemName())}});
    QStringList warnings;
    for (const auto& w : p.warnings)
        warnings << QString::fromStdString(w);
    detail->setText(warnings.isEmpty()
                        ? "Static inspection only. Timestamps can be altered or represent reproducible-build "
                          "metadata. MD5 is an identification fingerprint, not a security guarantee."
                        : warnings.join('\n'));
}
} // namespace bs::ui
