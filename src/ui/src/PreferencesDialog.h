// Desktop preferences dialog for units, analysis defaults, and report path.
#pragma once

#include <QDialog>

#include "opendva/domain/AppPreferences.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

namespace opendva::ui {

class PreferencesDialog final : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(const AppPreferences& preferences,
                               QWidget* parent = nullptr);

    AppPreferences preferences() const;

private:
    AppPreferences basePreferences_{};
    QComboBox* unitCombo_{};
    QCheckBox* monteCarloCheck_{};
    QCheckBox* contributorCheck_{};
    QSpinBox* totalRunsSpin_{};
    QLineEdit* seedEdit_{};
    QSpinBox* threadsSpin_{};
    QLineEdit* reportPathEdit_{};
};

}  // namespace opendva::ui
