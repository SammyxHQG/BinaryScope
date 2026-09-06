#include "MainWindow.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSettings>
#include <QStatusBar>
namespace bs::ui {
void MainWindow::showSearch() {
    if (!result_) {
        statusBar()->showMessage("Open a binary before searching.");
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("Search BinaryScope");
    dialog.resize(600, 250);
    auto* layout = new QVBoxLayout(&dialog);
    auto* title = new QLabel("Find a symbol, string or location", &dialog);
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    auto* mode = new QComboBox(&dialog);
    mode->addItems({"Imports", "Exports", "Strings", "Virtual address → hex", "RVA → hex",
                    "File offset → hex", "Hex byte sequence", "Text bytes (UTF-8)"});
    layout->addWidget(mode);
    auto* query = new QLineEdit(&dialog);
    query->setPlaceholderText("Name, text, or hexadecimal address / bytes");
    layout->addWidget(query);
    auto* hint = new QLabel("Addresses and offsets use hexadecimal, with an optional 0x prefix.", &dialog);
    hint->setObjectName("muted");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&, this] {
        const auto text = query->text().trimmed();
        if (text.isEmpty()) {
            hint->setText("Enter a search value.");
            return;
        }
        const int kind = mode->currentIndex();
        if (kind <= 2) {
            const int page = kind == 0 ? 3 : kind == 1 ? 4 : 5;
            navigation_->setCurrentRow(page);
            if (kind == 0)
                imports_->filter(text);
            else if (kind == 1)
                exports_->filter(text);
            else
                strings_->filter(text);
        } else if (kind <= 5) {
            bool ok = false;
            auto value = text.toULongLong(&ok, 16);
            if (!ok) {
                hint->setText("Invalid hexadecimal value.");
                return;
            }
            if (kind == 3) {
                if (value < result_->pe.imageBase) {
                    hint->setText("Address is below the image base.");
                    return;
                }
                value -= result_->pe.imageBase;
            }
            if (kind == 3 || kind == 4) {
                const auto offset = result_->pe.rvaToOffset(value);
                if (!offset) {
                    hint->setText("This address has no corresponding file bytes.");
                    return;
                }
                value = *offset;
            }
            if (!hex_->jump(value)) {
                hint->setText("Offset is outside the file.");
                return;
            }
            navigation_->setCurrentRow(6);
        } else {
            navigation_->setCurrentRow(6);
            hex_->searchBytes(text, kind == 7);
        }
        dialog.accept();
    });
    query->setFocus();
    dialog.exec();
}
void MainWindow::showSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle("BinaryScope settings");
    dialog.resize(440, 220);
    auto* layout = new QVBoxLayout(&dialog);
    auto* title = new QLabel("Workspace preferences", &dialog);
    title->setObjectName("pageTitle");
    layout->addWidget(title);
    auto* remember = new QCheckBox("Restore window size and last analysis page", &dialog);
    remember->setChecked(QSettings().value("window/remember", true).toBool());
    layout->addWidget(remember);
    auto* form = new QFormLayout;
    auto* minimum = new QSpinBox(&dialog);
    minimum->setRange(2, 1024);
    minimum->setValue(QSettings().value("strings/minimum", 4).toInt());
    form->addRow("Minimum string length", minimum);
    layout->addLayout(form);
    auto* note =
        new QLabel("BinaryScope reads files as data and never executes them.\nTheme: Scope Dark", &dialog);
    note->setObjectName("muted");
    layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        QSettings().setValue("window/remember", remember->isChecked());
        QSettings().setValue("strings/minimum", minimum->value());
        strings_->setMinimumLength(minimum->value());
    }
}
} // namespace bs::ui
