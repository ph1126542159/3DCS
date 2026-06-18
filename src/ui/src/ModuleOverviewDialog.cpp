#include "ModuleOverviewDialog.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QAbstractItemView>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace opendva::ui {

ModuleOverviewDialog::ModuleOverviewDialog(const QString& title,
                                           const QString& objectName,
                                           const QString& summary,
                                           const QStringList& capabilities,
                                           QWidget* parent)
    : QDialog(parent) {
    setObjectName(objectName);
    setWindowTitle(title);
    resize(640, 420);

    auto* layout = new QVBoxLayout(this);
    auto* summaryLabel = new QLabel(summary, this);
    summaryLabel->setObjectName("moduleOverviewSummaryLabel");
    summaryLabel->setWordWrap(true);
    layout->addWidget(summaryLabel);

    auto* table = new QTableWidget(this);
    table->setObjectName("moduleOverviewTable");
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels(QStringList{"Capability", "Delivery"});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->setRowCount(capabilities.size());
    for (int row = 0; row < capabilities.size(); ++row) {
        auto* capability = new QTableWidgetItem(capabilities[row]);
        auto* delivery = new QTableWidgetItem("Window shell available");
        table->setItem(row, 0, capability);
        table->setItem(row, 1, delivery);
    }
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    layout->addWidget(table);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->setObjectName("moduleOverviewButtonBox");
    if (QPushButton* closeButton = buttons->button(QDialogButtonBox::Close)) {
        closeButton->setObjectName("moduleOverviewCloseButton");
    }
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

}  // namespace opendva::ui
