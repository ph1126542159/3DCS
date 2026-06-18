// Run Analysis parameter dialog (README chapter 7).
#pragma once

#include <QDialog>

#include "opendva/domain/SimulationSettings.h"

class QCheckBox;
class QLineEdit;
class QSpinBox;

namespace opendva::ui {

class RunAnalysisDialog : public QDialog {
    Q_OBJECT

public:
    explicit RunAnalysisDialog(const SimulationSettings& settings,
                               QWidget* parent = nullptr);

    SimulationSettings settings() const;

private:
    QCheckBox* monteCarloCheck_{};
    QCheckBox* contributorCheck_{};
    QSpinBox* totalRunsSpin_{};
    QLineEdit* seedEdit_{};
    QSpinBox* threadsSpin_{};
};

}  // namespace opendva::ui
