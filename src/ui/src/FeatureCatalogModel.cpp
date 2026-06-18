#include "FeatureCatalogModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace opendva::ui {
namespace {

QString textValue(const QJsonObject& object, const char* key) {
    return object.value(QLatin1String(key)).toString();
}

QString workspaceForNode(const QString& title,
                         const QString& group,
                         const QString& uiSurface) {
    const QString lowerTitle = title.toLower();
    const QString lowerGroup = group.toLower();
    const QString lowerSurface = uiSurface.toLower();

    if (lowerGroup.contains("aao") || lowerTitle.contains("optimizer") ||
        lowerTitle.contains("geofactor") || lowerTitle.contains("cti")) {
        return "aao";
    }
    if (lowerGroup.contains("mechanical") || lowerTitle.contains("dof") ||
        lowerTitle.contains("joint") || lowerTitle.contains("kinematic")) {
        return "mechanical";
    }
    if (lowerGroup.contains("fea") &&
        (lowerTitle.contains("stiff") || lowerTitle.contains("compliant") ||
         lowerTitle.contains("thermal") || lowerTitle.contains("gravity") ||
         lowerTitle.contains("aset"))) {
        return "fea";
    }
    if (lowerGroup.contains("cad") || lowerTitle.contains("catia") ||
        lowerTitle.contains("nx") || lowerTitle.contains("creo") ||
        lowerTitle.contains("solidworks") || lowerTitle.contains("multi-cad")) {
        return "cad";
    }
    if (lowerGroup.contains("moves") || lowerSurface.contains("move") ||
        lowerTitle.contains("move") || lowerTitle.contains("clamp") ||
        lowerTitle.contains("contact")) {
        return "moves";
    }
    if (lowerGroup.contains("tolerance") || lowerGroup.contains("gd&t") ||
        lowerSurface.contains("tolerance") || lowerTitle.contains("datum") ||
        lowerTitle.contains("gdt") || lowerTitle.contains("gd&t")) {
        return "tolerances";
    }
    if (lowerGroup.contains("measure") || lowerSurface.contains("measure") ||
        lowerTitle.contains("measure") || lowerTitle.contains("gap") ||
        lowerTitle.contains("flush")) {
        return "measures";
    }
    if (lowerGroup.contains("simulation") ||
        lowerSurface.contains("analysis") ||
        lowerTitle.contains("analysis") || lowerTitle.contains("samples") ||
        lowerTitle.contains("histogram")) {
        return "simulation";
    }
    if (lowerSurface.contains("report") || lowerTitle.contains("report") ||
        lowerTitle.contains("export")) {
        return "reports";
    }
    if (lowerSurface.contains("system") || lowerTitle.contains("license") ||
        lowerTitle.contains("shared") || lowerTitle.contains("preferences")) {
        return "system";
    }
    if (lowerTitle.contains("help") || lowerGroup.contains("appendix") ||
        lowerTitle.contains("tutorial")) {
        return "help";
    }
    return "modeling";
}

QString commandForNode(const QString& title) {
    const QString lowerTitle = title.toLower();
    if (lowerTitle.contains("tree link")) return "Tree Link";
    if (lowerTitle.contains("update model")) return "Update Model";
    if (lowerTitle.contains("extract")) return "Extract CAD";
    if (lowerTitle.contains("validate")) return "Validate";
    if (lowerTitle.contains("stiffgen")) return "StiffGen";
    if (lowerTitle.contains("tolerance optimizer"))
        return "Tolerance Optimizer";
    if (lowerTitle.contains("sequence optimizer")) return "Sequence Optimizer";
    if (lowerTitle.contains("geofactor")) return "GeoFactor";
    if (lowerTitle.contains("cti")) return "CTI";
    if (lowerTitle.contains("color contour")) return "Color Contour";
    if (lowerTitle.contains("run analysis")) return "Run Analysis";
    if (lowerTitle.contains("dof")) return "DOF Counter";
    return title;
}

FeatureCatalogNode nodeFromJson(const QJsonObject& object) {
    FeatureCatalogNode node;
    node.id = textValue(object, "id");
    node.parentId = textValue(object, "parentId");
    node.level = object.value("level").toInt();
    node.title = textValue(object, "title");
    node.href = textValue(object, "href");
    node.group = textValue(object, "group");
    node.uiSurface = textValue(object, "uiSurface");
    node.status = textValue(object, "status");
    node.summary = textValue(object, "summary");
    node.workspaceId = workspaceForNode(node.title, node.group, node.uiSurface);
    node.command = commandForNode(node.title);
    return node;
}

QVariantMap mapFromNode(const FeatureCatalogNode& node) {
    return {{"id", node.id},
            {"parentId", node.parentId},
            {"level", node.level},
            {"title", node.title},
            {"href", node.href},
            {"group", node.group},
            {"uiSurface", node.uiSurface},
            {"status", node.status},
            {"summary", node.summary},
            {"workspaceId", node.workspaceId},
            {"command", node.command},
            {"routeCommand", node.command}};
}

}  // namespace

