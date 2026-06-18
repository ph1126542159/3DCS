#include "BatchProcessorDialog.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace opendva::ui {

BatchProcessorDialog::BatchProcessorDialog(const SimulationSettings& settings,
                                           QWidget* parent)
    : QDialog(parent) {
    setObjectName("batchProcessorDialog");
    setWindowTitle("Batch Processor");
    setMinimumWidth(560);

    const SimulationSettings normalized = normalizeSimulationSettings(settings, 64);

    modelTable_ = new QTableWidget(0, 4, this);
    modelTable_->setObjectName("modelTable");
    modelTable_->horizontalHeader()->setObjectName("modelTableHeader");
    modelTable_->setHorizontalHeaderLabels(
        {"Model File", "Runs", "Seed", "Threads"});
    modelTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    modelTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    modelTable_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    for (int column = 1; column < modelTable_->columnCount(); ++column) {
        modelTable_->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }

    auto* addButton = new QPushButton("Add Models...", this);
    addButton->setObjectName("addModelsButton");
    auto* removeButton = new QPushButton("Remove", this);
    removeButton->setObjectName("removeModelsButton");
    connect(addButton, &QPushButton::clicked,
            this, &BatchProcessorDialog::addModels);
    connect(removeButton, &QPushButton::clicked,
            this, &BatchProcessorDialog::removeSelectedModels);

    auto* modelButtons = new QHBoxLayout;
    modelButtons->addWidget(addButton);
    modelButtons->addWidget(removeButton);
    modelButtons->addStretch();

    outputDirEdit_ = new QLineEdit(this);
    outputDirEdit_->setObjectName("outputDirEdit");
    auto* browseButton = new QPushButton("Browse...", this);
    browseButton->setObjectName("browseOutputDirectoryButton");
    connect(browseButton, &QPushButton::clicked,
            this, &BatchProcessorDialog::browseOutputDirectory);

    auto* outputLayout = new QHBoxLayout;
    outputLayout->addWidget(outputDirEdit_);
    outputLayout->addWidget(browseButton);

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
    form->addRow("Model Files", modelTable_);
    form->addRow(QString(), modelButtons);
    form->addRow("Output Directory", outputLayout);
    form->addRow("Total Runs", totalRunsSpin_);
    form->addRow("Initial Seed", seedEdit_);
    form->addRow("Threads", threadsSpin_);

    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                             this);
    buttons->setObjectName("batchProcessorButtonBox");
    buttons->button(QDialogButtonBox::Ok)->setText("Run");
    buttons->button(QDialogButtonBox::Ok)
        ->setObjectName("batchProcessorRunButton");
    buttons->button(QDialogButtonBox::Cancel)
        ->setObjectName("batchProcessorCancelButton");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

std::vector<std::string> BatchProcessorDialog::modelPaths() const {
    std::vector<std::string> paths;
    paths.reserve(static_cast<std::size_t>(modelTable_->rowCount()));
    for (int row = 0; row < modelTable_->rowCount(); ++row) {
        const QTableWidgetItem* item = modelTable_->item(row, 0);
        paths.push_back(item ? item->text().toStdString() : std::string{});
    }
    return paths;
}

std::vector<BatchAnalysisJob> BatchProcessorDialog::jobs() const {
    std::vector<BatchAnalysisJobInput> inputs;
    inputs.reserve(static_cast<std::size_t>(modelTable_->rowCount()));
    for (int row = 0; row < modelTable_->rowCount(); ++row) {
        const QTableWidgetItem* pathItem = modelTable_->item(row, 0);
        const QTableWidgetItem* runsItem = modelTable_->item(row, 1);
        const QTableWidgetItem* seedItem = modelTable_->item(row, 2);
        const QTableWidgetItem* threadsItem = modelTable_->item(row, 3);

        BatchAnalysisJobInput input;
        input.modelPath = pathItem ? pathItem->text().trimmed().toStdString()
                                   : std::string{};
        input.runs = runsItem ? runsItem->text().toInt() : 0;
        input.seed = seedItem ? seedItem->text().toULongLong() : 0;
        input.threads = threadsItem ? threadsItem->text().toInt() : -1;
        inputs.push_back(input);
    }
    return makeBatchAnalysisJobs(inputs, outputDirectory());
}

