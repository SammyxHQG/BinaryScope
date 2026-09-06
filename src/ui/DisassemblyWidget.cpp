#include "DisassemblyWidget.h"
#include <QFutureWatcher>
#include <QPushButton>
#include <QtConcurrent>
namespace bs::ui {
DisassemblyWidget::DisassemblyWidget()
    : TablePage("Disassembly", "Linear decoding of executable section bytes. Instruction boundaries depend "
                               "on the chosen start address; embedded data may decode as code.") {
    auto* controls = new QHBoxLayout;
    section_ = new QComboBox(this);
    controls->addWidget(section_);
    address_ = new QLineEdit(this);
    address_->setPlaceholderText("Start RVA (hex)");
    controls->addWidget(address_, 1);
    auto* go = new QPushButton("Decode RVA", this);
    auto* entry = new QPushButton("Entry point", this);
    controls->addWidget(go);
    controls->addWidget(entry);
    layout->insertLayout(3, controls);
    connect(entry, &QPushButton::clicked, this, &DisassemblyWidget::entryPoint);
    const auto jump = [this] {
        bool ok = false;
        const auto rva = address_->text().trimmed().toULongLong(&ok, 16);
        if (ok && rva <= 0xffffffffULL)
            decode(std::uint32_t(rva));
        else
            detail->setText("Enter a valid 32-bit RVA in hexadecimal.");
    };
    connect(go, &QPushButton::clicked, this, jump);
    connect(address_, &QLineEdit::returnPressed, this, jump);
    connect(section_, &QComboBox::activated, this,
            [this](int index) { decode(section_->itemData(index).toUInt()); });
    table->setSortingEnabled(false);
}
DisassemblyWidget::~DisassemblyWidget() {
    if (cancel_)
        cancel_->store(true);
}
void DisassemblyWidget::setAnalysis(std::shared_ptr<const AnalysisResult> result) {
    if (cancel_)
        cancel_->store(true);
    ++generation_;
    result_ = std::move(result);
    section_->clear();
    setRows({"Address", "Bytes", "Instruction", "Operands"}, {});
    for (const auto& s : result_->pe.sections)
        if (s.characteristics & 0x20000000)
            section_->addItem(QString::fromStdString(s.name), s.virtualAddress);
    if (result_->pe.entryRva)
        entryPoint();
    else if (section_->count())
        decode(section_->itemData(0).toUInt());
    else
        detail->setText("No executable sections in this image.");
}
void DisassemblyWidget::entryPoint() {
    if (result_ && result_->pe.entryRva)
        decode(result_->pe.entryRva);
    else
        detail->setText("This image does not declare an entry point.");
}
void DisassemblyWidget::decode(std::uint32_t rva) {
    if (!result_)
        return;
    const PESection* selected = nullptr;
    for (const auto& s : result_->pe.sections)
        if ((s.characteristics & 0x20000000) && rva >= s.virtualAddress &&
            std::uint64_t(rva) - s.virtualAddress < s.rawSize) {
            selected = &s;
            break;
        }
    if (!selected) {
        detail->setText("This RVA is not in an executable section backed by file bytes.");
        return;
    }
    address_->setText(hex(rva));
    const auto result = result_;
    const auto delta = rva - selected->virtualAddress;
    const auto offset = std::uint64_t(selected->rawOffset) + delta;
    const auto length = selected->rawSize - delta;
    if (cancel_)
        cancel_->store(true);
    cancel_ = std::make_shared<std::atomic_bool>(false);
    const auto cancel = cancel_;
    const auto generation = ++generation_;
    detail->setText("Decoding in background...");
    auto* watcher = new QFutureWatcher<Disassembly>(this);
    connect(watcher, &QFutureWatcher<Disassembly>::finished, this, [this, watcher, generation] {
        watcher->deleteLater();
        if (generation != generation_)
            return;
        try {
            const auto decoded = std::make_shared<const Disassembly>(watcher->result());
            setSource({"Address", "Bytes", "Instruction", "Operands"}, int(decoded->instructions.size()),
                      [decoded](int row, int col) -> QVariant {
                          const auto& instruction = decoded->instructions[row];
                          switch (col) {
                          case 0:
                              return hex(instruction.address);
                          case 1:
                              return QString::fromLatin1(
                                  QByteArray(reinterpret_cast<const char*>(instruction.bytes.data()),
                                             qsizetype(instruction.bytes.size()))
                                      .toHex(' ')
                                      .toUpper());
                          case 2:
                              return QString::fromStdString(instruction.mnemonic);
                          default:
                              return QString::fromStdString(instruction.operands);
                          }
                      });
            detail->setText(QString::fromStdString(Disassembler::create()->name() + " · " + decoded->note));
        } catch (const std::exception& e) {
            detail->setText(QString::fromUtf8(e.what()));
        }
    });
    watcher->setFuture(QtConcurrent::run([result, rva, offset, length, cancel] {
        const auto machine = result->pe.machine;
        const auto architecture = machine == 0x8664   ? InstructionSet::X64
                                  : machine == 0x14c  ? InstructionSet::X86
                                  : machine == 0xaa64 ? InstructionSet::Arm64
                                                      : InstructionSet::Unsupported;
        return Disassembler::create()->decode(result->file->bytes().subspan(std::size_t(offset), length),
                                              result->pe.imageBase + rva, architecture, 20000, cancel.get());
    }));
}
} // namespace bs::ui