FeatureCatalogModel::FeatureCatalogModel(QObject* parent)
    : QAbstractListModel(parent) {
    loadFromResource();
}

int FeatureCatalogModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : nodes_.size();
}

QVariant FeatureCatalogModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= nodes_.size()) {
        return {};
    }

    const FeatureCatalogNode& node = nodes_.at(index.row());
    switch (role) {
        case IdRole:
            return node.id;
        case ParentIdRole:
            return node.parentId;
        case LevelRole:
            return node.level;
        case TitleRole:
            return node.title;
        case HrefRole:
            return node.href;
        case GroupRole:
            return node.group;
        case UiSurfaceRole:
            return node.uiSurface;
        case StatusRole:
            return node.status;
        case SummaryRole:
            return node.summary;
        case WorkspaceIdRole:
            return node.workspaceId;
        case CommandRole:
            return node.command;
        default:
            return {};
    }
}

QHash<int, QByteArray> FeatureCatalogModel::roleNames() const {
    return {{IdRole, "nodeId"},
            {ParentIdRole, "parentId"},
            {LevelRole, "level"},
            {TitleRole, "title"},
            {HrefRole, "href"},
            {GroupRole, "group"},
            {UiSurfaceRole, "uiSurface"},
            {StatusRole, "status"},
            {SummaryRole, "summary"},
            {WorkspaceIdRole, "workspaceId"},
            {CommandRole, "routeCommand"}};
}

int FeatureCatalogModel::nodeCount() const {
    return nodes_.size();
}

const FeatureCatalogNode* FeatureCatalogModel::findById(
    const QString& id) const {
    for (const FeatureCatalogNode& node : nodes_) {
        if (node.id == id) return &node;
    }
    return nullptr;
}

const FeatureCatalogNode* FeatureCatalogModel::findByTitle(
    const QString& title) const {
    for (const FeatureCatalogNode& node : nodes_) {
        if (node.title == title) return &node;
    }
    return nullptr;
}

QVariantMap FeatureCatalogModel::get(int row) const {
    if (row < 0 || row >= nodes_.size()) return {};
    return mapFromNode(nodes_.at(row));
}

int FeatureCatalogModel::firstIndexForGroup(const QString& group) const {
    for (int i = 0; i < nodes_.size(); ++i) {
        if (nodes_.at(i).group == group) return i;
    }
    return -1;
}

void FeatureCatalogModel::loadFromResource() {
    QFile file(":/help/3dcs_help_toc.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonArray nodes =
        document.object().value(QLatin1String("nodes")).toArray();
    nodes_.reserve(nodes.size());
    for (const QJsonValue& value : nodes) {
        if (value.isObject()) nodes_.push_back(nodeFromJson(value.toObject()));
    }
}

}  // namespace opendva::ui