std::string BatchProcessorDialog::outputDirectory() const {
    return outputDirEdit_->text().trimmed().toStdString();
}

SimulationSettings BatchProcessorDialog::settings() const {
    SimulationSettings out;
    out.monteCarloEnabled = true;
    out.contributorEnabled = true;
    out.totalRuns = totalRunsSpin_->value();
    out.initialSeed = seedEdit_->text().toULongLong();
    out.threads = threadsSpin_->value();
    return normalizeSimulationSettings(out, 64);
}

void BatchProcessorDialog::accept() {
    if (modelTable_->rowCount() == 0) {
        QMessageBox::warning(this, "Batch Processor",
                             "Add at least one model file.");
        return;
    }
    for (int row = 0; row < modelTable_->rowCount(); ++row) {
        const QTableWidgetItem* pathItem = modelTable_->item(row, 0);
        const QTableWidgetItem* runsItem = modelTable_->item(row, 1);
        const QTableWidgetItem* seedItem = modelTable_->item(row, 2);
        const QTableWidgetItem* threadsItem = modelTable_->item(row, 3);
        bool runsOk = false;
        bool seedOk = false;
        bool threadsOk = false;
        const int runs = runsItem ? runsItem->text().toInt(&runsOk) : 0;
        const auto seed = seedItem ? seedItem->text().toULongLong(&seedOk) : 0;
        const int threads =
            threadsItem ? threadsItem->text().toInt(&threadsOk) : -1;
        if (!pathItem || pathItem->text().trimmed().isEmpty() || !runsOk ||
            runs <= 0 || !seedOk || seed == 0 || !threadsOk || threads < 0) {
            QMessageBox::warning(this, "Batch Processor",
                                 "Each row needs a model file, positive runs, "
                                 "positive seed, and non-negative threads.");
            return;
        }
    }
    if (outputDirEdit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Batch Processor",
                             "Choose an output directory.");
        return;
    }
    QDialog::accept();
}

void BatchProcessorDialog::addModelRow(const QString& path) {
    const int row = modelTable_->rowCount();
    modelTable_->insertRow(row);
    modelTable_->setItem(row, 0, new QTableWidgetItem(path));
    modelTable_->setItem(row, 1,
                         new QTableWidgetItem(QString::number(totalRunsSpin_->value())));
    modelTable_->setItem(row, 2, new QTableWidgetItem(seedEdit_->text()));
    modelTable_->setItem(row, 3,
                         new QTableWidgetItem(QString::number(threadsSpin_->value())));
}

void BatchProcessorDialog::addModels() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, "Add OpenDVA Models", QString(),
        "OpenDVA Model (*.xml *.odva);;All Files (*)");
    for (const QString& path : paths) {
        if (!path.isEmpty()) addModelRow(path);
    }
}

void BatchProcessorDialog::removeSelectedModels() {
    const QModelIndexList selected = modelTable_->selectionModel()->selectedRows();
    std::vector<int> rows;
    rows.reserve(static_cast<std::size_t>(selected.size()));
    for (const QModelIndex& index : selected) {
        rows.push_back(index.row());
    }
    std::sort(rows.rbegin(), rows.rend());
    for (int row : rows) {
        modelTable_->removeRow(row);
    }
}

void BatchProcessorDialog::browseOutputDirectory() {
    const QString path = QFileDialog::getExistingDirectory(
        this, "Choose Batch Output Directory", outputDirEdit_->text());
    if (!path.isEmpty()) outputDirEdit_->setText(path);
}

}  // namespace opendva::ui
