#include "PreferencesDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QIntValidator>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>

namespace opendva::ui {

PreferencesDialog::PreferencesDialog(const AppPreferences& preferences,
                                     QWidget* parent)
    : QDialog(parent), basePreferences_(normalizeAppPreferences(preferences, 64)) {
    setObjectName("preferencesDialog");
    setWindowTitle("Preferences");
    setMinimumWidth(420);

    const AppPreferences& normalized = basePreferences_;

    unitCombo_ = new QComboBox(this);
    unitCombo_->setObjectName("lengthUnitCombo");
    unitCombo_->addItems(QStringList{"Millimeter", "Inch"});
    unitCombo_->setCurrentIndex(normalized.lengthUnit == LengthUnit::Inch ? 1 : 0);

    monteCarloCheck_ = new QCheckBox("Monte Carlo Analysis", this);
    monteCarloCheck_->setObjectName("monteCarloCheck");
    monteCarloCheck_->setChecked(normalized.analysisDefaults.monteCarloEnabled);

    contributorCheck_ = new QCheckBox("Contributor Analysis", this);
    contributorCheck_->setObjectName("contributorCheck");
    contributorCheck_->setChecked(normalized.analysisDefaults.contributorEnabled);

    totalRunsSpin_ = new QSpinBox(this);
    totalRunsSpin_->setObjectName("totalRunsSpin");
    totalRunsSpin_->setRange(1, 10000000);
    totalRunsSpin_->setSingleStep(1000);
    totalRunsSpin_->setValue(normalized.analysisDefaults.totalRuns);

    seedEdit_ =
        new QLineEdit(QString::number(normalized.analysisDefaults.initialSeed), this);
    seedEdit_->setObjectName("seedEdit");
    seedEdit_->setValidator(new QIntValidator(1, 2147483647, seedEdit_));

    threadsSpin_ = new QSpinBox(this);
    threadsSpin_->setObjectName("threadsSpin");
    threadsSpin_->setRange(0, 64);
    threadsSpin_->setSpecialValueText("Auto");
    threadsSpin_->setValue(normalized.analysisDefaults.threads);

    reportPathEdit_ =
        new QLineEdit(QString::fromStdString(normalized.defaultReportPath), this);
    reportPathEdit_->setObjectName("reportPathEdit");

    auto* form = new QFormLayout;
    form->addRow("Length Unit", unitCombo_);
    form->addRow("Default Analyses", monteCarloCheck_);
    form->addRow(QString(), contributorCheck_);
    form->addRow("Default Runs", totalRunsSpin_);
    form->addRow("Default Seed", seedEdit_);
    form->addRow("Default Threads", threadsSpin_);
    form->addRow("Default Report Path", reportPathEdit_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         this);
    buttons->setObjectName("preferencesButtonBox");
    buttons->button(QDialogButtonBox::Ok)->setObjectName("preferencesOkButton");
    buttons->button(QDialogButtonBox::Cancel)
        ->setObjectName("preferencesCancelButton");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

AppPreferences PreferencesDialog::preferences() const {
    AppPreferences out = basePreferences_;
    out.lengthUnit = unitCombo_->currentIndex() == 1
        ? LengthUnit::Inch
        : LengthUnit::Millimeter;
    out.analysisDefaults.monteCarloEnabled = monteCarloCheck_->isChecked();
    out.analysisDefaults.contributorEnabled = contributorCheck_->isChecked();
    out.analysisDefaults.totalRuns = totalRunsSpin_->value();
    out.analysisDefaults.initialSeed = seedEdit_->text().toULongLong();
    out.analysisDefaults.threads = threadsSpin_->value();
    out.defaultReportPath = reportPathEdit_->text().trimmed().toStdString();
    return normalizeAppPreferences(out, 64);
}

}  // namespace opendva::ui
