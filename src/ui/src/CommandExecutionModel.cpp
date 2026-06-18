#include "CommandExecutionModel.h"

#include <QStringList>

namespace opendva::ui {

CommandExecutionModel::CommandExecutionModel(QObject* parent) : QObject(parent) {
    lastRecord_ = {{"id", "EXEC-0000"},
                   {"status", "Idle"},
                   {"riskLevel", "None"},
                   {"outputCount", 0}};
}

int CommandExecutionModel::executionCount() const {
    return executionCount_;
}

QVariantMap CommandExecutionModel::lastRecord() const {
    return lastRecord_;
}

QVariantMap CommandExecutionModel::execute(const QVariantMap& workspace,
                                           const QVariantMap& commandView,
                                           const QString& commandName) {
    ++executionCount_;

    const QString outputs = commandView.value("outputs").toString();
    const QStringList outputList =
        outputs.isEmpty() ? QStringList{} : outputs.split('|', Qt::SkipEmptyParts);
    const QString executeLabel =
        commandView.value("executeLabel", QStringLiteral("Open")).toString();

    lastRecord_ = {
        {"id", QStringLiteral("EXEC-%1")
                   .arg(executionCount_, 4, 10, QLatin1Char('0'))},
        {"workspace", workspace.value("id").toString()},
        {"workspaceTitle", workspace.value("title").toString()},
        {"command", commandName},
        {"action", executeLabel},
        {"status", commandView.value("state", workspace.value("status")).toString()},
        {"riskLevel", riskLevel(workspace, commandView)},
        {"outputCount", outputList.size()},
        {"outputs", outputList.join(QStringLiteral(", "))},
    };

    emit executionCountChanged();
    emit lastRecordChanged();
    return lastRecord_;
}

QString CommandExecutionModel::riskLevel(const QVariantMap& workspace,
                                         const QVariantMap& commandView) const {
    const QString riskText =
        commandView.value("risk", workspace.value("gaps")).toString().toLower();
    const QString stateText =
        commandView.value("state", workspace.value("status")).toString().toLower();
    const QString status = workspace.value("status").toString();

    if (riskText.contains(QStringLiteral("blocked")) ||
        stateText.contains(QStringLiteral("blocked")) || status == "Missing") {
        return QStringLiteral("High");
    }
    if (riskText.contains(QStringLiteral("shell")) ||
        stateText.contains(QStringLiteral("shell")) || status == "Shell") {
        return QStringLiteral("Medium");
    }
    if (riskText.contains(QStringLiteral("partial")) || status == "Partial") {
        return QStringLiteral("Watch");
    }
    return QStringLiteral("Low");
}

}  // namespace opendva::ui
