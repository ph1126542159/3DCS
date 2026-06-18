// Qt dialog for before-analysis model validation results.
#pragma once

#include <QDialog>

#include <vector>

#include "opendva/domain/ModelValidation.h"

namespace opendva::ui {

class ValidationResultsDialog : public QDialog {
public:
    explicit ValidationResultsDialog(const std::vector<ModelIssue>& issues,
                                     QWidget* parent = nullptr);
};

}  // namespace opendva::ui
