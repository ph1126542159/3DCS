#include "SimulationResultsDialog.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>
#include <utility>

#include "opendva/report/AnalysisDataFiles.h"
#include "opendva/report/CsvReport.h"
#include "opendva/report/HtmlReport.h"
#include "opendva/report/ReportAssets.h"

namespace opendva::ui {
namespace {

QString statText(double value) {
    return QString::number(value, 'g', 7);
}

}  // namespace

SimulationResultsDialog::SimulationResultsDialog(QString modelName,
                                                 std::map<MeasureId, MeasureStats> stats,
                                                 std::map<MeasureId, std::string> names,
                                                 std::vector<SimulationSampleRow> samples,
                                                 std::vector<ContributorRow> contributors,
                                                 std::map<ToleranceId, std::string> toleranceNames,
                                                 QString defaultReportPath,
                                                 SnapshotWriter snapshotWriter,
                                                 QWidget* parent)
    : QDialog(parent),
      modelName_(std::move(modelName)),
      stats_(std::move(stats)),
      names_(std::move(names)),
      samples_(std::move(samples)),
      contributors_(std::move(contributors)),
      toleranceNames_(std::move(toleranceNames)),
      defaultReportPath_(std::move(defaultReportPath)),
      snapshotWriter_(std::move(snapshotWriter)) {
    setObjectName("simulationResultsDialog");
    setWindowTitle("Simulation Window");
    resize(960, 520);

    const QStringList headers{"Measure", "Nominal", "Mean", "Sigma", "6Sigma",
                              "Min",     "Max",     "Cp",   "Cpk",   "Tot OUT %",
                              "DPMO"};
    table_ = new QTableWidget(static_cast<int>(stats_.size()), headers.size(), this);
    table_->setObjectName("summaryTable");
    table_->horizontalHeader()->setObjectName("summaryTableHeader");
    table_->setHorizontalHeaderLabels(headers);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->horizontalHeader()->setStretchLastSection(true);
    populateTable();

    const QStringList histogramHeaders{"Measure", "Bin", "Count", "Share", "Bar"};
    histogramTable_ = new QTableWidget(0, histogramHeaders.size(), this);
    histogramTable_->setObjectName("histogramTable");
    histogramTable_->horizontalHeader()->setObjectName("histogramTableHeader");
    histogramTable_->setHorizontalHeaderLabels(histogramHeaders);
    histogramTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    histogramTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    histogramTable_->horizontalHeader()->setStretchLastSection(true);
    populateHistogramTable();

    samplesTable_ = new QTableWidget(0, 0, this);
    samplesTable_->setObjectName("samplesTable");
    samplesTable_->horizontalHeader()->setObjectName("samplesTableHeader");
    samplesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    samplesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    samplesTable_->horizontalHeader()->setStretchLastSection(true);
    refreshSamplesTableHeaders();
    populateSamplesTable();

    const QStringList contributorHeaders{"Measure", "Contributor", "GeoFactor",
                                         "6Sigma", "Contribution %"};
    contributorTable_ = new QTableWidget(0, contributorHeaders.size(), this);
    contributorTable_->setObjectName("contributorTable");
    contributorTable_->horizontalHeader()->setObjectName("contributorTableHeader");
    contributorTable_->setHorizontalHeaderLabels(contributorHeaders);
    contributorTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    contributorTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    contributorTable_->horizontalHeader()->setStretchLastSection(true);
    populateContributorTable();

    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("resultsTabs");
    tabs->tabBar()->setObjectName("resultsTabBar");
    tabs->addTab(table_, "Summary");
    tabs->addTab(histogramTable_, "Histogram");
    tabs->addTab(samplesTable_, "Samples");
    tabs->addTab(contributorTable_, "Contributor");

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->setObjectName("simulationResultsButtonBox");
    buttons->button(QDialogButtonBox::Close)
        ->setObjectName("simulationResultsCloseButton");
    auto* htmlButton = buttons->addButton("Export HTML", QDialogButtonBox::ActionRole);
    auto* csvButton = buttons->addButton("Export CSV", QDialogButtonBox::ActionRole);
    auto* excelButton = buttons->addButton("Export Excel XML", QDialogButtonBox::ActionRole);
    auto* hstButton = buttons->addButton("Export HST", QDialogButtonBox::ActionRole);
    auto* hlmButton = buttons->addButton("Export HLM", QDialogButtonBox::ActionRole);
    auto* openHstButton = buttons->addButton("Open HST", QDialogButtonBox::ActionRole);
    auto* openHlmButton = buttons->addButton("Open HLM", QDialogButtonBox::ActionRole);
    htmlButton->setObjectName("exportHtmlButton");
    csvButton->setObjectName("exportCsvButton");
    excelButton->setObjectName("exportExcelXmlButton");
    hstButton->setObjectName("exportHstButton");
    hlmButton->setObjectName("exportHlmButton");
    openHstButton->setObjectName("openHstButton");
    openHlmButton->setObjectName("openHlmButton");

    connect(htmlButton, &QAbstractButton::clicked, this, &SimulationResultsDialog::exportHtml);
    connect(csvButton, &QAbstractButton::clicked, this, &SimulationResultsDialog::exportCsv);
    connect(excelButton, &QAbstractButton::clicked, this, &SimulationResultsDialog::exportExcelXml);
    connect(hstButton, &QAbstractButton::clicked, this, &SimulationResultsDialog::exportHst);
    connect(hlmButton, &QAbstractButton::clicked, this, &SimulationResultsDialog::exportHlm);
    connect(openHstButton, &QAbstractButton::clicked, this, &SimulationResultsDialog::importHst);
    connect(openHlmButton, &QAbstractButton::clicked, this, &SimulationResultsDialog::importHlm);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
}

void SimulationResultsDialog::populateTable() {
    int row = 0;
    for (const auto& [id, s] : stats_) {
        const auto nameIt = names_.find(id);
        const QString name =
            nameIt == names_.end() ? QString("M%1").arg(id)
                                   : QString::fromStdString(nameIt->second);

        const QString values[] = {name,
                                  statText(s.nominal),
                                  statText(s.mean),
                                  statText(s.sigma),
                                  statText(s.sixSigma),
                                  statText(s.minVal),
                                  statText(s.maxVal),
                                  statText(s.cp),
                                  statText(s.cpk),
                                  statText(s.totOutPct),
                                  statText(s.dpmo)};
        for (int col = 0; col < static_cast<int>(std::size(values)); ++col) {
            table_->setItem(row, col, new QTableWidgetItem(values[col]));
        }
        ++row;
    }
    table_->resizeColumnsToContents();
}

void SimulationResultsDialog::populateHistogramTable() {
    int totalRows = 0;
    std::uint32_t maxCount = 0;
    for (const auto& [id, s] : stats_) {
        totalRows += static_cast<int>(s.histogram.size());
        for (std::uint32_t count : s.histogram) maxCount = std::max(maxCount, count);
    }

    histogramTable_->setRowCount(totalRows);

    int row = 0;
    for (const auto& [id, s] : stats_) {
        const auto nameIt = names_.find(id);
        const QString name =
            nameIt == names_.end() ? QString("M%1").arg(id)
                                   : QString::fromStdString(nameIt->second);

        std::uint32_t measureTotal = 0;
        for (std::uint32_t count : s.histogram) measureTotal += count;

        for (std::size_t bin = 0; bin < s.histogram.size(); ++bin) {
            const std::uint32_t count = s.histogram[bin];
            const double share = measureTotal == 0
                ? 0.0
                : 100.0 * static_cast<double>(count) / static_cast<double>(measureTotal);

            histogramTable_->setItem(row, 0, new QTableWidgetItem(name));
            histogramTable_->setItem(row, 1,
                                     new QTableWidgetItem(QString::number(bin)));
            histogramTable_->setItem(row, 2,
                                     new QTableWidgetItem(QString::number(count)));
            histogramTable_->setItem(row, 3,
                                     new QTableWidgetItem(QString("%1%")
                                                              .arg(share, 0, 'f', 2)));

            auto* bar = new QProgressBar(histogramTable_);
            bar->setObjectName(QString("histogramBar%1").arg(row));
            bar->setRange(0, static_cast<int>(std::max<std::uint32_t>(maxCount, 1)));
            bar->setValue(static_cast<int>(count));
            bar->setFormat(QString::number(count));
            histogramTable_->setCellWidget(row, 4, bar);
            ++row;
        }
    }

    histogramTable_->resizeColumnsToContents();
}

void SimulationResultsDialog::refreshSamplesTableHeaders(
    const std::vector<MeasureId>& measureOrder) {
    sampleMeasureOrder_ = measureOrder;
    if (sampleMeasureOrder_.empty()) {
        for (const auto& [id, stats] : stats_) {
            (void)stats;
            sampleMeasureOrder_.push_back(id);
        }
    } else {
        for (MeasureId id : sampleMeasureOrder_) {
            if (stats_.find(id) == stats_.end()) {
                MeasureStats placeholder;
                placeholder.nominal = 0.0;
                stats_[id] = placeholder;
            }
        }
    }

    QStringList sampleHeaders{"Build"};
    for (MeasureId id : sampleMeasureOrder_) {
        const auto nameIt = names_.find(id);
        sampleHeaders << (nameIt == names_.end()
            ? QString("M%1").arg(id)
            : QString::fromStdString(nameIt->second));
    }
    samplesTable_->setColumnCount(sampleHeaders.size());
    samplesTable_->setHorizontalHeaderLabels(sampleHeaders);
}

void SimulationResultsDialog::populateSamplesTable() {
    samplesTable_->setRowCount(static_cast<int>(samples_.size()));

    int row = 0;
    for (const SimulationSampleRow& sample : samples_) {
        samplesTable_->setItem(row, 0,
                               new QTableWidgetItem(QString::number(sample.buildIndex)));

        int col = 1;
        for (MeasureId id : sampleMeasureOrder_) {
            const auto valueIt = sample.measureValues.find(id);
            const QString value = valueIt == sample.measureValues.end()
                ? QString()
                : statText(valueIt->second);
            samplesTable_->setItem(row, col, new QTableWidgetItem(value));
            ++col;
        }
        ++row;
    }

    samplesTable_->resizeColumnsToContents();
}

void SimulationResultsDialog::populateContributorTable() {
    contributorTable_->setRowCount(static_cast<int>(contributors_.size()));

    int row = 0;
    for (const ContributorRow& contributor : contributors_) {
        const auto measureIt = names_.find(contributor.measure);
        const QString measureName = measureIt == names_.end()
            ? QString("M%1").arg(contributor.measure)
            : QString::fromStdString(measureIt->second);

        const auto toleranceIt = toleranceNames_.find(contributor.contributor);
        const QString contributorName = toleranceIt == toleranceNames_.end()
            ? QString("T%1").arg(contributor.contributor)
            : QString::fromStdString(toleranceIt->second);

        const QString values[] = {
            measureName,
            contributorName,
            statText(contributor.geoFactor),
            statText(contributor.sixSigma),
            statText(contributor.contributionPct)};
        for (int col = 0; col < static_cast<int>(std::size(values)); ++col) {
            contributorTable_->setItem(row, col, new QTableWidgetItem(values[col]));
        }
        ++row;
    }

    contributorTable_->resizeColumnsToContents();
}

void SimulationResultsDialog::exportHtml() {
    const QString path = QFileDialog::getSaveFileName(this, "Export HTML Report",
                                                     defaultReportPath_,
                                                     "HTML Report (*.html *.htm)");
    if (path.isEmpty()) return;

    const std::string reportPath = path.toStdString();
    std::vector<ReportImage> images;
    if (snapshotWriter_) {
        const ReportImageAsset asset =
            planHtmlSnapshotAsset(reportPath, "Model View",
                                  "Current Qt3D color contour view");
        if (snapshotWriter_(QString::fromStdString(asset.filePath))) {
            images.push_back(asset.image);
        }
    }

    const bool reportWritten = images.empty()
        ? writeHtmlReport(reportPath, modelName_.toStdString(), stats_, names_, {},
                          samples_, contributors_, toleranceNames_)
        : writeHtmlReport(reportPath, modelName_.toStdString(), stats_, names_, images,
                          samples_, contributors_, toleranceNames_);
    if (!reportWritten) {
        QMessageBox::warning(this, "Export failed", "Could not write the HTML report.");
    }
}

void SimulationResultsDialog::exportCsv() {
    const QString path = QFileDialog::getSaveFileName(this, "Export CSV Report", "opendva-report.csv",
                                                     "CSV Report (*.csv)");
    if (path.isEmpty()) return;
    if (!writeCsvReport(path.toStdString(), stats_, names_)) {
        QMessageBox::warning(this, "Export failed", "Could not write the CSV report.");
    }
}

void SimulationResultsDialog::exportExcelXml() {
    const QString path = QFileDialog::getSaveFileName(this, "Export Excel XML Report",
                                                     "opendva-report.xml",
                                                     "Excel XML Report (*.xml)");
    if (path.isEmpty()) return;
    if (!writeExcelXmlReport(path.toStdString(), stats_, names_)) {
        QMessageBox::warning(this, "Export failed", "Could not write the Excel XML report.");
    }
}

void SimulationResultsDialog::exportHst() {
    const QString path = QFileDialog::getSaveFileName(this, "Export HST Samples",
                                                     "opendva-samples.hst",
                                                     "HST Samples (*.hst)");
    if (path.isEmpty()) return;
    if (!writeHstFile(path.toStdString(), samples_, stats_)) {
        QMessageBox::warning(this, "Export failed", "Could not write the HST samples.");
    }
}

void SimulationResultsDialog::exportHlm() {
    const QString path = QFileDialog::getSaveFileName(this, "Export HLM Contributors",
                                                     "opendva-contributors.hlm",
                                                     "HLM Contributors (*.hlm)");
    if (path.isEmpty()) return;
    if (!writeHlmFile(path.toStdString(), contributors_)) {
        QMessageBox::warning(this, "Export failed", "Could not write the HLM contributors.");
    }
}

void SimulationResultsDialog::importHst() {
    const QString path = QFileDialog::getOpenFileName(this, "Open HST Samples",
                                                     QString(),
                                                     "HST Samples (*.hst)");
    if (path.isEmpty()) return;

    std::vector<SimulationSampleRow> loadedSamples;
    std::vector<MeasureId> measureOrder;
    if (!readHstFile(path.toStdString(), loadedSamples, measureOrder)) {
        QMessageBox::warning(this, "Open failed", "Could not read the HST samples.");
        return;
    }

    samples_ = std::move(loadedSamples);
    refreshSamplesTableHeaders(measureOrder);
    populateSamplesTable();
}

void SimulationResultsDialog::importHlm() {
    const QString path = QFileDialog::getOpenFileName(this, "Open HLM Contributors",
                                                     QString(),
                                                     "HLM Contributors (*.hlm)");
    if (path.isEmpty()) return;

    std::vector<ContributorRow> loadedContributors;
    if (!readHlmFile(path.toStdString(), loadedContributors)) {
        QMessageBox::warning(this, "Open failed", "Could not read the HLM contributors.");
        return;
    }

    contributors_ = std::move(loadedContributors);
    populateContributorTable();
}

}  // namespace opendva::ui
