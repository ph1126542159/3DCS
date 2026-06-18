#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace opendva::ui {

struct WorkbenchWorkspace {
    QString id;
    QString title;
    QString icon;
    QString status;
    QString surface;
    QString summary;
    QString commands;
    QString gaps;
    QString workflow;
    QString primaryPanel;
    QString matrixRows;
    QString editorFields;
    QString metrics;
};

class WorkbenchModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int workspaceCount READ workspaceCount CONSTANT)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        IconRole,
        StatusRole,
        SurfaceRole,
        SummaryRole,
        CommandsRole,
        GapsRole,
        WorkflowRole,
        PrimaryPanelRole,
        MatrixRowsRole,
        EditorFieldsRole,
        MetricsRole
    };

    explicit WorkbenchModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int workspaceCount() const;
    const WorkbenchWorkspace* findById(const QString& id) const;

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int indexOf(const QString& id) const;
    Q_INVOKABLE QVariantMap commandDetail(int row, const QString& command) const;

private:
    QVector<WorkbenchWorkspace> workspaces_;
};

}  // namespace opendva::ui
