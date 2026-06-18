// Simulation results window for the OpenDVA desktop MVP.
#pragma once

#include <QDialog>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "opendva/ISimulationEngine.h"

class QString;
class QTableWidget;

namespace opendva::ui {

class SimulationResultsDialog final : public QDialog {
    Q_OBJECT

public:
    using SnapshotWriter = std::function<bool(const QString&)>;

    SimulationResultsDialog(QString modelName,
                            std::map<MeasureId, MeasureStats> stats,
                            std::map<MeasureId, std::string> names,
                            std::vector<SimulationSampleRow> samples,
                            std::vector<ContributorRow> contributors = {},
                            std::map<ToleranceId, std::string> toleranceNames = {},
                            QString defaultReportPath = "opendva-report.html",
                            SnapshotWriter snapshotWriter = {},
                            QWidget* parent = nullptr);

private slots:
    void exportHtml();
    void exportCsv();
    void exportExcelXml();
    void exportHst();
    void exportHlm();
    void importHst();
    void importHlm();

private:
    void populateTable();
    void populateHistogramTable();
    void refreshSamplesTableHeaders(const std::vector<MeasureId>& measureOrder = {});
    void populateSamplesTable();
    void populateContributorTable();

    QString modelName_;
    std::map<MeasureId, MeasureStats> stats_;
    std::map<MeasureId, std::string> names_;
    std::vector<SimulationSampleRow> samples_;
    std::vector<MeasureId> sampleMeasureOrder_;
    std::vector<ContributorRow> contributors_;
    std::map<ToleranceId, std::string> toleranceNames_;
    QString defaultReportPath_;
    SnapshotWriter snapshotWriter_;
    QTableWidget* table_{};
    QTableWidget* histogramTable_{};
    QTableWidget* samplesTable_{};
    QTableWidget* contributorTable_{};
};

}  // namespace opendva::ui
