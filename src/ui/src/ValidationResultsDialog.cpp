// Qt dialog for before-analysis model validation results.
#include "ValidationResultsDialog.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace opendva::ui {
namespace {

QString categoryCountsText(const ValidationSummary& summary) {
    QStringList parts;
    const ModelIssueCategory categories[] = {
        ModelIssueCategory::Model,     ModelIssueCategory::Move,
        ModelIssueCategory::Tolerance, ModelIssueCategory::Measure,
        ModelIssueCategory::Gdt,       ModelIssueCategory::Feature};

    for (ModelIssueCategory category : categories) {
        const auto it = summary.categoryCounts.find(category);
        const int count = it == summary.categoryCounts.end() ? 0 : it->second;
        if (count == 0) continue;
        parts << QString("%1 %2").arg(issueCategoryName(category)).arg(count);
    }
    return parts.isEmpty() ? "No categorized issues." : parts.join(" | ");
}

QBrush severityBrush(ModelIssueSeverity severity) {
    switch (severity) {
        case ModelIssueSeverity::Error: return QBrush(QColor(170, 30, 30));
        case ModelIssueSeverity::Warning: return QBrush(QColor(150, 100, 0));
        case ModelIssueSeverity::Info: return QBrush(QColor(60, 90, 140));
    }
    return QBrush(QColor(60, 60, 60));
}

QTableWidgetItem* readOnlyItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

}  // namespace

ValidationResultsDialog::ValidationResultsDialog(const std::vector<ModelIssue>& issues,
                                                 QWidget* parent)
    : QDialog(parent) {
    setObjectName("validationResultsDialog");
    setWindowTitle("Model Validation");
    resize(860, 520);

    const ValidationSummary summary = summarizeIssues(issues);
    auto* layout = new QVBoxLayout(this);

    auto* summaryLabel = new QLabel(
        QString("Errors %1   Warnings %2   Info %3")
            .arg(summary.errorCount)
            .arg(summary.warningCount)
            .arg(summary.infoCount),
        this);
    summaryLabel->setObjectName("summaryLabel");
    layout->addWidget(summaryLabel);

    auto* categoryLabel = new QLabel(categoryCountsText(summary), this);
    categoryLabel->setObjectName("categoryLabel");
    categoryLabel->setWordWrap(true);
    layout->addWidget(categoryLabel);

    auto* table = new QTableWidget(this);
    table->setObjectName("issueTable");
    table->horizontalHeader()->setObjectName("issueTableHeader");
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels(
        QStringList{"Severity", "Category", "Code", "Message"});
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setRowCount(static_cast<int>(issues.size()));

    for (int row = 0; row < static_cast<int>(issues.size()); ++row) {
        const ModelIssue& issue = issues[static_cast<std::size_t>(row)];
        auto* severityItem =
            readOnlyItem(QString::fromLatin1(issueSeverityName(issue.severity)));
        severityItem->setForeground(severityBrush(issue.severity));
        table->setItem(row, 0, severityItem);
        table->setItem(row, 1,
                       readOnlyItem(QString::fromLatin1(issueCategoryName(issue.category))));
        table->setItem(row, 2, readOnlyItem(QString::fromStdString(issue.code)));
        table->setItem(row, 3, readOnlyItem(QString::fromStdString(issue.message)));
    }

    table->resizeColumnsToContents();
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    layout->addWidget(table, 1);

    if (issues.empty()) {
        auto* okLabel = new QLabel("No validation issues found.", this);
        okLabel->setObjectName("emptyStateLabel");
        layout->insertWidget(2, okLabel);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->setObjectName("validationResultsButtonBox");
    buttons->button(QDialogButtonBox::Close)
        ->setObjectName("validationResultsCloseButton");
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

}  // namespace opendva::ui
