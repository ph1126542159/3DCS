#pragma once

#include <QDialog>
#include <QStringList>

namespace opendva::ui {

class ModuleOverviewDialog final : public QDialog {
    Q_OBJECT

public:
    ModuleOverviewDialog(const QString& title,
                         const QString& objectName,
                         const QString& summary,
                         const QStringList& capabilities,
                         QWidget* parent = nullptr);
};

}  // namespace opendva::ui
