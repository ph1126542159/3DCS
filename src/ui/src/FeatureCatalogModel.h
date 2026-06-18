#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

namespace opendva::ui {

struct FeatureCatalogNode {
    QString id;
    QString parentId;
    int level = 0;
    QString title;
    QString href;
    QString group;
    QString uiSurface;
    QString status;
    QString summary;
    QString workspaceId;
    QString command;
};

class FeatureCatalogModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int nodeCount READ nodeCount CONSTANT)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        ParentIdRole,
        LevelRole,
        TitleRole,
        HrefRole,
        GroupRole,
        UiSurfaceRole,
        StatusRole,
        SummaryRole,
        WorkspaceIdRole,
        CommandRole
    };

    explicit FeatureCatalogModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int nodeCount() const;
    const FeatureCatalogNode* findById(const QString& id) const;
    const FeatureCatalogNode* findByTitle(const QString& title) const;

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int firstIndexForGroup(const QString& group) const;

private:
    void loadFromResource();

    QVector<FeatureCatalogNode> nodes_;
};

}  // namespace opendva::ui
