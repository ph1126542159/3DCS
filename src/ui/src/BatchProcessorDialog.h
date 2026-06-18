// Batch Processor parameter dialog (README chapter 7.7).
#pragma once

#include <QDialog>

#include <string>
#include <vector>

#include "opendva/app/DesktopSmokeWorkflow.h"
#include "opendva/domain/SimulationSettings.h"

class QLineEdit;
class QSpinBox;
class QTableWidget;

namespace opendva::ui {

class BatchProcessorDialog final : public QDialog {
public:
    explicit BatchProcessorDialog(const SimulationSettings& settings,
                                  QWidget* parent = nullptr);

    std::vector<std::string> modelPaths() const;
    std::vector<BatchAnalysisJob> jobs() const;
    std::string outputDirectory() const;
    SimulationSettings settings() const;

protected:
    void accept() override;

private:
    void addModelRow(const QString& path);
    void addModels();
    void removeSelectedModels();
    void browseOutputDirectory();

    QTableWidget* modelTable_{};
    QLineEdit* outputDirEdit_{};
    QSpinBox* totalRunsSpin_{};
    QLineEdit* seedEdit_{};
    QSpinBox* threadsSpin_{};
};

}  // namespace opendva::ui
