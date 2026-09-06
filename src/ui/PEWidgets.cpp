#include "PEWidgets.h"
#include <QHeaderView>
namespace bs::ui {
HeaderWidget::HeaderWidget()
    : TablePage("PE headers", "Decoded DOS, NT, COFF and optional headers. Addresses in data directories are "
                              "RVAs except the certificate table.") {}
void HeaderWidget::setAnalysis(std::shared_ptr<const AnalysisResult> result) {
    QList<QStringList> rows;
    for (const auto& f : result->pe.fields)
        rows.push_back({QString::fromStdString(f.group), QString::fromStdString(f.name), hex(f.value),
                        QString::fromStdString(f.description)});
    const QStringList names{"Export table",
                            "Import table",
                            "Resources",
                            "Exception table",
                            "Certificate table",
                            "Base relocations",
                            "Debug",
                            "Architecture",
                            "Global pointer",
                            "TLS",
                            "Load configuration",
                            "Bound imports",
                            "IAT",
                            "Delay imports",
                            "CLR runtime",
                            "Reserved"};
    for (std::size_t i = 0; i < result->pe.directories.size(); ++i) {
        const auto& d = result->pe.directories[i];
        rows.push_back({"Data directories", names.value(int(i), "Directory " + QString::number(i)),
                        hex(d.rva),
                        (i == 4 ? "File offset" : "RVA") +
                            QString("; size %1 bytes (%2)").arg(d.size).arg(hex(d.size))});
    }
    setRows({"Header", "Field", "Value", "Description"}, std::move(rows));
}
SectionWidget::SectionWidget()
    : TablePage("Sections", "Physical file ranges, virtual layout and section permissions. Entropy is "
                            "measured over raw bytes.") {}
void SectionWidget::setAnalysis(std::shared_ptr<const AnalysisResult> result) {
    setSource({"Name", "Virtual address (RVA)", "Virtual size", "Raw offset", "Raw size", "Characteristics",
               "Entropy"},
              int(result->pe.sections.size()), [result](int row, int col) -> QVariant {
                  const auto& s = result->pe.sections[row];
                  switch (col) {
                  case 0:
                      return QString::fromStdString(s.name);
                  case 1:
                      return hex(s.virtualAddress);
                  case 2:
                      return hex(s.virtualSize);
                  case 3:
                      return hex(s.rawOffset);
                  case 4:
                      return hex(s.rawSize);
                  case 5:
                      return hex(s.characteristics);
                  default:
                      return QString::number(s.entropy, 'f', 3);
                  }
              });
    const std::vector<int> widths{100, 175, 125, 125, 125, 150, 100};
    for (int column = 0; column < int(widths.size()); ++column)
        table->setColumnWidth(column, widths[column]);
    disconnect(table->selectionModel(), nullptr, this, nullptr);
    connect(table->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this, result](const QModelIndex& index) {
                if (!index.isValid())
                    return;
                const auto& s = result->pe.sections[proxy->mapToSource(index).row()];
                detail->setText(QString("%1   |   VA %2   |   %3 %4 %5   |   Raw range [%6, %7)   |   %8 "
                                        "bytes of virtual zero-fill")
                                    .arg(QString::fromStdString(s.name),
                                         hex(result->pe.imageBase + s.virtualAddress),
                                         s.characteristics & 0x40000000 ? "READ" : "",
                                         s.characteristics & 0x80000000 ? "WRITE" : "",
                                         s.characteristics & 0x20000000 ? "EXECUTE" : "", hex(s.rawOffset),
                                         hex(std::uint64_t(s.rawOffset) + s.rawSize))
                                    .arg(s.virtualSize > s.rawSize ? s.virtualSize - s.rawSize : 0));
            });
}
ImportWidget::ImportWidget()
    : TablePage("Imports", "Imported dependencies and symbols. Ordinal-only imports are preserved; IAT "
                           "locations are relative virtual addresses.") {}
void ImportWidget::setAnalysis(std::shared_ptr<const AnalysisResult> result) {
    setSource({"DLL", "Function", "Ordinal", "IAT RVA"}, int(result->pe.imports.size()),
              [result](int row, int col) -> QVariant {
                  const auto& s = result->pe.imports[row];
                  switch (col) {
                  case 0:
                      return QString::fromStdString(s.dll);
                  case 1:
                      return s.ordinal ? "(by ordinal)" : QString::fromStdString(s.name);
                  case 2:
                      return s.ordinal ? QString::number(*s.ordinal) : "—";
                  default:
                      return hex(s.iatRva);
                  }
              });
    detail->setText(
        result->pe.imports.empty()
            ? "No regular imports found. Check Overview for parser warnings; delay imports are not yet "
              "decoded."
            : QString("%1 imported symbols · filter by DLL or function name").arg(result->pe.imports.size()));
}
ExportWidget::ExportWidget()
    : TablePage("Exports", "Named and ordinal-only exports, including aliases and forwarded symbols.") {}
void ExportWidget::setAnalysis(std::shared_ptr<const AnalysisResult> result) {
    setSource({"Function", "Ordinal", "RVA", "Virtual address", "Forwarder"}, int(result->pe.exports.size()),
              [result](int row, int col) -> QVariant {
                  const auto& s = result->pe.exports[row];
                  switch (col) {
                  case 0:
                      return s.name.empty() ? "(ordinal only)" : QString::fromStdString(s.name);
                  case 1:
                      return s.ordinal;
                  case 2:
                      return hex(s.rva);
                  case 3:
                      return hex(result->pe.imageBase + s.rva);
                  default:
                      return QString::fromStdString(s.forwarder);
                  }
              });
    detail->setText(
        result->pe.exports.empty()
            ? "No exports found. Check Overview for parser warnings."
            : QString("%1 exports · forwarded RVAs point to forwarder strings, not executable code")
                  .arg(result->pe.exports.size()));
}
} // namespace bs::ui
