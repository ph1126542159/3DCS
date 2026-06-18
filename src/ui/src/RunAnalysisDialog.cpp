#include "RunAnalysisDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace opendva::ui {

RunAnalysisDialog::RunAnalysisDialog(const SimulationSettings& settings, QWidget* parent)
    : QDialog(parent) {
    setObjectName("runAnalysisDialog");
    setWindowTitle("Run Analysis");
    setMinimumWidth(360);

    const SimulationSettings normalized = normalizeSimulationSettings(settings, 64);

    monteCarloCheck_ = new QCheckBox("Monte Carlo Analysis", this);
    monteCarloCheck_->setObjectName("monteCarloCheck");
    monteCarloCheck_->setChecked(normalized.monteCarloEnabled);

    contributorCheck_ = new QCheckBox("Contributor Analysis", this);
    contributorCheck_->setObjectName("contributorCheck");
    contributorCheck_->setChecked(normalized.contributorEnabled);

    totalRunsSpin_ = new QSpinBox(this);
    totalRunsSpin_->setObjectName("totalRunsSpin");
    totalRunsSpin_->setRange(1, 10000000);
    totalRunsSpin_->setSingleStep(1000);
    totalRunsSpin_->setValue(normalized.totalRuns);

    seedEdit_ = new QLineEdit(QString::number(normalized.initialSeed), this);
    seedEdit_->setObjectName("seedEdit");
    seedEdit_->setValidator(new QIntValidator(1, 2147483647, seedEdit_));

    threadsSpin_ = new QSpinBox(this);
    threadsSpin_->setObjectName("threadsSpin");
    threadsSpin_->setRange(0, 64);
    threadsSpin_->setSpecialValueText("Auto");
    threadsSpin_->setValue(normalized.threads);

    auto* form = new QFormLayout;
    form->addRow(monteCarloCheck_);
    form->addRow(contributorCheck_);
    form->addRow("Total Runs", totalRunsSpin_);
    form->addRow("Initial Seed", seedEdit_);
    form->addRow("Threads", threadsSpin_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         this);
    buttons->setObjectName("runAnalysisButtonBox");
    buttons->button(QDialogButtonBox::Ok)->setObjectName("runAnalysisOkButton");
    buttons->button(QDialogButtonBox::Cancel)
        ->setObjectName("runAnalysisCancelButton");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

SimulationSettings RunAnalysisDialog::settings() const {
    SimulationSettings out;
    out.monteCarloEnabled = monteCarloCheck_->isChecked();
    out.contributorEnabled = contributorCheck_->isChecked();
    out.totalRuns = totalRunsSpin_->value();
    out.initialSeed = seedEdit_->text().toULongLong();
    out.threads = threadsSpin_->value();
    return normalizeSimulationSettings(out, 64);
}

}  // namespace opendva::ui
