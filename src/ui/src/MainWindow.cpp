// A8 Qt UI main window implementation (README §3.1 / §7.6 / §8).
#include "MainWindow.h"

#include <QAction>
#include <QAbstractItemView>
#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDropEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QIcon>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QDir>
#include <QPushButton>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "BatchProcessorDialog.h"
#include "ModelViewport.h"
#include "ModuleOverviewDialog.h"
#include "PreferencesDialog.h"
#include "RunAnalysisDialog.h"
#include "SimulationResultsDialog.h"
#include "ValidationResultsDialog.h"
#include "opendva/domain/ModelEditing.h"
#include "opendva/domain/ModelSerializer.h"
#include "opendva/domain/ModelValidation.h"
#include "opendva/app/DesktopSmokeWorkflow.h"
#include "opendva/sim/SimulationEngine.h"

namespace opendva::ui {
namespace {

constexpr int kNodeKindRole = Qt::UserRole;
constexpr int kNodeIdRole = Qt::UserRole + 1;
constexpr int kPropertyKeyRole = Qt::UserRole + 2;
constexpr int kNodeNameRole = Qt::UserRole + 3;

QString visibleActionText(const QString& text) {
    QString visible;
    for (int index = 0; index < text.size(); ++index) {
        if (text[index] == QLatin1Char('&')) {
            if (index + 1 < text.size() &&
                text[index + 1] == QLatin1Char('&')) {
                visible.append(QLatin1Char('&'));
                ++index;
            }
            continue;
        }
        visible.append(text[index]);
    }
    return visible;
}

QString openDvaTech3DStyleSheet() {
    return R"qss(
/* OpenDVA Tech3D */
QMainWindow {
    background: #0f1418;
    color: #d8e7ef;
}
QMenuBar#mainMenuBar {
    background: #111921;
    color: #d8e7ef;
    border-bottom: 1px solid #2b4658;
    padding: 2px 8px;
}
QMenuBar#mainMenuBar::item {
    background: transparent;
    padding: 6px 10px;
}
QMenuBar#mainMenuBar::item:selected {
    background: #193244;
    color: #ffffff;
}
QMenu {
    background: #121a21;
    color: #d8e7ef;
    border: 1px solid #315267;
    padding: 6px;
}
QMenu::item {
    padding: 7px 28px 7px 26px;
}
QMenu::item:selected {
    background: #1b3b4d;
    color: #ffffff;
}
QToolBar#mainToolBar {
    background: #121a21;
    border: 1px solid #2b4658;
    border-left: none;
    border-right: none;
    spacing: 5px;
    padding: 6px;
}
QToolBar#mainToolBar QToolButton {
    background: #17242c;
    color: #d8e7ef;
    border: 1px solid #2f5265;
    border-radius: 5px;
    padding: 5px;
    min-width: 48px;
}
QToolBar#mainToolBar QToolButton:hover {
    background: #1e3a48;
    border-color: #39b8ff;
}
QToolBar#mainToolBar QToolButton:pressed {
    background: #0d2530;
    border-color: #ffb84a;
}
QDockWidget {
    color: #d8e7ef;
    titlebar-close-icon: none;
    titlebar-normal-icon: none;
}
QDockWidget::title {
    background: #17242c;
    border: 1px solid #2b4658;
    padding: 6px 8px;
    text-align: left;
}
QTreeWidget, QTableWidget, QTableView {
    background: #10171d;
    alternate-background-color: #14202a;
    color: #d8e7ef;
    border: 1px solid #284252;
    gridline-color: #263946;
    selection-background-color: #1e5a74;
    selection-color: #ffffff;
}
QHeaderView::section {
    background: #17242c;
    color: #d8e7ef;
    border: 1px solid #2b4658;
    padding: 5px;
}
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background: #0d141a;
    color: #eaf7ff;
    border: 1px solid #315267;
    border-radius: 4px;
    padding: 4px 6px;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border-color: #39b8ff;
}
QPushButton {
    background: #183141;
    color: #eaf7ff;
    border: 1px solid #3b6b84;
    border-radius: 5px;
    padding: 6px 12px;
}
QPushButton:hover {
    background: #21485c;
    border-color: #39b8ff;
}
QPushButton:pressed {
    background: #102732;
    border-color: #ffb84a;
}
QStatusBar#mainStatusBar {
    background: #0d141a;
    color: #a9c7d4;
    border-top: 1px solid #2b4658;
}
QTabWidget::pane {
    border: 1px solid #284252;
    background: #10171d;
}
QTabBar::tab {
    background: #17242c;
    color: #c7d9e2;
    border: 1px solid #2b4658;
    padding: 6px 10px;
}
QTabBar::tab:selected {
    background: #1e3a48;
    color: #ffffff;
    border-color: #39b8ff;
}
)qss";
}

void applyIcon(QAction* action, const char* iconName) {
    if (action == nullptr || iconName == nullptr) return;
    action->setIcon(QIcon(QString(":/icons/%1.png").arg(iconName)));
}

enum class PropertyKey {
    None = 0,
    AssemblyName,
    PartDcsName,
    PointActive,
    PointKind,
    PointHoleType,
    PointX,
    PointY,
    PointZ,
    PointDiameter,
    PointI,
    PointJ,
    PointK,
    FeatureKind,
    FeatureDefiningPoints,
    ToleranceName,
    ToleranceActive,
    ToleranceDistribution,
    ToleranceRange,
    ToleranceOffset,
    ToleranceSigmaNumber,
    ToleranceRandomVariables,
    ToleranceGeomRule,
    ToleranceRangeScale,
    ToleranceI,
    ToleranceJ,
    ToleranceK,
    ToleranceTruncationActive,
    ToleranceMinTruncation,
    ToleranceMaxTruncation,
    ToleranceFeatures,
    GdtName,
    GdtType,
    GdtActive,
    GdtRange,
    GdtDiametrical,
    GdtDrfPrimary,
    GdtDrfSecondary,
    GdtDrfTertiary,
    GdtFeatures,
    MoveName,
    MoveType,
    MoveUserDllRoutine,
    MoveActive,
    MoveNominalBuild,
    MovePairs,
    MoveObjectX,
    MoveObjectY,
    MoveObjectZ,
    MoveTargetX,
    MoveTargetY,
    MoveTargetZ,
    MoveParts,
    MoveSearchAccuracy,
    MoveMaxIterations,
    MoveFloatActive,
    MoveFloatSigmaNumber,
    MoveFloatRangeScale,
    MoveFloatAngleRange,
    MoveFloatAngleOffset,
    MoveDirectionI,
    MoveDirectionJ,
    MoveDirectionK,
    MoveTranslationX,
    MoveTranslationY,
    MoveTranslationZ,
    MeasureName,
    MeasureType,
    MeasureActive,
    MeasureAsOutput,
    MeasureInputPoints,
    MeasureInputFeatures,
    MeasureSpecMode,
    MeasureDirectionMode,
    MeasureI,
    MeasureJ,
    MeasureK,
    MeasureScale,
    MeasureEquation,
    MeasureValues,
    MeasureLslActive,
    MeasureUslActive,
    MeasureLsl,
    MeasureUsl,
    VariantName,
    VariantActive,
    VariantMoves,
    VariantTolerances,
    VariantMeasures
};

void tagItem(QTreeWidgetItem* item, NavigatorNodeKind kind, std::uint64_t id = 0) {
    item->setData(0, kNodeKindRole, static_cast<int>(kind));
    item->setData(0, kNodeIdRole, QVariant::fromValue<qulonglong>(id));
}

void tagVariantItem(QTreeWidgetItem* item, const std::string& name) {
    tagItem(item, NavigatorNodeKind::Variant);
    item->setData(0, kNodeNameRole, QString::fromStdString(name));
}

QTreeWidgetItem* addCategory(QTreeWidgetItem* parent, const QString& label) {
    auto* item = new QTreeWidgetItem(parent, QStringList{label});
    tagItem(item, NavigatorNodeKind::Category);
    item->setExpanded(true);
    return item;
}

QTreeWidgetItem* childCategory(QTreeWidgetItem* parent, const QString& label) {
    if (parent == nullptr) return nullptr;
    for (int index = 0; index < parent->childCount(); ++index) {
        QTreeWidgetItem* child = parent->child(index);
        if (child != nullptr && child->text(0) == label) return child;
    }
    return nullptr;
}

QTreeWidgetItem* findNavigatorItem(QTreeWidgetItem* root,
                                   NavigatorNodeKind kind,
                                   std::uint64_t id,
                                   const QString& name) {
    if (root == nullptr) return nullptr;
    const auto itemKind =
        static_cast<NavigatorNodeKind>(root->data(0, kNodeKindRole).toInt());
    const std::uint64_t itemId = root->data(0, kNodeIdRole).toULongLong();
    const QString itemName = root->data(0, kNodeNameRole).toString();
    if (itemKind == kind && itemId == id &&
        (kind != NavigatorNodeKind::Variant || itemName == name)) {
        return root;
    }
    for (int i = 0; i < root->childCount(); ++i) {
        if (QTreeWidgetItem* found =
                findNavigatorItem(root->child(i), kind, id, name)) {
            return found;
        }
    }
    return nullptr;
}

bool isMoveItem(QTreeWidgetItem* item) {
    return item != nullptr &&
           static_cast<NavigatorNodeKind>(item->data(0, kNodeKindRole).toInt()) ==
               NavigatorNodeKind::Move;
}

bool isMovesCategory(QTreeWidgetItem* item) {
    return item != nullptr &&
           static_cast<NavigatorNodeKind>(item->data(0, kNodeKindRole).toInt()) ==
               NavigatorNodeKind::Category &&
           item->text(0) == "Moves";
}

void restoreNavigatorSelection(QTreeWidget* navigator,
                               NavigatorNodeKind kind,
                               std::uint64_t id,
                               const QString& name) {
    if (navigator == nullptr || navigator->topLevelItemCount() == 0) return;
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    if (QTreeWidgetItem* item = findNavigatorItem(root, kind, id, name)) {
        navigator->setCurrentItem(item);
    }
}

class NavigatorTree final : public QTreeWidget {
public:
    explicit NavigatorTree(QWidget* parent = nullptr) : QTreeWidget(parent) {}
    std::function<void(MoveId)> moveDropped;

protected:
    void dropEvent(QDropEvent* event) override {
        QTreeWidgetItem* dragged =
            selectedItems().isEmpty() ? nullptr : selectedItems().front();
        if (!isMoveItem(dragged) || !isMovesCategory(dragged->parent())) {
            event->ignore();
            return;
        }

        const MoveId movedId = dragged->data(0, kNodeIdRole).toULongLong();
        QTreeWidget::dropEvent(event);
        if (moveDropped) {
            moveDropped(movedId);
        }
    }
};

QString vecText(const Vec3& v) {
    return QString("(%1, %2, %3)")
        .arg(v.x, 0, 'g', 6)
        .arg(v.y, 0, 'g', 6)
        .arg(v.z, 0, 'g', 6);
}

void preparePropertyTable(QTableWidget* table) {
    table->clearContents();
    table->setRowCount(0);
}

void addPropertyRow(QTableWidget* table, const QString& field, const QString& value,
                    PropertyKey key = PropertyKey::None, bool editable = false) {
    const int row = table->rowCount();
    table->insertRow(row);

    auto* fieldItem = new QTableWidgetItem(field);
    fieldItem->setFlags(fieldItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, 0, fieldItem);

    auto* valueItem = new QTableWidgetItem(value);
    valueItem->setData(kPropertyKeyRole, static_cast<int>(key));
    if (!editable) valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, 1, valueItem);
}

void addCheckPropertyRow(QTableWidget* table, const QString& field, bool checked,
                         PropertyKey key) {
    const int row = table->rowCount();
    table->insertRow(row);

    auto* fieldItem = new QTableWidgetItem(field);
    fieldItem->setFlags(fieldItem->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, 0, fieldItem);

    auto* valueItem = new QTableWidgetItem;
    valueItem->setData(kPropertyKeyRole, static_cast<int>(key));
    valueItem->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    valueItem->setFlags((valueItem->flags() | Qt::ItemIsUserCheckable) &
                        ~Qt::ItemIsEditable);
    table->setItem(row, 1, valueItem);
}

QString numberText(double value) {
    return QString::number(value, 'g', 12);
}

QString valuesText(const std::vector<double>& values) {
    QStringList parts;
    for (const double value : values) {
        parts << numberText(value);
    }
    return parts.join(", ");
}

std::optional<std::vector<double>> parseValues(const QString& text) {
    std::vector<double> values;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return values;

    const QStringList parts = trimmed.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        bool ok = false;
        const double value = part.trimmed().toDouble(&ok);
        if (!ok) return std::nullopt;
        values.push_back(value);
    }
    return values;
}

QString vec3Text(const Vec3& vec) {
    return QString("%1, %2, %3")
        .arg(numberText(vec.x), numberText(vec.y), numberText(vec.z));
}

std::optional<Vec3> parseVec3(const QString& text) {
    const std::optional<std::vector<double>> parsed = parseValues(text);
    if (!parsed.has_value() || parsed->size() != 3) return std::nullopt;
    return Vec3{parsed->at(0), parsed->at(1), parsed->at(2)};
}

QString movePairsText(const std::vector<MovePair>& pairs) {
    QStringList specs;
    for (const MovePair& pair : pairs) {
        specs << QString("%1 -> %2 -> %3")
                     .arg(vec3Text(pair.objectPoint),
                          vec3Text(pair.targetPoint),
                          vec3Text(pair.direction.ijk));
    }
    return specs.join("; ");
}

std::optional<std::vector<MovePair>> parseMovePairs(const QString& text) {
    std::vector<MovePair> pairs;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return pairs;

    const QStringList specs = trimmed.split(';', Qt::SkipEmptyParts);
    for (const QString& spec : specs) {
        const QStringList fields = spec.split("->", Qt::KeepEmptyParts);
        if (fields.size() != 3) return std::nullopt;

        const std::optional<Vec3> objectPoint = parseVec3(fields[0]);
        const std::optional<Vec3> targetPoint = parseVec3(fields[1]);
        const std::optional<Vec3> direction = parseVec3(fields[2]);
        if (!objectPoint.has_value() || !targetPoint.has_value() ||
            !direction.has_value()) {
            return std::nullopt;
        }

        MovePair pair;
        pair.objectPoint = objectPoint.value();
        pair.targetPoint = targetPoint.value();
        pair.direction.type = DirectionType::TypeIn;
        pair.direction.ijk = direction.value();
        pairs.push_back(pair);
    }
    return pairs;
}

std::optional<FeatureId> parseFeatureId(const QString& text) {
    bool ok = false;
    const qulonglong value = text.trimmed().toULongLong(&ok);
    if (!ok) return std::nullopt;
    return static_cast<FeatureId>(value);
}

std::optional<PointId> parsePointId(const QString& text) {
    bool ok = false;
    const qulonglong value = text.trimmed().toULongLong(&ok);
    if (!ok) return std::nullopt;
    return static_cast<PointId>(value);
}

std::optional<PartId> parsePartId(const QString& text) {
    bool ok = false;
    const qulonglong value = text.trimmed().toULongLong(&ok);
    if (!ok) return std::nullopt;
    return static_cast<PartId>(value);
}

QString partIdsText(const std::vector<PartId>& ids) {
    QStringList parts;
    for (const PartId id : ids) {
        parts << QString::number(id);
    }
    return parts.join(", ");
}

std::optional<std::vector<PartId>> parsePartIds(const QString& text) {
    std::vector<PartId> ids;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return ids;

    const QStringList parts = trimmed.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const std::optional<PartId> parsed = parsePartId(part);
        if (!parsed.has_value()) return std::nullopt;
        ids.push_back(parsed.value());
    }
    return ids;
}

std::optional<MoveId> parseMoveId(const QString& text) {
    bool ok = false;
    const qulonglong value = text.trimmed().toULongLong(&ok);
    if (!ok) return std::nullopt;
    return static_cast<MoveId>(value);
}

QString moveIdsText(const std::vector<MoveId>& ids) {
    QStringList parts;
    for (const MoveId id : ids) parts << QString::number(id);
    return parts.join(", ");
}

std::optional<std::vector<MoveId>> parseMoveIds(const QString& text) {
    std::vector<MoveId> ids;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return ids;

    const QStringList parts = trimmed.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const std::optional<MoveId> parsed = parseMoveId(part);
        if (!parsed.has_value()) return std::nullopt;
        ids.push_back(parsed.value());
    }
    return ids;
}

QString pointIdsText(const std::vector<PointId>& ids) {
    QStringList parts;
    for (const PointId id : ids) {
        parts << QString::number(id);
    }
    return parts.join(", ");
}

std::optional<std::vector<PointId>> parsePointIds(const QString& text) {
    std::vector<PointId> ids;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return ids;

    const QStringList parts = trimmed.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const std::optional<PointId> parsed = parsePointId(part);
        if (!parsed.has_value()) return std::nullopt;
        ids.push_back(parsed.value());
    }
    return ids;
}

QString batchSummaryText(const BatchAnalysisResult& result,
                         const QString& label = "Batch completed") {
    return QString("%1: %2 succeeded, %3 failed")
        .arg(label)
        .arg(result.succeeded)
        .arg(result.failed);
}

QString batchDetailsText(const BatchAnalysisResult& result) {
    QString details;
    for (const BatchAnalysisItemResult& item : result.items) {
        details += QString("%1: %2\n")
                       .arg(QString::fromStdString(item.modelPath),
                            item.ok ? "OK"
                                    : QString::fromStdString(item.message));
    }
    return details;
}

std::optional<ToleranceId> parseToleranceId(const QString& text) {
    bool ok = false;
    const qulonglong value = text.trimmed().toULongLong(&ok);
    if (!ok) return std::nullopt;
    return static_cast<ToleranceId>(value);
}

QString toleranceIdsText(const std::vector<ToleranceId>& ids) {
    QStringList parts;
    for (const ToleranceId id : ids) parts << QString::number(id);
    return parts.join(", ");
}

std::optional<std::vector<ToleranceId>> parseToleranceIds(const QString& text) {
    std::vector<ToleranceId> ids;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return ids;

    const QStringList parts = trimmed.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const std::optional<ToleranceId> parsed = parseToleranceId(part);
        if (!parsed.has_value()) return std::nullopt;
        ids.push_back(parsed.value());
    }
    return ids;
}

QString featureIdsText(const std::vector<FeatureId>& ids) {
    QStringList parts;
    for (const FeatureId id : ids) {
        parts << QString::number(id);
    }
    return parts.join(", ");
}

std::optional<std::vector<FeatureId>> parseFeatureIds(const QString& text) {
    std::vector<FeatureId> ids;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return ids;

    const QStringList parts = trimmed.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const std::optional<FeatureId> parsed = parseFeatureId(part);
        if (!parsed.has_value()) return std::nullopt;
        ids.push_back(parsed.value());
    }
    return ids;
}

std::optional<MeasureId> parseMeasureId(const QString& text) {
    bool ok = false;
    const qulonglong value = text.trimmed().toULongLong(&ok);
    if (!ok) return std::nullopt;
    return static_cast<MeasureId>(value);
}

QString measureIdsText(const std::vector<MeasureId>& ids) {
    QStringList parts;
    for (const MeasureId id : ids) parts << QString::number(id);
    return parts.join(", ");
}

std::optional<std::vector<MeasureId>> parseMeasureIds(const QString& text) {
    std::vector<MeasureId> ids;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return ids;

    const QStringList parts = trimmed.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const std::optional<MeasureId> parsed = parseMeasureId(part);
        if (!parsed.has_value()) return std::nullopt;
        ids.push_back(parsed.value());
    }
    return ids;
}

QString validationStatusText(const std::vector<ModelIssue>& issues) {
    const ValidationSummary summary = summarizeIssues(issues);
    return QString("Validation: %1 error(s), %2 warning(s), %3 info")
        .arg(summary.errorCount)
        .arg(summary.warningCount)
        .arg(summary.infoCount);
}

QStringList variantNames(const Model& model) {
    QStringList names;
    for (const ModelVariant& variant : model.variants) {
        names << QString::fromStdString(variant.name);
    }
    return names;
}

QStringList partChoiceNames(const Model& model) {
    QStringList names;
    for (const Part& part : model.parts) {
        names << QString("%1 - %2")
                     .arg(part.id)
                     .arg(QString::fromStdString(part.dcsName));
    }
    return names;
}

PartId partIdAt(const Model& model, int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= model.parts.size()) {
        return kInvalidId;
    }
    return model.parts[static_cast<std::size_t>(index)].id;
}

int partIndexOf(const Model& model, PartId id) {
    for (std::size_t index = 0; index < model.parts.size(); ++index) {
        if (model.parts[index].id == id) return static_cast<int>(index);
    }
    return 0;
}

int firstOtherPartIndex(const Model& model, PartId id) {
    for (std::size_t index = 0; index < model.parts.size(); ++index) {
        if (model.parts[index].id != id) return static_cast<int>(index);
    }
    return 0;
}

void appendPointIds(const Model& model, PartId partId,
                    std::vector<PointId>& points) {
    const Part* part = model.findPart(partId);
    if (part == nullptr) return;

    for (const Point& point : part->points) {
        points.push_back(point.id);
    }
}

bool containsPointId(const std::vector<PointId>& points, PointId id) {
    for (const PointId point : points) {
        if (point == id) return true;
    }
    return false;
}

std::vector<PointId> orderedMeasurePoints(const Model& model,
                                          PartId selectedPart,
                                          PointId selectedPoint) {
    std::vector<PointId> points;
    if (selectedPoint != kInvalidId) {
        points.push_back(selectedPoint);
    }

    appendPointIds(model, selectedPart, points);
    for (const Part& part : model.parts) {
        for (const Point& point : part.points) {
            points.push_back(point.id);
        }
    }

    std::vector<PointId> unique;
    for (const PointId point : points) {
        if (point != kInvalidId && !containsPointId(unique, point)) {
            unique.push_back(point);
        }
    }
    return unique;
}

QString defaultVariantName(const Model& model) {
    return QString("Scenario_%1").arg(model.variants.size() + 1);
}

QString activeVariantText(const Model& model) {
    const std::optional<std::string> active = model.activeVariantName();
    return active.has_value() ? QString::fromStdString(active.value()) : QString();
}

QString pointKindName(PointKind kind) {
    switch (kind) {
        case PointKind::Coordinate: return "Coordinate";
        case PointKind::Feature: return "Feature";
        case PointKind::Dynamic: return "Dynamic";
    }
    return "Coordinate";
}

QStringList pointKindNames() {
    return QStringList{"Coordinate", "Feature", "Dynamic"};
}

PointKind pointKindAt(int index) {
    if (index < static_cast<int>(PointKind::Coordinate) ||
        index > static_cast<int>(PointKind::Dynamic)) {
        return PointKind::Coordinate;
    }
    return static_cast<PointKind>(index);
}

std::optional<PointKind> parsePointKind(const QString& text) {
    const QString normalized = text.trimmed();
    const QString compact = normalized.toLower().remove(' ');
    const QStringList names = pointKindNames();
    for (int i = 0; i < names.size(); ++i) {
        const QString candidate = names[i].toLower().remove(' ');
        if (normalized.compare(names[i], Qt::CaseInsensitive) == 0 ||
            compact == candidate) {
            return pointKindAt(i);
        }
    }
    return std::nullopt;
}

QString holeTypeName(HoleType type) {
    switch (type) {
        case HoleType::None: return "None";
        case HoleType::Hole: return "Hole";
        case HoleType::Pin: return "Pin";
    }
    return "None";
}

QStringList holeTypeNames() {
    return QStringList{"None", "Hole", "Pin"};
}

HoleType holeTypeAt(int index) {
    if (index < static_cast<int>(HoleType::None) ||
        index > static_cast<int>(HoleType::Pin)) {
        return HoleType::None;
    }
    return static_cast<HoleType>(index);
}

std::optional<HoleType> parseHoleType(const QString& text) {
    const QString normalized = text.trimmed();
    const QString compact = normalized.toLower().remove(' ');
    const QStringList names = holeTypeNames();
    for (int i = 0; i < names.size(); ++i) {
        const QString candidate = names[i].toLower().remove(' ');
        if (normalized.compare(names[i], Qt::CaseInsensitive) == 0 ||
            compact == candidate) {
            return holeTypeAt(i);
        }
    }
    return std::nullopt;
}

QString featureKindName(FeatureKind kind) {
    switch (kind) {
        case FeatureKind::Plane: return "Plane";
        case FeatureKind::Cylinder: return "Cylinder";
        case FeatureKind::Cone: return "Cone";
        case FeatureKind::Sphere: return "Sphere";
        case FeatureKind::Edge: return "Edge";
        case FeatureKind::SlotTab: return "Slot/Tab";
        case FeatureKind::PointBased: return "Point Based";
        case FeatureKind::Combined: return "Combined";
    }
    return "Plane";
}

QStringList featureKindNames() {
    return QStringList{"Plane", "Cylinder", "Cone", "Sphere",
                       "Edge", "Slot/Tab", "Point Based", "Combined"};
}

FeatureKind featureKindAt(int index) {
    if (index < static_cast<int>(FeatureKind::Plane) ||
        index > static_cast<int>(FeatureKind::Combined)) {
        return FeatureKind::Plane;
    }
    return static_cast<FeatureKind>(index);
}

std::optional<FeatureKind> parseFeatureKind(const QString& text) {
    const QString normalized = text.trimmed();
    const QString compact = normalized.toLower().remove(' ').remove('/');
    const QStringList names = featureKindNames();
    for (int i = 0; i < names.size(); ++i) {
        const QString candidate = names[i].toLower().remove(' ').remove('/');
        if (normalized.compare(names[i], Qt::CaseInsensitive) == 0 ||
            compact == candidate) {
            return featureKindAt(i);
        }
    }
    return std::nullopt;
}

QString distributionName(DistributionType type) {
    switch (type) {
        case DistributionType::Normal: return "Normal";
        case DistributionType::Uniform: return "Uniform";
        case DistributionType::Triangular: return "Triangular";
        case DistributionType::BiMode: return "BiMode";
        case DistributionType::RightSkew: return "Right Skew";
        case DistributionType::LeftSkew: return "Left Skew";
        case DistributionType::OpenUp: return "Open Up";
        case DistributionType::OpenDown: return "Open Down";
        case DistributionType::UserDefined: return "User Defined";
        case DistributionType::Step: return "Step";
        case DistributionType::Constant: return "Constant";
        case DistributionType::Weibull4: return "Weibull 4";
        case DistributionType::Pearson4: return "Pearson 4";
        case DistributionType::Modal: return "Modal";
        case DistributionType::Trapezoid: return "Trapezoid";
        case DistributionType::PowerFunction: return "Power Function";
        case DistributionType::Normal2D: return "Normal 2D";
        case DistributionType::Uniform2D: return "Uniform 2D";
        case DistributionType::Triangular2D: return "Triangular 2D";
        case DistributionType::Trapezoid2D: return "Trapezoid 2D";
    }
    return "Normal";
}

QStringList distributionNames() {
    QStringList names;
    for (int i = static_cast<int>(DistributionType::Normal);
         i <= static_cast<int>(DistributionType::Trapezoid2D); ++i) {
        names << distributionName(static_cast<DistributionType>(i));
    }
    return names;
}

std::optional<DistributionType> parseDistribution(const QString& text) {
    const QString normalized = text.trimmed();
    const QString compact = normalized.toLower().remove(' ').remove('-');
    const QStringList names = distributionNames();
    for (int i = 0; i < names.size(); ++i) {
        const QString candidate = names[i].toLower().remove(' ').remove('-');
        if (normalized.compare(names[i], Qt::CaseInsensitive) == 0 ||
            compact == candidate) {
            return static_cast<DistributionType>(i);
        }
    }
    return std::nullopt;
}

QString randSpecsText(const std::vector<RandSpec>& rands) {
    QStringList specs;
    for (const RandSpec& rand : rands) {
        specs << QString("%1, %2, %3, %4")
                     .arg(distributionName(rand.distribution),
                          numberText(rand.range),
                          numberText(rand.offset),
                          numberText(rand.sigmaNum));
    }
    return specs.join("; ");
}

std::optional<std::vector<RandSpec>> parseRandSpecs(const QString& text) {
    std::vector<RandSpec> rands;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return rands;

    const QStringList specs = trimmed.split(';', Qt::SkipEmptyParts);
    for (const QString& spec : specs) {
        const QStringList fields = spec.split(',', Qt::KeepEmptyParts);
        if (fields.size() != 4) return std::nullopt;

        const std::optional<DistributionType> distribution =
            parseDistribution(fields[0]);
        if (!distribution.has_value()) return std::nullopt;

        bool rangeOk = false;
        bool offsetOk = false;
        bool sigmaOk = false;
        const double range = fields[1].trimmed().toDouble(&rangeOk);
        const double offset = fields[2].trimmed().toDouble(&offsetOk);
        const double sigmaNum = fields[3].trimmed().toDouble(&sigmaOk);
        if (!rangeOk || !offsetOk || !sigmaOk) return std::nullopt;

        RandSpec rand;
        rand.distribution = distribution.value();
        rand.range = range;
        rand.offset = offset;
        rand.sigmaNum = sigmaNum;
        rands.push_back(rand);
    }
    return rands;
}

QString geomRuleName(GeomRule rule) {
    switch (rule) {
        case GeomRule::TranslateAlongVector: return "Translate Along Vector";
        case GeomRule::RotateAboutLocatorPoint: return "Rotate About Locator Point";
        case GeomRule::NodeNormalOffset: return "Node Normal Offset";
        case GeomRule::SectionRadialOffset: return "Section Radial Offset";
        case GeomRule::DiameterScale: return "Diameter Scale";
    }
    return "Translate Along Vector";
}

QStringList geomRuleNames() {
    QStringList names;
    for (int i = static_cast<int>(GeomRule::TranslateAlongVector);
         i <= static_cast<int>(GeomRule::DiameterScale); ++i) {
        names << geomRuleName(static_cast<GeomRule>(i));
    }
    return names;
}

std::optional<GeomRule> parseGeomRule(const QString& text) {
    const QString normalized = text.trimmed();
    const QString compact = normalized.toLower().remove(' ').remove('-');
    const QStringList names = geomRuleNames();
    for (int i = 0; i < names.size(); ++i) {
        const QString candidate = names[i].toLower().remove(' ').remove('-');
        if (normalized.compare(names[i], Qt::CaseInsensitive) == 0 ||
            compact == candidate) {
            return static_cast<GeomRule>(i);
        }
    }
    return std::nullopt;
}

QString gdtTypeName(GdtType type) {
    switch (type) {
        case GdtType::Size: return "Size";
        case GdtType::Position: return "Position";
        case GdtType::SurfaceProfile: return "Surface Profile";
        case GdtType::Flatness: return "Flatness";
        case GdtType::Perpendicularity: return "Perpendicularity";
        case GdtType::Angularity: return "Angularity";
        case GdtType::Parallelism: return "Parallelism";
        case GdtType::Straightness: return "Straightness";
        case GdtType::TotalRunout: return "Total Runout";
        case GdtType::CircularRunout: return "Circular Runout";
        case GdtType::Circularity: return "Circularity";
        case GdtType::Cylindricity: return "Cylindricity";
        case GdtType::Concentricity: return "Concentricity";
        case GdtType::Symmetry: return "Symmetry";
        case GdtType::LineProfile: return "Line Profile";
        case GdtType::DimensioningLocation: return "Dimensioning Location";
        case GdtType::AngleSize: return "Angle Size";
        case GdtType::TorusMinorDiameterSize: return "Torus Minor Diameter Size";
        case GdtType::SetFeatureAverage: return "Set Feature Average";
    }
    return "Position";
}

QStringList gdtTypeNames() {
    QStringList names;
    for (int i = static_cast<int>(GdtType::Size);
         i <= static_cast<int>(GdtType::SetFeatureAverage); ++i) {
        names << gdtTypeName(static_cast<GdtType>(i));
    }
    return names;
}

std::optional<GdtType> parseGdtType(const QString& text) {
    const QString normalized = text.trimmed();
    const QString compact = normalized.toLower().remove(' ');
    const QStringList names = gdtTypeNames();
    for (int i = 0; i < names.size(); ++i) {
        const QString candidate = names[i].toLower().remove(' ');
        if (normalized.compare(names[i], Qt::CaseInsensitive) == 0 ||
            compact == candidate) {
            return static_cast<GdtType>(i);
        }
    }
    return std::nullopt;
}

QString moveTypeName(MoveType type) {
    switch (type) {
        case MoveType::StepPlane: return "Step Plane";
        case MoveType::SixPlane: return "Six Plane";
        case MoveType::ThreePoint: return "Three Point";
        case MoveType::TwoPoint: return "Two Point";
        case MoveType::BestFit: return "Best Fit";
        case MoveType::FeatureMove: return "Feature Move";
        case MoveType::PatternRigid: return "Pattern Rigid";
        case MoveType::PatternFit: return "Pattern Fit";
        case MoveType::Match: return "Match";
        case MoveType::Iteration: return "Iteration";
        case MoveType::Transform: return "Transform";
        case MoveType::ThermalScaling: return "Thermal Scaling";
        case MoveType::UserDll: return "User DLL";
        case MoveType::AutoBend: return "Auto Bend";
        case MoveType::RTouch: return "RTouch";
        case MoveType::RotateLine: return "Rotate Line";
        case MoveType::Gravity: return "Gravity";
        case MoveType::LeastSquaresAxis: return "Least Squares Axis";
        case MoveType::CrossProduct: return "Cross Product";
        case MoveType::LinePlane: return "Line Plane";
    }
    return "Transform";
}

QStringList moveTypeNames() {
    QStringList names;
    for (int i = static_cast<int>(MoveType::StepPlane);
         i <= static_cast<int>(MoveType::LinePlane); ++i) {
        names << moveTypeName(static_cast<MoveType>(i));
    }
    return names;
}

std::optional<MoveType> parseMoveType(const QString& text) {
    const QString normalized = text.trimmed();
    const QString compact = normalized.toLower().remove(' ').remove('-');
    const QStringList names = moveTypeNames();
    for (int i = 0; i < names.size(); ++i) {
        const QString candidate = names[i].toLower().remove(' ').remove('-');
        if (normalized.compare(names[i], Qt::CaseInsensitive) == 0 ||
            compact == candidate) {
            return static_cast<MoveType>(i);
        }
    }
    return std::nullopt;
}

QString measureTypeName(MeasureType type) {
    switch (type) {
        case MeasureType::NominalPoint: return "Nominal Point";
        case MeasureType::PointPoint: return "Point-Point";
        case MeasureType::PointLine: return "Point-Line";
        case MeasureType::PointPlane: return "Point-Plane";
        case MeasureType::DimensionalDistance: return "Dimensional Distance";
        case MeasureType::CircleInterference: return "Circle Interference";
        case MeasureType::VirtualClearance: return "Virtual Clearance";
        case MeasureType::CircleDiameter: return "Circle Diameter";
        case MeasureType::Circularity: return "Circularity";
        case MeasureType::FeatureMeasure: return "Feature Measure";
        case MeasureType::FeatureAngle: return "Feature Angle";
        case MeasureType::LineNominal: return "Line-Nominal";
        case MeasureType::LineLine: return "Line-Line";
        case MeasureType::LinePlane: return "Line-Plane";
        case MeasureType::PlaneNominal: return "Plane-Nominal";
        case MeasureType::PlanePlane: return "Plane-Plane";
        case MeasureType::TwoPointList: return "Two Point List";
        case MeasureType::Combination: return "Combination";
        case MeasureType::Equation: return "Equation";
        case MeasureType::GdtPosition: return "GD&T Position";
        case MeasureType::GdtSurfaceProfile: return "GD&T Surface Profile";
        case MeasureType::GdtPerpendicularity: return "GD&T Perpendicularity";
        case MeasureType::GdtAngularity: return "GD&T Angularity";
        case MeasureType::GdtParallelism: return "GD&T Parallelism";
        case MeasureType::GdtConcentricity: return "GD&T Concentricity";
        case MeasureType::UserDll: return "User DLL";
    }
    return "Point-Point";
}

QStringList measureTypeNames() {
    QStringList names;
    for (int i = static_cast<int>(MeasureType::NominalPoint);
         i <= static_cast<int>(MeasureType::UserDll); ++i) {
        names << measureTypeName(static_cast<MeasureType>(i));
    }
    return names;
}

std::optional<MeasureType> parseMeasureType(const QString& text) {
    const QString normalized = text.trimmed();
    const QString compact = normalized.toLower().remove(' ').remove('-').remove('&');
    const QStringList names = measureTypeNames();
    for (int i = 0; i < names.size(); ++i) {
        const QString candidate = names[i].toLower().remove(' ').remove('-').remove('&');
        if (normalized.compare(names[i], Qt::CaseInsensitive) == 0 ||
            compact == candidate) {
            return static_cast<MeasureType>(i);
        }
    }
    return std::nullopt;
}

QString specModeName(SpecMode mode) {
    switch (mode) {
        case SpecMode::Absolute: return "Absolute";
        case SpecMode::RelativeToNominal: return "Relative To Nominal";
    }
    return "Absolute";
}

std::optional<SpecMode> parseSpecMode(const QString& text) {
    const QString normalized = text.trimmed();
    const QString compact = normalized.toLower().remove(' ').remove('-');
    if (normalized.compare("Absolute", Qt::CaseInsensitive) == 0 ||
        compact == "absolute") {
        return SpecMode::Absolute;
    }
    if (normalized.compare("Relative To Nominal", Qt::CaseInsensitive) == 0 ||
        normalized.compare("Relative", Qt::CaseInsensitive) == 0 ||
        compact == "relativetonominal") {
        return SpecMode::RelativeToNominal;
    }
    return std::nullopt;
}

QString directionModeName(DirectionMode mode) {
    switch (mode) {
        case DirectionMode::TrueDistance: return "True Distance";
        case DirectionMode::ProjectedOnVector: return "Projected On Vector";
        case DirectionMode::ProjectedOnPlane: return "Projected On Plane";
    }
    return "True Distance";
}

std::optional<DirectionMode> parseDirectionMode(const QString& text) {
    const QString normalized = text.trimmed();
    const QString compact = normalized.toLower().remove(' ').remove('-');
    if (normalized.compare("True Distance", Qt::CaseInsensitive) == 0 ||
        normalized.compare("True", Qt::CaseInsensitive) == 0 ||
        compact == "truedistance") {
        return DirectionMode::TrueDistance;
    }
    if (normalized.compare("Projected On Vector", Qt::CaseInsensitive) == 0 ||
        normalized.compare("Vector", Qt::CaseInsensitive) == 0 ||
        compact == "projectedonvector") {
        return DirectionMode::ProjectedOnVector;
    }
    if (normalized.compare("Projected On Plane", Qt::CaseInsensitive) == 0 ||
        normalized.compare("Plane", Qt::CaseInsensitive) == 0 ||
        compact == "projectedonplane") {
        return DirectionMode::ProjectedOnPlane;
    }
    return std::nullopt;
}

Point* findPoint(Model& model, PointId id) {
    for (Part& part : model.parts) {
        for (Point& point : part.points) {
            if (point.id == id) return &point;
        }
    }
    return nullptr;
}

ToleranceDef* findTolerance(Model& model, ToleranceId id) {
    for (Part& part : model.parts) {
        for (ToleranceDef& tolerance : part.tolerances) {
            if (tolerance.id == id) return &tolerance;
        }
    }
    return nullptr;
}

GdtDef* findGdt(Model& model, GdtId id) {
    for (Part& part : model.parts) {
        for (GdtDef& gdt : part.gdts) {
            if (gdt.id == id) return &gdt;
        }
    }
    return nullptr;
}

MeasureRecord* findMeasure(Model& model, MeasureId id) {
    for (MeasureRecord& measure : model.measures) {
        if (measure.id == id) return &measure;
    }
    return nullptr;
}

std::map<MeasureId, std::string> measureNames(const Model& model) {
    std::map<MeasureId, std::string> names;
    for (const MeasureRecord& measure : model.measures) {
        names[measure.id] = measure.name;
    }
    return names;
}

std::map<ToleranceId, std::string> toleranceNames(const Model& model) {
    std::map<ToleranceId, std::string> names;
    for (const Part& part : model.parts) {
        for (const ToleranceDef& tolerance : part.tolerances) {
            names[tolerance.id] = tolerance.name;
        }
    }
    return names;
}

QString defaultPreferencesPath() {
    QString dirPath =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dirPath.isEmpty()) {
        dirPath = QDir::currentPath();
    }
    QDir dir;
    dir.mkpath(dirPath);
    return QDir(dirPath).filePath("preferences.ini");
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), model_(createStarterModel()) {
    resize(1200, 760);
    setStyleSheet(openDvaTech3DStyleSheet());
    menuBar()->setObjectName("mainMenuBar");
    statusBar()->setObjectName("mainStatusBar");
    preferencesPath_ = defaultPreferencesPath();
    preferences_ = loadAppPreferences(preferencesPath_.toStdString());
    viewport_ = new ModelViewport(this);
    viewport_->setObjectName("modelViewport");
    setCentralWidget(viewport_);
    buildDocks();
    buildMenus();
    refreshUi();
    statusBar()->showMessage("Ready");
}

#ifdef OPENDVA_UI_TESTING
void MainWindow::setModelForTesting(const Model& model) {
    model_ = model;
    refreshUi();
}

bool MainWindow::selectRootForTesting() {
    QTreeWidgetItem* root = navigator_->topLevelItem(0);
    if (root == nullptr) return false;
    navigator_->setCurrentItem(root);
    updateSelectionDetails(root, nullptr);
    return true;
}

bool MainWindow::selectFirstPartForTesting() {
    QTreeWidgetItem* root = navigator_->topLevelItem(0);
    if (root == nullptr) return false;
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* partsCategory = root->child(i);
        if (partsCategory->text(0) == "Parts" &&
            partsCategory->childCount() > 0) {
            navigator_->setCurrentItem(partsCategory->child(0));
            return true;
        }
    }
    return false;
}

bool MainWindow::selectFirstPointForTesting() {
    QTreeWidgetItem* root = navigator_->topLevelItem(0);
    if (root == nullptr) return false;
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* partsCategory = root->child(i);
        if (partsCategory->text(0) != "Parts") continue;
        for (int partIndex = 0; partIndex < partsCategory->childCount();
             ++partIndex) {
            QTreeWidgetItem* part = partsCategory->child(partIndex);
            for (int categoryIndex = 0; categoryIndex < part->childCount();
                 ++categoryIndex) {
                QTreeWidgetItem* category = part->child(categoryIndex);
                if (category->text(0) == "Points" &&
                    category->childCount() > 0) {
                    navigator_->setCurrentItem(category->child(0));
                    return true;
                }
            }
        }
    }
    return false;
}

bool MainWindow::selectFirstFeatureForTesting() {
    QTreeWidgetItem* root = navigator_->topLevelItem(0);
    if (root == nullptr) return false;
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* partsCategory = root->child(i);
        if (partsCategory->text(0) != "Parts") continue;
        for (int partIndex = 0; partIndex < partsCategory->childCount();
             ++partIndex) {
            QTreeWidgetItem* part = partsCategory->child(partIndex);
            for (int categoryIndex = 0; categoryIndex < part->childCount();
                 ++categoryIndex) {
                QTreeWidgetItem* category = part->child(categoryIndex);
                if (category->text(0) == "Features" &&
                    category->childCount() > 0) {
                    navigator_->setCurrentItem(category->child(0));
                    return true;
                }
            }
        }
    }
    return false;
}

bool MainWindow::selectFirstToleranceForTesting() {
    QTreeWidgetItem* root = navigator_->topLevelItem(0);
    if (root == nullptr) return false;
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* partsCategory = root->child(i);
        if (partsCategory->text(0) != "Parts") continue;
        for (int partIndex = 0; partIndex < partsCategory->childCount();
             ++partIndex) {
            QTreeWidgetItem* part = partsCategory->child(partIndex);
            for (int categoryIndex = 0; categoryIndex < part->childCount();
                 ++categoryIndex) {
                QTreeWidgetItem* category = part->child(categoryIndex);
                if (category->text(0) == "Tolerances" &&
                    category->childCount() > 0) {
                    navigator_->setCurrentItem(category->child(0));
                    return true;
                }
            }
        }
    }
    return false;
}

bool MainWindow::selectFirstGdtForTesting() {
    QTreeWidgetItem* root = navigator_->topLevelItem(0);
    if (root == nullptr) return false;
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* partsCategory = root->child(i);
        if (partsCategory->text(0) != "Parts") continue;
        for (int partIndex = 0; partIndex < partsCategory->childCount();
             ++partIndex) {
            QTreeWidgetItem* part = partsCategory->child(partIndex);
            for (int categoryIndex = 0; categoryIndex < part->childCount();
                 ++categoryIndex) {
                QTreeWidgetItem* category = part->child(categoryIndex);
                if (category->text(0) == "GD&T" &&
                    category->childCount() > 0) {
                    navigator_->setCurrentItem(category->child(0));
                    return true;
                }
            }
        }
    }
    return false;
}

bool MainWindow::selectFirstMoveForTesting() {
    QTreeWidgetItem* root = navigator_->topLevelItem(0);
    if (root == nullptr) return false;
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* category = root->child(i);
        if (category->text(0) == "Moves" && category->childCount() > 0) {
            navigator_->setCurrentItem(category->child(0));
            return true;
        }
    }
    return false;
}

bool MainWindow::selectFirstMeasureForTesting() {
    QTreeWidgetItem* root = navigator_->topLevelItem(0);
    if (root == nullptr) return false;
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* category = root->child(i);
        if (category->text(0) == "Measures" && category->childCount() > 0) {
            navigator_->setCurrentItem(category->child(0));
            return true;
        }
    }
    return false;
}

bool MainWindow::selectFirstVariantForTesting() {
    QTreeWidgetItem* root = navigator_->topLevelItem(0);
    if (root == nullptr) return false;
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* category = root->child(i);
        if (category->text(0) == "Variants" && category->childCount() > 0) {
            navigator_->setCurrentItem(category->child(0));
            return true;
        }
    }
    return false;
}

bool MainWindow::moveNavigatorMoveForTesting(int fromIndex, int toIndex) {
    if (navigator_ == nullptr || navigator_->topLevelItemCount() == 0) {
        return false;
    }
    QTreeWidgetItem* moves =
        childCategory(navigator_->topLevelItem(0), "Moves");
    if (moves == nullptr || fromIndex < 0 || toIndex < 0 ||
        fromIndex >= moves->childCount() || toIndex > moves->childCount()) {
        return false;
    }

    QTreeWidgetItem* moved = moves->takeChild(fromIndex);
    if (moved == nullptr) return false;
    if (toIndex > moves->childCount()) {
        toIndex = moves->childCount();
    }
    moves->insertChild(toIndex, moved);
    navigator_->setCurrentItem(moved);
    const MoveId movedId = moved->data(0, kNodeIdRole).toULongLong();
    return syncMoveOrderFromNavigator(movedId);
}

void MainWindow::setPreferencesForTesting(const AppPreferences& preferences) {
    preferences_ = preferences;
    updateRecentFilesMenu();
}

void MainWindow::setPreferencesPathForTesting(const QString& path) {
    preferencesPath_ = path;
}

void MainWindow::applyPreferencesForTesting(const AppPreferences& preferences) {
    applyPreferences(preferences);
}

QStringList MainWindow::recentFileActionTextsForTesting() const {
    QStringList texts;
    if (recentFilesMenu_ == nullptr) return texts;
    for (QAction* action : recentFilesMenu_->actions()) {
        if (action != nullptr) texts.push_back(action->text());
    }
    return texts;
}

bool MainWindow::recentFileActionEnabledForTesting(int index) const {
    if (recentFilesMenu_ == nullptr || index < 0 ||
        index >= recentFilesMenu_->actions().size()) {
        return false;
    }
    const QAction* action = recentFilesMenu_->actions()[index];
    return action != nullptr && action->isEnabled();
}

bool MainWindow::triggerRecentFileForTesting(int index) {
    if (recentFilesMenu_ == nullptr || index < 0 ||
        index >= recentFilesMenu_->actions().size()) {
        return false;
    }
    QAction* action = recentFilesMenu_->actions()[index];
    if (action == nullptr || !action->isEnabled()) return false;
    action->trigger();
    return true;
}

bool MainWindow::openModelFromPathForTesting(const QString& path) {
    return openModelFromPath(path);
}

bool MainWindow::saveModelToPathForTesting(const QString& path) {
    return saveModelToPath(path);
}

void MainWindow::newModelForTesting() {
    newModel();
}

const Model& MainWindow::modelForTesting() const {
    return model_;
}

QTableWidget* MainWindow::propertyTableForTesting() const {
    return propertyTable_;
}

QStringList MainWindow::navigatorContextActionNamesForTesting() {
    QTreeWidgetItem* current = navigator_ ? navigator_->currentItem() : nullptr;
    const auto selectedKind = current
        ? static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt())
        : NavigatorNodeKind::Root;
    QMenu menu(this);
    populateNavigatorContextMenu(menu, selectedKind);
    QStringList names;
    for (QAction* action : menu.actions()) {
        if (action != nullptr && !action->objectName().isEmpty()) {
            names.push_back(action->objectName());
        }
    }
    return names;
}

QStringList MainWindow::navigatorContextActionTextsForTesting() {
    QTreeWidgetItem* current = navigator_ ? navigator_->currentItem() : nullptr;
    const auto selectedKind = current
        ? static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt())
        : NavigatorNodeKind::Root;
    QMenu menu(this);
    populateNavigatorContextMenu(menu, selectedKind);
    QStringList texts;
    for (QAction* action : menu.actions()) {
        if (action != nullptr && !action->isSeparator()) {
            texts.push_back(visibleActionText(action->text()));
        }
    }
    return texts;
}

QStringList MainWindow::navigatorContextActionSequenceForTesting() {
    QTreeWidgetItem* current = navigator_ ? navigator_->currentItem() : nullptr;
    const auto selectedKind = current
        ? static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt())
        : NavigatorNodeKind::Root;
    QMenu menu(this);
    populateNavigatorContextMenu(menu, selectedKind);
    QStringList sequence;
    for (QAction* action : menu.actions()) {
        if (action == nullptr) continue;
        if (action->isSeparator()) {
            sequence.push_back("<separator>");
        } else {
            sequence.push_back(action->objectName());
        }
    }
    return sequence;
}

QStringList MainWindow::navigatorContextEnabledActionNamesForTesting() {
    QTreeWidgetItem* current = navigator_ ? navigator_->currentItem() : nullptr;
    const auto selectedKind = current
        ? static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt())
        : NavigatorNodeKind::Root;
    QMenu menu(this);
    populateNavigatorContextMenu(menu, selectedKind);
    QStringList names;
    for (QAction* action : menu.actions()) {
        if (action != nullptr && action->isEnabled() &&
            !action->objectName().isEmpty()) {
            names.push_back(action->objectName());
        }
    }
    return names;
}
#endif

void MainWindow::buildMenus() {
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->setObjectName("fileMenu");
    QAction* newModelAction =
        fileMenu->addAction("&New", this, &MainWindow::newModel);
    newModelAction->setObjectName("newModelAction");
    applyIcon(newModelAction, "new-model");
    QAction* openModelAction =
        fileMenu->addAction("&Open...", this, &MainWindow::openModel);
    openModelAction->setObjectName("openModelAction");
    applyIcon(openModelAction, "open-model");
    recentFilesMenu_ = fileMenu->addMenu("Open &Recent");
    recentFilesMenu_->setObjectName("recentFilesMenu");
    updateRecentFilesMenu();
    QAction* saveModelAction =
        fileMenu->addAction("&Save", this, &MainWindow::saveModel);
    saveModelAction->setObjectName("saveModelAction");
    applyIcon(saveModelAction, "save-model");
    QAction* saveModelAsAction =
        fileMenu->addAction("Save &As...", this, &MainWindow::saveModelAs);
    saveModelAsAction->setObjectName("saveModelAsAction");
    applyIcon(saveModelAsAction, "save-model");
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction("E&xit", this, &QWidget::close);
    exitAction->setObjectName("exitAction");

    QMenu* analysisMenu = menuBar()->addMenu("&Analysis");
    analysisMenu->setObjectName("analysisMenu");
    QAction* validateModelAction =
        analysisMenu->addAction("&Validate Model", this, &MainWindow::validateModel);
    validateModelAction->setObjectName("validateModelAction");
    applyIcon(validateModelAction, "validate");
    analysisMenu->addSeparator();
    QAction* runMonteCarloAction =
        analysisMenu->addAction("Run &Monte Carlo", this, &MainWindow::runMonteCarlo);
    runMonteCarloAction->setObjectName("runMonteCarloAction");
    applyIcon(runMonteCarloAction, "run-simulation");
    QAction* runBatchProcessorAction =
        analysisMenu->addAction("&Batch Processor...", this,
                                &MainWindow::runBatchProcessor);
    runBatchProcessorAction->setObjectName("runBatchProcessorAction");
    applyIcon(runBatchProcessorAction, "batch-processor");
    analysisMenu->addSeparator();
    QAction* geoFactorMatrixAction =
        analysisMenu->addAction("GeoFactor &Matrix...", this, [this]() {
            showModuleWindow("geoFactorMatrix");
        });
    geoFactorMatrixAction->setObjectName("geoFactorMatrixAction");
    applyIcon(geoFactorMatrixAction, "color-contour");
    QAction* ctiAnalyzerAction =
        analysisMenu->addAction("&CTI Analyzer...", this, [this]() {
            showModuleWindow("ctiAnalyzer");
        });
    ctiAnalyzerAction->setObjectName("ctiAnalyzerAction");
    applyIcon(ctiAnalyzerAction, "measure");

    QMenu* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->setObjectName("toolsMenu");
    QAction* userDllManagerAction =
        toolsMenu->addAction("&User DLL Manager...", this, [this]() {
            showModuleWindow("userDllManager");
        });
    userDllManagerAction->setObjectName("userDllManagerAction");
    applyIcon(userDllManagerAction, "user-dll");
    toolsMenu->addSeparator();
    QAction* preferencesAction =
        toolsMenu->addAction("&Preferences...", this, &MainWindow::showPreferences);
    preferencesAction->setObjectName("preferencesAction");
    applyIcon(preferencesAction, "batch-processor");

    QMenu* modelMenu = menuBar()->addMenu("&Model");
    modelMenu->setObjectName("modelMenu");
    QAction* addPartAction =
        modelMenu->addAction("Add &Part", this, &MainWindow::addPart);
    addPartAction->setObjectName("addPartAction");
    applyIcon(addPartAction, "feature");
    QAction* addPointAction =
        modelMenu->addAction("Add &Point", this, &MainWindow::addPoint);
    addPointAction->setObjectName("addPointAction");
    applyIcon(addPointAction, "point");
    QAction* addFeatureAction =
        modelMenu->addAction("Add &Feature", this, &MainWindow::addFeature);
    addFeatureAction->setObjectName("addFeatureAction");
    applyIcon(addFeatureAction, "feature");
    QAction* addLinearToleranceAction =
        modelMenu->addAction("Add Linear &Tolerance", this, &MainWindow::addLinearTolerance);
    addLinearToleranceAction->setObjectName("addLinearToleranceAction");
    applyIcon(addLinearToleranceAction, "tolerance");
    QAction* addGdtAction =
        modelMenu->addAction("Add &GD&&T", this, &MainWindow::addGdt);
    addGdtAction->setObjectName("addGdtAction");
    applyIcon(addGdtAction, "gdt");
    QAction* addTransformMoveAction =
        modelMenu->addAction("Add Transform &Move", this, &MainWindow::addTransformMove);
    addTransformMoveAction->setObjectName("addTransformMoveAction");
    applyIcon(addTransformMoveAction, "move");
    QAction* addPointPointMeasureAction =
        modelMenu->addAction("Add Point-Point &Measure", this,
                             &MainWindow::addPointPointMeasure);
    addPointPointMeasureAction->setObjectName("addPointPointMeasureAction");
    applyIcon(addPointPointMeasureAction, "measure");
    modelMenu->addSeparator();
    QAction* moveSelectedMoveUpAction =
        modelMenu->addAction("Move Selected Move &Up", this,
                             &MainWindow::moveSelectedMoveUp);
    moveSelectedMoveUpAction->setObjectName("moveSelectedMoveUpAction");
    QAction* moveSelectedMoveDownAction =
        modelMenu->addAction("Move Selected Move &Down", this,
                             &MainWindow::moveSelectedMoveDown);
    moveSelectedMoveDownAction->setObjectName("moveSelectedMoveDownAction");
    modelMenu->addSeparator();
    QAction* captureModelVariantAction =
        modelMenu->addAction("Capture Model &Variant...", this,
                             &MainWindow::captureModelVariant);
    captureModelVariantAction->setObjectName("captureModelVariantAction");
    applyIcon(captureModelVariantAction, "variant");
    QAction* applyModelVariantAction =
        modelMenu->addAction("Apply Model Variant...", this,
                             &MainWindow::applyModelVariant);
    applyModelVariantAction->setObjectName("applyModelVariantAction");
    applyIcon(applyModelVariantAction, "variant");
    QAction* deleteModelVariantAction =
        modelMenu->addAction("Delete Model Variant...", this,
                             &MainWindow::deleteModelVariant);
    deleteModelVariantAction->setObjectName("deleteModelVariantAction");
    applyIcon(deleteModelVariantAction, "variant");

    QMenu* displayMenu = menuBar()->addMenu("&Display");
    displayMenu->setObjectName("displayMenu");
    QAction* displayOptionsAction =
        displayMenu->addAction("Display &Options...", this, [this]() {
            showModuleWindow("displayOptions");
        });
    displayOptionsAction->setObjectName("displayOptionsAction");
    applyIcon(displayOptionsAction, "color-contour");
    QAction* animationWindowAction =
        displayMenu->addAction("&Animation Window...", this, [this]() {
            showModuleWindow("animationWindow");
        });
    animationWindowAction->setObjectName("animationWindowAction");
    applyIcon(animationWindowAction, "run-simulation");

    QMenu* reportMenu = menuBar()->addMenu("&Report");
    reportMenu->setObjectName("reportMenu");
    QAction* generateReportAction =
        reportMenu->addAction("&Generate Report...", this, [this]() {
            showModuleWindow("generateReport");
        });
    generateReportAction->setObjectName("generateReportAction");
    applyIcon(generateReportAction, "report");
    QAction* specStudyAction =
        reportMenu->addAction("&Spec Study...", this, [this]() {
            showModuleWindow("specStudy");
        });
    specStudyAction->setObjectName("specStudyAction");
    applyIcon(specStudyAction, "variant");

    QMenu* aaoMenu = menuBar()->addMenu("&AAO");
    aaoMenu->setObjectName("aaoMenu");
    QAction* toleranceOptimizerAction =
        aaoMenu->addAction("&Tolerance Optimizer...", this, [this]() {
            showModuleWindow("toleranceOptimizer");
        });
    toleranceOptimizerAction->setObjectName("toleranceOptimizerAction");
    applyIcon(toleranceOptimizerAction, "tolerance");
    QAction* sequenceOptimizerAction =
        aaoMenu->addAction("&Sequence Optimizer...", this, [this]() {
            showModuleWindow("sequenceOptimizer");
        });
    sequenceOptimizerAction->setObjectName("sequenceOptimizerAction");
    applyIcon(sequenceOptimizerAction, "move");
    QAction* datumOptimizerAction =
        aaoMenu->addAction("&Datum Optimizer...", this, [this]() {
            showModuleWindow("datumOptimizer");
        });
    datumOptimizerAction->setObjectName("datumOptimizerAction");
    applyIcon(datumOptimizerAction, "gdt");

    QMenu* mechanicalMenu = menuBar()->addMenu("M&echanical");
    mechanicalMenu->setObjectName("mechanicalMenu");
    QAction* dofCounterAction =
        mechanicalMenu->addAction("&DOF Counter...", this, [this]() {
            showModuleWindow("dofCounter");
        });
    dofCounterAction->setObjectName("dofCounterAction");
    applyIcon(dofCounterAction, "move");
    QAction* kinematicsAction =
        mechanicalMenu->addAction("&Kinematics...", this, [this]() {
            showModuleWindow("kinematics");
        });
    kinematicsAction->setObjectName("kinematicsAction");
    applyIcon(kinematicsAction, "run-simulation");
    QAction* collisionDetectionAction =
        mechanicalMenu->addAction("&Collision Detection...", this, [this]() {
            showModuleWindow("collisionDetection");
        });
    collisionDetectionAction->setObjectName("collisionDetectionAction");
    applyIcon(collisionDetectionAction, "measure");

    QMenu* feaMenu = menuBar()->addMenu("&FEA");
    feaMenu->setObjectName("feaMenu");
    QAction* stiffGenAction =
        feaMenu->addAction("&StiffGen...", this, [this]() {
            showModuleWindow("stiffGen");
        });
    stiffGenAction->setObjectName("stiffGenAction");
    applyIcon(stiffGenAction, "feature");
    QAction* loadFeaDataAction =
        feaMenu->addAction("&Load FEA Data...", this, [this]() {
            showModuleWindow("loadFeaData");
        });
    loadFeaDataAction->setObjectName("loadFeaDataAction");
    applyIcon(loadFeaDataAction, "open-model");
    QAction* pointLinkWizardAction =
        feaMenu->addAction("&Point Link Wizard...", this, [this]() {
            showModuleWindow("pointLinkWizard");
        });
    pointLinkWizardAction->setObjectName("pointLinkWizardAction");
    applyIcon(pointLinkWizardAction, "point");
    QAction* validateCompliantModelAction =
        feaMenu->addAction("&Validate Compliant Model...", this, [this]() {
            showModuleWindow("validateCompliantModel");
        });
    validateCompliantModelAction->setObjectName("validateCompliantModelAction");
    applyIcon(validateCompliantModelAction, "validate");

    QMenu* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->setObjectName("helpMenu");
    QAction* welcomeAction =
        helpMenu->addAction("&Welcome...", this, [this]() {
            showModuleWindow("welcome");
        });
    welcomeAction->setObjectName("welcomeAction");
    applyIcon(welcomeAction, "new-model");
    QAction* licenseStatusAction =
        helpMenu->addAction("&License Status...", this, [this]() {
            showModuleWindow("licenseStatus");
        });
    licenseStatusAction->setObjectName("licenseStatusAction");
    applyIcon(licenseStatusAction, "validate");
    QAction* systemInformationAction =
        helpMenu->addAction("&System Information...", this, [this]() {
            showModuleWindow("systemInformation");
        });
    systemInformationAction->setObjectName("systemInformationAction");
    applyIcon(systemInformationAction, "batch-processor");

    auto* toolbar = addToolBar("OpenDVA");
    toolbar->setObjectName("mainToolBar");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(30, 30));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    QAction* newModelToolAction =
        toolbar->addAction("New", this, &MainWindow::newModel);
    newModelToolAction->setObjectName("newModelToolAction");
    applyIcon(newModelToolAction, "new-model");
    QAction* openModelToolAction =
        toolbar->addAction("Open", this, &MainWindow::openModel);
    openModelToolAction->setObjectName("openModelToolAction");
    applyIcon(openModelToolAction, "open-model");
    QAction* saveModelToolAction =
        toolbar->addAction("Save", this, &MainWindow::saveModel);
    saveModelToolAction->setObjectName("saveModelToolAction");
    applyIcon(saveModelToolAction, "save-model");
    toolbar->addSeparator();
    QAction* addPointToolAction =
        toolbar->addAction("Point", this, &MainWindow::addPoint);
    addPointToolAction->setObjectName("addPointToolAction");
    applyIcon(addPointToolAction, "point");
    QAction* addFeatureToolAction =
        toolbar->addAction("Feature", this, &MainWindow::addFeature);
    addFeatureToolAction->setObjectName("addFeatureToolAction");
    applyIcon(addFeatureToolAction, "feature");
    QAction* addLinearToleranceToolAction =
        toolbar->addAction("Tolerance", this, &MainWindow::addLinearTolerance);
    addLinearToleranceToolAction->setObjectName("addLinearToleranceToolAction");
    applyIcon(addLinearToleranceToolAction, "tolerance");
    QAction* addGdtToolAction =
        toolbar->addAction("GD&&T", this, &MainWindow::addGdt);
    addGdtToolAction->setObjectName("addGdtToolAction");
    applyIcon(addGdtToolAction, "gdt");
    QAction* addTransformMoveToolAction =
        toolbar->addAction("Move", this, &MainWindow::addTransformMove);
    addTransformMoveToolAction->setObjectName("addTransformMoveToolAction");
    applyIcon(addTransformMoveToolAction, "move");
    QAction* addPointPointMeasureToolAction =
        toolbar->addAction("Measure", this, &MainWindow::addPointPointMeasure);
    addPointPointMeasureToolAction->setObjectName(
        "addPointPointMeasureToolAction");
    applyIcon(addPointPointMeasureToolAction, "measure");
    QAction* captureModelVariantToolAction =
        toolbar->addAction("Variant", this, &MainWindow::captureModelVariant);
    captureModelVariantToolAction->setObjectName(
        "captureModelVariantToolAction");
    applyIcon(captureModelVariantToolAction, "variant");
    toolbar->addSeparator();
    QAction* validateModelToolAction =
        toolbar->addAction("Validate", this, &MainWindow::validateModel);
    validateModelToolAction->setObjectName("validateModelToolAction");
    applyIcon(validateModelToolAction, "validate");
    QAction* runMonteCarloToolAction =
        toolbar->addAction("Run", this, &MainWindow::runMonteCarlo);
    runMonteCarloToolAction->setObjectName("runMonteCarloToolAction");
    applyIcon(runMonteCarloToolAction, "run-simulation");
    QAction* runBatchProcessorToolAction =
        toolbar->addAction("Batch", this, &MainWindow::runBatchProcessor);
    runBatchProcessorToolAction->setObjectName("runBatchProcessorToolAction");
    applyIcon(runBatchProcessorToolAction, "batch-processor");
}

void MainWindow::buildDocks() {
    auto* navigator = new NavigatorTree(this);
    navigator_ = navigator;
    navigator_->setObjectName("modelNavigator");
    navigator_->setHeaderLabel("Model Navigator");
    navigator_->setSelectionMode(QAbstractItemView::SingleSelection);
    navigator_->setDragEnabled(true);
    navigator_->setAcceptDrops(true);
    navigator_->setDropIndicatorShown(true);
    navigator_->setDragDropMode(QAbstractItemView::InternalMove);
    navigator_->setContextMenuPolicy(Qt::CustomContextMenu);
    navigator->moveDropped = [this](MoveId movedId) {
        syncMoveOrderFromNavigator(movedId);
    };
    connect(navigator_, &QTreeWidget::currentItemChanged,
            this, &MainWindow::updateSelectionDetails);
    connect(navigator_, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::showNavigatorMenu);

    auto* navDock = new QDockWidget("Model Navigator", this);
    navDock->setObjectName("ModelNavigatorDock");
    navDock->setWidget(navigator_);
    addDockWidget(Qt::LeftDockWidgetArea, navDock);

    propertyTable_ = new QTableWidget(this);
    propertyTable_->setObjectName("propertyTable");
    propertyTable_->setColumnCount(2);
    propertyTable_->setHorizontalHeaderLabels(QStringList{"Property", "Value"});
    propertyTable_->setMinimumWidth(300);
    propertyTable_->verticalHeader()->setVisible(false);
    propertyTable_->setAlternatingRowColors(true);
    connect(propertyTable_, &QTableWidget::itemChanged,
            this, &MainWindow::handlePropertyEdited);

    auto* propDock = new QDockWidget("Properties", this);
    propDock->setObjectName("PropertiesDock");
    propDock->setWidget(propertyTable_);
    addDockWidget(Qt::RightDockWidgetArea, propDock);
}

void MainWindow::validateModel() {
    const std::vector<ModelIssue> issues = validateModelForSimulation(model_);
    auto* dlg = new ValidationResultsDialog(issues, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    statusBar()->showMessage(validationStatusText(issues));
}

void MainWindow::populateNavigator() {
    navigator_->clear();

    auto* root = new QTreeWidgetItem(navigator_,
                                     QStringList{QString::fromStdString(model_.assemblyName)});
    tagItem(root, NavigatorNodeKind::Root);
    root->setExpanded(true);

    // Parts -> Points / Features / Tolerances. Moves / Measures live at the
    // assembly level (Model owns them), shown under the root.
    auto* partsNode = addCategory(root, "Parts");
    for (const Part& part : model_.parts) {
        auto* partItem = new QTreeWidgetItem(
            partsNode, QStringList{QString::fromStdString(part.dcsName)});
        tagItem(partItem, NavigatorNodeKind::Part, part.id);
        partItem->setExpanded(true);

        auto* pts = addCategory(partItem, "Points");
        for (const Point& p : part.points) {
            auto* item = new QTreeWidgetItem(pts, QStringList{QString("Point %1").arg(p.id)});
            tagItem(item, NavigatorNodeKind::Point, p.id);
        }

        auto* feats = addCategory(partItem, "Features");
        for (const Feature& f : part.features) {
            auto* item = new QTreeWidgetItem(feats, QStringList{QString("Feature %1").arg(f.id)});
            tagItem(item, NavigatorNodeKind::Feature, f.id);
        }

        auto* tols = addCategory(partItem, "Tolerances");
        for (const ToleranceDef& t : part.tolerances) {
            auto* item = new QTreeWidgetItem(tols, QStringList{QString::fromStdString(t.name)});
            tagItem(item, NavigatorNodeKind::Tolerance, t.id);
        }

        auto* gdts = addCategory(partItem, "GD&T");
        for (const GdtDef& g : part.gdts) {
            auto* item = new QTreeWidgetItem(gdts, QStringList{QString::fromStdString(g.name)});
            tagItem(item, NavigatorNodeKind::Gdt, g.id);
        }
    }

    auto* moves = addCategory(root, "Moves");
    for (const MoveDef& mv : model_.moves) {
        auto* item = new QTreeWidgetItem(moves, QStringList{QString::fromStdString(mv.name)});
        tagItem(item, NavigatorNodeKind::Move, mv.id);
    }

    auto* measures = addCategory(root, "Measures");
    for (const MeasureRecord& mr : model_.measures) {
        auto* item = new QTreeWidgetItem(measures, QStringList{QString::fromStdString(mr.name)});
        tagItem(item, NavigatorNodeKind::Measure, mr.id);
    }

    auto* variants = addCategory(root, "Variants");
    for (const ModelVariant& variant : model_.variants) {
        QString label = QString::fromStdString(variant.name);
        if (variant.active) label += " [active]";
        auto* item = new QTreeWidgetItem(variants, QStringList{label});
        tagVariantItem(item, variant.name);
    }

    navigator_->setCurrentItem(root);
}

void MainWindow::runMonteCarlo() {
    const QString variantName = activeVariantText(model_);
    const Model runModel = model_.activeVariantApplied();
    const std::vector<ModelIssue> issues = validateModelForSimulation(runModel);
    if (hasBlockingIssues(issues)) {
        ValidationResultsDialog dlg(issues, this);
        dlg.exec();
        statusBar()->showMessage(validationStatusText(issues));
        return;
    }

    auto engine = makeMonteCarloEngine();
    RunAnalysisDialog runDialog(preferences_.analysisDefaults, this);
    if (runDialog.exec() != QDialog::Accepted) {
        statusBar()->showMessage("Run cancelled");
        return;
    }

    preferences_.analysisDefaults = runDialog.settings();
    savePreferences();
    if (!shouldRunAnyAnalysis(preferences_.analysisDefaults)) {
        statusBar()->showMessage("No analysis selected");
        return;
    }

    const RunConfig cfg = toRunConfig(preferences_.analysisDefaults, 64);
    MonteCarloResult result;
    if (shouldRunMonteCarlo(preferences_.analysisDefaults)) {
        result = engine->runMonteCarloDetailed(runModel, cfg);
    }
    std::vector<ContributorRow> contributors;
    if (shouldRunContributor(preferences_.analysisDefaults)) {
        contributors = engine->runContributor(runModel);
    }
    viewport_->setPointDeviations(pointDeviationsFromContributors(runModel, contributors),
                                  1.0, 0.0);

    QString resultTitle = QString::fromStdString(model_.assemblyName);
    if (!variantName.isEmpty()) resultTitle += QString(" - %1").arg(variantName);
    auto* dlg = new SimulationResultsDialog(resultTitle,
                                            result.stats, measureNames(runModel),
                                            result.samples, contributors,
                                            toleranceNames(runModel),
                                            QString::fromStdString(preferences_.defaultReportPath),
                                            [this](const QString& path) {
                                                return viewport_->saveSnapshot(path);
                                            },
                                            this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    QStringList completed;
    if (shouldRunMonteCarlo(preferences_.analysisDefaults)) {
        completed << QString("Monte Carlo %1 runs").arg(cfg.totalRuns);
    }
    if (shouldRunContributor(preferences_.analysisDefaults)) {
        completed << "Contributor";
    }
    QString status = "Analysis complete: " + completed.join(", ");
    if (!variantName.isEmpty()) status += QString(" - %1").arg(variantName);
    statusBar()->showMessage(status);
}

void MainWindow::runBatchProcessor() {
    BatchProcessorDialog dialog(preferences_.analysisDefaults, this);
    if (dialog.exec() != QDialog::Accepted) {
        statusBar()->showMessage("Batch cancelled");
        return;
    }

    preferences_.analysisDefaults = dialog.settings();
    savePreferences();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    BatchAnalysisResult result = runBatchModelFileAnalysis(
        dialog.jobs(),
        preferences_.analysisDefaults.totalRuns,
        preferences_.analysisDefaults.initialSeed,
        preferences_.analysisDefaults.threads);
    QApplication::restoreOverrideCursor();

    QString summary = batchSummaryText(result);
    statusBar()->showMessage(summary);

    QString details = batchDetailsText(result);
    if (result.failed > 0) {
        QMessageBox box(QMessageBox::Information, "Batch Processor",
                        details.isEmpty() ? summary : summary + "\n\n" + details,
                        QMessageBox::NoButton, this);
        QPushButton* retryButton =
            box.addButton("Retry Failed", QMessageBox::ActionRole);
        box.addButton(QMessageBox::Ok);
        box.exec();

        if (box.clickedButton() == retryButton) {
            const std::vector<BatchAnalysisJob> retryJobs =
                makeFailedBatchRetryJobs(result);
            if (!retryJobs.empty()) {
                statusBar()->showMessage("Retrying failed batch items...");
                QApplication::setOverrideCursor(Qt::WaitCursor);
                const BatchAnalysisResult retryResult = runBatchModelFileAnalysis(
                    retryJobs,
                    preferences_.analysisDefaults.totalRuns,
                    preferences_.analysisDefaults.initialSeed,
                    preferences_.analysisDefaults.threads);
                QApplication::restoreOverrideCursor();

                result = mergeBatchRetryResult(result, retryResult);
                summary = batchSummaryText(result, "Batch retry completed");
                statusBar()->showMessage(summary);
                details = batchDetailsText(result);
                QMessageBox::information(
                    this, "Batch Processor",
                    details.isEmpty() ? summary : summary + "\n\n" + details);
            }
        }
        return;
    }

    QMessageBox::information(this, "Batch Processor",
                             details.isEmpty() ? summary : summary + "\n\n" + details);
}

void MainWindow::newModel() {
    if (!maybeSaveDirtyModel()) return;
    model_ = createStarterModel();
    currentPath_.clear();
    setDirty(false);
    refreshUi();
    statusBar()->showMessage("New model created");
}

void MainWindow::openModel() {
    if (!maybeSaveDirtyModel()) return;
    const QString path = QFileDialog::getOpenFileName(this, "Open OpenDVA Model", QString(),
                                                     "OpenDVA Model (*.xml *.odva);;All Files (*)");
    if (path.isEmpty()) return;

    openModelFromPath(path);
}

void MainWindow::openRecentModel() {
    auto* action = qobject_cast<QAction*>(sender());
    if (!action) return;

    const QString path = action->data().toString();
    if (path.isEmpty()) return;
    if (!maybeSaveDirtyModel()) return;

    openModelFromPath(path);
}

bool MainWindow::openModelFromPath(const QString& path) {
    Model loaded;
    if (!opendva::loadModel(loaded, path.toStdString())) {
        QMessageBox::warning(this, "Open failed", "Could not read the selected OpenDVA model.");
        return false;
    }

    model_ = std::move(loaded);
    currentPath_ = path;
    setDirty(false);
    recordRecentModelPath(path);
    refreshUi();
    statusBar()->showMessage(QString("Opened %1").arg(path));
    return true;
}

void MainWindow::recordRecentModelPath(const QString& path) {
    rememberRecentModelPath(preferences_, path.toStdString());
    savePreferences();
    updateRecentFilesMenu();
}

void MainWindow::savePreferences() {
    if (preferencesPath_.isEmpty()) return;
    saveAppPreferences(preferences_, preferencesPath_.toStdString());
}

void MainWindow::updateRecentFilesMenu() {
    if (!recentFilesMenu_) return;

    recentFilesMenu_->clear();
    if (preferences_.recentModelPaths.empty()) {
        QAction* emptyAction = recentFilesMenu_->addAction("(No recent files)");
        emptyAction->setObjectName("emptyRecentFilesAction");
        emptyAction->setEnabled(false);
        return;
    }

    int index = 0;
    for (const std::string& modelPath : preferences_.recentModelPaths) {
        const QString path = QString::fromStdString(modelPath);
        QAction* action = recentFilesMenu_->addAction(path, this,
                                                      &MainWindow::openRecentModel);
        action->setObjectName(QString("recentFileAction%1").arg(index));
        action->setData(path);
        ++index;
    }
}

void MainWindow::saveModel() {
    if (currentPath_.isEmpty()) {
        saveModelAs();
        return;
    }
    saveModelToPath(currentPath_);
}

void MainWindow::saveModelAs() {
    const QString path = QFileDialog::getSaveFileName(this, "Save OpenDVA Model",
                                                     currentPath_.isEmpty() ? "model.xml"
                                                                            : currentPath_,
                                                     "OpenDVA Model (*.xml *.odva);;All Files (*)");
    if (path.isEmpty()) return;
    saveModelToPath(path);
}

void MainWindow::showPreferences() {
    PreferencesDialog dialog(preferences_, this);
    if (dialog.exec() != QDialog::Accepted) return;

    applyPreferences(dialog.preferences());
}

void MainWindow::showModuleWindow(const QString& windowId) {
    struct ModuleWindowSpec {
        const char* id;
        const char* title;
        const char* objectName;
        const char* summary;
        QStringList capabilities;
    };

    const std::vector<ModuleWindowSpec> specs{
        {"displayOptions", "Display Options", "displayOptionsWindow",
         "Controls viewport visibility, point labels, mesh display, rendering quality, and graphics performance.",
         {"General display toggles", "Appearance and color controls",
          "Effects and performance settings"}},
        {"animationWindow", "Animation Window", "animationWindow",
         "Runs graphical build/deviate animation sequences from the current model state.",
         {"Run At and step count", "Delay and hold controls", "Cancel-safe animation flow"}},
        {"generateReport", "Generate Report", "generateReportWindow",
         "Collects report format, saved views, MTM views, and output destinations.",
         {"HTML report", "Spreadsheet-style export", "Saved model view references"}},
        {"specStudy", "Spec Study", "specStudyWindow",
         "Manages named specification studies and design variants for repeated analysis.",
         {"Variant table", "CSV import and export", "Summary generation"}},
        {"geoFactorMatrix", "GeoFactor Matrix", "geoFactorMatrixWindow",
         "Shows the sensitivity matrix used by GeoFactor and what-if contribution analysis.",
         {"Per-tolerance matrix", "Per-feature matrix", "HLM/GF result loading"}},
        {"ctiAnalyzer", "CTI Analyzer", "ctiAnalyzerWindow",
         "Ranks critical tolerances across multiple measures from the current analysis set.",
         {"Cross-measure ranking", "Contribution chart", "Tolerance drill-down"}},
        {"toleranceOptimizer", "Tolerance Optimizer", "toleranceOptimizerWindow",
         "Explores cost and capability tradeoffs by adjusting tolerance ranges.",
         {"Cost objective setup", "Range constraints", "Optimization result table"}},
        {"sequenceOptimizer", "Sequence Optimizer", "sequenceOptimizerWindow",
         "Evaluates assembly sequence choices and their impact on variation.",
         {"Move grouping", "Trial settings", "Best sequence results"}},
        {"datumOptimizer", "Datum Optimizer", "datumOptimizerWindow",
         "Compares datum reference frame candidates and their measurement impact.",
         {"Candidate datum sets", "Result comparison", "Apply selected DRF"}},
        {"dofCounter", "DOF Counter", "dofCounterWindow",
         "Displays part degrees of freedom before and after mechanical constraints.",
         {"Move-by-move DOF matrix", "Under/over-constraint signals", "Constraint rank review"}},
        {"kinematics", "Kinematics", "kinematicsWindow",
         "Prepares mechanism motion studies for joints and constraints.",
         {"Joint list", "Motion setup", "Kinematic simulation results"}},
        {"collisionDetection", "Collision Detection", "collisionDetectionWindow",
         "Creates clearance and part-distance checks from selected part sets.",
         {"Between-all checks", "First-to-all checks", "Clearance zone threshold"}},
        {"stiffGen", "StiffGen", "stiffGenWindow",
         "Prepares condensed stiffness, mass, and thermal files for compliant models.",
         {"ASET point selection", "Solver file paths", "Generated matrix summary"}},
        {"loadFeaData", "Load FEA Data", "loadFeaDataWindow",
         "Binds stiffness, mesh, mass, and thermal files to compliant parts.",
         {"Stiffness matrix binding", "Mesh and mass files", "Refresh ASET links"}},
        {"pointLinkWizard", "Point Link Wizard", "pointLinkWizardWindow",
         "Reviews and repairs DCS point to FEA node links.",
         {"Point-node distance table", "Manual link override", "Bulk unlink above threshold"}},
        {"validateCompliantModel", "Validate Compliant Model",
         "validateCompliantModelWindow",
         "Checks compliant model setup before flexible moves and analysis.",
         {"Missing file checks", "Point-link validation", "Constraint completeness"}},
        {"userDllManager", "User DLL Manager", "userDllManagerWindow",
         "Loads and reviews external C ABI routines for Move, Tolerance, and Measure calculations.",
         {"Add DLL", "Routine summary", "Move/Tolerance/Measure registration"}},
        {"welcome", "Welcome", "welcomeWindow",
         "Introduces the OpenDVA desktop workflow and quick-start actions.",
         {"New or open model", "Modeling workflow guide", "Help and examples"}},
        {"licenseStatus", "License Status", "licenseStatusWindow",
         "Shows local module availability and configured license source.",
         {"License source", "Module status", "Diagnostic output"}},
        {"systemInformation", "System Information", "systemInformationWindow",
         "Displays compute resources and runtime information relevant to simulation.",
         {"CPU/thread summary", "Qt runtime", "Build and report directories"}}};

    for (const ModuleWindowSpec& spec : specs) {
        if (windowId != QLatin1String(spec.id)) continue;
        auto* dialog = new ModuleOverviewDialog(
            spec.title, spec.objectName, spec.summary, spec.capabilities, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
        statusBar()->showMessage(QString("%1 opened").arg(spec.title));
        return;
    }

    QMessageBox::warning(this, "OpenDVA", "Unknown module window.");
}

void MainWindow::applyPreferences(const AppPreferences& preferences) {
    preferences_ = preferences;
    savePreferences();
    updateRecentFilesMenu();
    statusBar()->showMessage("Preferences updated");
}

void MainWindow::addPart() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Add Part", "DCS part name:",
                                               QLineEdit::Normal,
                                               QString("Part %1").arg(opendva::nextPartId(model_)),
                                               &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    opendva::addPart(model_, name.trimmed().toStdString());
    setDirty(true);
    refreshUi();
    statusBar()->showMessage("Part added");
}

void MainWindow::addPoint() {
    bool okX = false;
    const double x = QInputDialog::getDouble(this, "Add Point", "X:", 0.0, -1e9, 1e9,
                                            4, &okX);
    if (!okX) return;
    bool okY = false;
    const double y = QInputDialog::getDouble(this, "Add Point", "Y:", 0.0, -1e9, 1e9,
                                            4, &okY);
    if (!okY) return;
    bool okZ = false;
    const double z = QInputDialog::getDouble(this, "Add Point", "Z:", 0.0, -1e9, 1e9,
                                            4, &okZ);
    if (!okZ) return;

    const PointId id =
        opendva::addCoordinatePoint(model_, {x, y, z}, selectedPartContext());

    setDirty(true);
    refreshUi();
    statusBar()->showMessage(QString("Point %1 added").arg(id));
}

void MainWindow::addFeature() {
    Part& editablePart = opendva::ensureEditablePart(model_);
    if (editablePart.points.empty()) {
        QMessageBox::information(this, "Add Feature",
                                 "Add at least one point before creating a feature.");
        return;
    }

    const QStringList parts = partChoiceNames(model_);
    const int defaultPartIndex = partIndexOf(model_, selectedPartContext());
    bool okPart = false;
    const QString partChoice =
        QInputDialog::getItem(this, "Add Feature", "Part:",
                              parts, defaultPartIndex, false, &okPart);
    if (!okPart || partChoice.isEmpty()) return;

    const PartId partId = partIdAt(model_, parts.indexOf(partChoice));

    bool okType = false;
    const QStringList types = featureKindNames();
    const QString selected =
        QInputDialog::getItem(this, "Add Feature", "Type:",
                              types, 0, false, &okType);
    if (!okType || selected.isEmpty()) return;

    const FeatureId id =
        opendva::addFeature(model_, featureKindAt(types.indexOf(selected)), partId);
    if (id == kInvalidId) {
        QMessageBox::information(this, "Add Feature",
                                 "The selected part needs at least one point.");
        return;
    }

    setDirty(true);
    refreshUi();
    statusBar()->showMessage(QString("Feature %1 added").arg(id));
}

void MainWindow::addLinearTolerance() {
    opendva::ensureEditablePart(model_);
    const PartId partId = selectedPartContext();
    const Part* selectedPart = model_.findPart(partId);
    const Part& part = selectedPart != nullptr ? *selectedPart : model_.parts.front();
    if (part.points.empty()) {
        QMessageBox::information(this, "Add Linear Tolerance",
                                 "Add at least one point before creating a tolerance.");
        return;
    }

    bool ok = false;
    const double range = QInputDialog::getDouble(this, "Add Linear Tolerance",
                                                 "Range (Max - Min):", 1.0,
                                                 0.0, 1e9, 4, &ok);
    if (!ok) return;

    if (opendva::addLinearTolerance(model_, range, part.id) == kInvalidId) return;

    setDirty(true);
    refreshUi();
    statusBar()->showMessage("Linear tolerance added");
}

void MainWindow::addGdt() {
    opendva::ensureEditablePart(model_);
    const PartId partId = selectedPartContext();
    const Part* selectedPart = model_.findPart(partId);
    const Part& part = selectedPart != nullptr ? *selectedPart : model_.parts.front();
    if (part.features.empty()) {
        QMessageBox::information(this, "Add GD&T",
                                 "Add at least one point or feature before creating GD&T.");
        return;
    }

    bool okType = false;
    const QStringList types = gdtTypeNames();
    const QString selected = QInputDialog::getItem(this, "Add GD&T", "Type:",
                                                   types, 1, false, &okType);
    if (!okType || selected.isEmpty()) return;

    bool okRange = false;
    const double range = QInputDialog::getDouble(this, "Add GD&T",
                                                 "Zone Range:", 1.0,
                                                 0.0, 1e9, 4, &okRange);
    if (!okRange) return;

    bool okDiametrical = false;
    const QString zone = QInputDialog::getItem(this, "Add GD&T", "Tolerance zone:",
                                               QStringList{"Diametrical", "Non-diametrical"},
                                               0, false, &okDiametrical);
    if (!okDiametrical) return;

    const GdtType type = static_cast<GdtType>(types.indexOf(selected));
    const bool diametrical = zone == "Diametrical";
    if (opendva::addGdt(model_, type, range, diametrical, part.id) == kInvalidId) return;

    setDirty(true);
    refreshUi();
    statusBar()->showMessage("GD&T added");
}

void MainWindow::addTransformMove() {
    if (model_.parts.size() < 2) {
        QMessageBox::information(this, "Add Transform Move",
                                 "Add at least two parts before creating a move.");
        return;
    }

    const QStringList parts = partChoiceNames(model_);
    const PartId selectedPart = selectedPartContext();
    const int objectDefault = partIndexOf(model_, selectedPart);
    bool okObject = false;
    const QString objectChoice =
        QInputDialog::getItem(this, "Add Transform Move", "Object part:",
                              parts, objectDefault, false, &okObject);
    if (!okObject || objectChoice.isEmpty()) return;

    const PartId objectId = partIdAt(model_, parts.indexOf(objectChoice));
    const int targetDefault = firstOtherPartIndex(model_, objectId);
    bool okTarget = false;
    const QString targetChoice =
        QInputDialog::getItem(this, "Add Transform Move", "Target part:",
                              parts, targetDefault, false, &okTarget);
    if (!okTarget || targetChoice.isEmpty()) return;

    const PartId targetId = partIdAt(model_, parts.indexOf(targetChoice));
    if (objectId == targetId) {
        QMessageBox::information(this, "Add Transform Move",
                                 "Object and target parts must be different.");
        return;
    }

    bool okX = false;
    const double x = QInputDialog::getDouble(this, "Add Transform Move",
                                            "Translation X:", 10.0, -1e9, 1e9,
                                            4, &okX);
    if (!okX) return;
    bool okY = false;
    const double y = QInputDialog::getDouble(this, "Add Transform Move",
                                            "Translation Y:", 0.0, -1e9, 1e9,
                                            4, &okY);
    if (!okY) return;
    bool okZ = false;
    const double z = QInputDialog::getDouble(this, "Add Transform Move",
                                            "Translation Z:", 0.0, -1e9, 1e9,
                                            4, &okZ);
    if (!okZ) return;

    const MoveId id = opendva::addTransformMove(model_, {x, y, z}, objectId, targetId);
    if (id == kInvalidId) return;

    setDirty(true);
    refreshUi();
    statusBar()->showMessage(QString("Transform move %1 added").arg(id));
}

void MainWindow::addPointPointMeasure() {
    const PointId selectedPoint = selectedPointContext();
    const PartId selectedPart = selectedPartContext();
    MeasureId id = opendva::addPointPointMeasure(model_);
    if (id == kInvalidId) {
        QMessageBox::information(this, "Add Point-Point Measure",
                                 "Add at least two points before creating a measure.");
        return;
    }

    const std::vector<PointId> points =
        orderedMeasurePoints(model_, selectedPart, selectedPoint);
    if (points.size() >= 2) {
        opendva::setMeasureInputPoints(model_, id, {points[0], points[1]});
    }

    setDirty(true);
    refreshUi();
    statusBar()->showMessage("Point-point measure added");
}

void MainWindow::moveSelectedMoveUp() {
    QTreeWidgetItem* current = navigator_->currentItem();
    if (!current ||
        static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt()) !=
            NavigatorNodeKind::Move) {
        statusBar()->showMessage("Select a move to reorder");
        return;
    }

    const MoveId id = current->data(0, kNodeIdRole).toULongLong();
    for (std::size_t index = 0; index < model_.moves.size(); ++index) {
        if (model_.moves[index].id == id) {
            if (index == 0) {
                statusBar()->showMessage("Move is already first");
                return;
            }
            if (!opendva::reorderMove(model_, id, index - 1)) return;
            setDirty(true);
            refreshUi();
            restoreNavigatorSelection(navigator_, NavigatorNodeKind::Move, id,
                                      QString());
            statusBar()->showMessage("Move reordered");
            return;
        }
    }
}

void MainWindow::moveSelectedMoveDown() {
    QTreeWidgetItem* current = navigator_->currentItem();
    if (!current ||
        static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt()) !=
            NavigatorNodeKind::Move) {
        statusBar()->showMessage("Select a move to reorder");
        return;
    }

    const MoveId id = current->data(0, kNodeIdRole).toULongLong();
    for (std::size_t index = 0; index < model_.moves.size(); ++index) {
        if (model_.moves[index].id == id) {
            if (index + 1 >= model_.moves.size()) {
                statusBar()->showMessage("Move is already last");
                return;
            }
            if (!opendva::reorderMove(model_, id, index + 1)) return;
            setDirty(true);
            refreshUi();
            restoreNavigatorSelection(navigator_, NavigatorNodeKind::Move, id,
                                      QString());
            statusBar()->showMessage("Move reordered");
            return;
        }
    }
}

void MainWindow::deleteSelectedNavigatorItem() {
    QTreeWidgetItem* current = navigator_->currentItem();
    if (!current) {
        statusBar()->showMessage("Select an item to delete");
        return;
    }

    const auto kind =
        static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt());
    const std::uint64_t id = current->data(0, kNodeIdRole).toULongLong();
    const QString label = current->text(0);

    QString typeName;
    switch (kind) {
        case NavigatorNodeKind::Part: typeName = "part"; break;
        case NavigatorNodeKind::Point: typeName = "point"; break;
        case NavigatorNodeKind::Feature: typeName = "feature"; break;
        case NavigatorNodeKind::Tolerance: typeName = "tolerance"; break;
        case NavigatorNodeKind::Gdt: typeName = "GD&T"; break;
        case NavigatorNodeKind::Move: typeName = "move"; break;
        case NavigatorNodeKind::Measure: typeName = "measure"; break;
        case NavigatorNodeKind::Variant:
            deleteModelVariant();
            return;
        case NavigatorNodeKind::Root:
        case NavigatorNodeKind::Category:
        default:
            statusBar()->showMessage("Select a model object to delete");
            return;
    }

    if (QMessageBox::question(this, "Delete Model Object",
                              QString("Delete %1 \"%2\"? Dependent references "
                                      "will be removed.")
                                  .arg(typeName, label)) != QMessageBox::Yes) {
        return;
    }

    bool deleted = false;
    switch (kind) {
        case NavigatorNodeKind::Part:
            deleted = opendva::deletePart(model_, id);
            break;
        case NavigatorNodeKind::Point:
            deleted = opendva::deletePoint(model_, id);
            break;
        case NavigatorNodeKind::Feature:
            deleted = opendva::deleteFeature(model_, id);
            break;
        case NavigatorNodeKind::Tolerance:
            deleted = opendva::deleteTolerance(model_, id);
            break;
        case NavigatorNodeKind::Gdt:
            deleted = opendva::deleteGdt(model_, id);
            break;
        case NavigatorNodeKind::Move:
            deleted = opendva::deleteMove(model_, id);
            break;
        case NavigatorNodeKind::Measure:
            deleted = opendva::deleteMeasure(model_, id);
            break;
        default:
            break;
    }

    if (!deleted) {
        statusBar()->showMessage("Selected item could not be deleted");
        return;
    }

    setDirty(true);
    refreshUi();
    statusBar()->showMessage(QString("Deleted %1").arg(typeName));
}

void MainWindow::captureModelVariant() {
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Capture Model Variant",
                                               "Variant name:",
                                               QLineEdit::Normal,
                                               defaultVariantName(model_),
                                               &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    const ModelVariant variant =
        opendva::captureActiveVariant(model_, name.trimmed().toStdString());
    if (!opendva::addOrReplaceModelVariant(model_, variant)) return;

    setDirty(true);
    refreshUi();
    statusBar()->showMessage(QString("Model variant captured: %1").arg(name.trimmed()));
}

void MainWindow::applyModelVariant() {
    const QStringList names = variantNames(model_);
    if (names.isEmpty()) {
        QMessageBox::information(this, "Apply Model Variant",
                                 "No model variants have been captured.");
        return;
    }

    bool ok = false;
    const QString selected = QInputDialog::getItem(this, "Apply Model Variant",
                                                   "Variant:", names, 0, false, &ok);
    if (!ok || selected.isEmpty()) return;

    if (!opendva::applyModelVariant(model_, selected.toStdString())) {
        QMessageBox::warning(this, "Apply Model Variant",
                             "Could not apply the selected model variant.");
        return;
    }

    setDirty(true);
    refreshUi();
    statusBar()->showMessage(QString("Model variant applied: %1").arg(selected));
}

void MainWindow::deleteModelVariant() {
    const QStringList names = variantNames(model_);
    if (names.isEmpty()) {
        QMessageBox::information(this, "Delete Model Variant",
                                 "No model variants have been captured.");
        return;
    }

    bool ok = false;
    const QString selected = QInputDialog::getItem(this, "Delete Model Variant",
                                                   "Variant:", names, 0, false, &ok);
    if (!ok || selected.isEmpty()) return;

    if (QMessageBox::question(this, "Delete Model Variant",
                              QString("Delete model variant \"%1\"?").arg(selected)) !=
        QMessageBox::Yes) {
        return;
    }

    if (!opendva::deleteModelVariant(model_, selected.toStdString())) return;

    setDirty(true);
    refreshUi();
    statusBar()->showMessage(QString("Model variant deleted: %1").arg(selected));
}

void MainWindow::populateNavigatorContextMenu(QMenu& menu,
                                              NavigatorNodeKind selectedKind) {
    const bool moveSelected = selectedKind == NavigatorNodeKind::Move;
    const bool deletable = selectedKind == NavigatorNodeKind::Part ||
                           selectedKind == NavigatorNodeKind::Point ||
                           selectedKind == NavigatorNodeKind::Feature ||
                           selectedKind == NavigatorNodeKind::Tolerance ||
                           selectedKind == NavigatorNodeKind::Gdt ||
                           selectedKind == NavigatorNodeKind::Move ||
                           selectedKind == NavigatorNodeKind::Measure ||
                           selectedKind == NavigatorNodeKind::Variant;
    QAction* addPartAction = menu.addAction("Add Part", this, &MainWindow::addPart);
    addPartAction->setObjectName("addPartContextAction");
    QAction* addPointAction = menu.addAction("Add Point", this, &MainWindow::addPoint);
    addPointAction->setObjectName("addPointContextAction");
    QAction* addFeatureAction =
        menu.addAction("Add Feature", this, &MainWindow::addFeature);
    addFeatureAction->setObjectName("addFeatureContextAction");
    QAction* addLinearToleranceAction =
        menu.addAction("Add Linear Tolerance", this,
                       &MainWindow::addLinearTolerance);
    addLinearToleranceAction->setObjectName("addLinearToleranceContextAction");
    QAction* addGdtAction = menu.addAction("Add GD&&T", this, &MainWindow::addGdt);
    addGdtAction->setObjectName("addGdtContextAction");
    QAction* addTransformMoveAction =
        menu.addAction("Add Transform Move", this, &MainWindow::addTransformMove);
    addTransformMoveAction->setObjectName("addTransformMoveContextAction");
    QAction* addPointPointMeasureAction =
        menu.addAction("Add Point-Point Measure", this,
                       &MainWindow::addPointPointMeasure);
    addPointPointMeasureAction->setObjectName("addPointPointMeasureContextAction");
    menu.addSeparator();
    QAction* moveUp = menu.addAction("Move Up", this, &MainWindow::moveSelectedMoveUp);
    moveUp->setObjectName("moveSelectedMoveUpContextAction");
    QAction* moveDown =
        menu.addAction("Move Down", this, &MainWindow::moveSelectedMoveDown);
    moveDown->setObjectName("moveSelectedMoveDownContextAction");
    moveUp->setEnabled(moveSelected);
    moveDown->setEnabled(moveSelected);
    QAction* deleteItem =
        menu.addAction("Delete Selected", this, &MainWindow::deleteSelectedNavigatorItem);
    deleteItem->setObjectName("deleteSelectedContextAction");
    deleteItem->setEnabled(deletable);
    menu.addSeparator();
    QAction* captureVariantAction =
        menu.addAction("Capture Model Variant", this,
                       &MainWindow::captureModelVariant);
    captureVariantAction->setObjectName("captureModelVariantContextAction");
    QAction* applyVariantAction =
        menu.addAction("Apply Model Variant", this, &MainWindow::applyModelVariant);
    applyVariantAction->setObjectName("applyModelVariantContextAction");
    QAction* deleteVariantAction =
        menu.addAction("Delete Model Variant", this,
                       &MainWindow::deleteModelVariant);
    deleteVariantAction->setObjectName("deleteModelVariantContextAction");
}

void MainWindow::showNavigatorMenu(const QPoint& position) {
    QMenu menu(this);
    QTreeWidgetItem* current = navigator_->itemAt(position);
    if (current) navigator_->setCurrentItem(current);
    const auto selectedKind = current
        ? static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt())
        : NavigatorNodeKind::Root;
    populateNavigatorContextMenu(menu, selectedKind);
    menu.exec(navigator_->viewport()->mapToGlobal(position));
}

void MainWindow::updateSelectionDetails(QTreeWidgetItem* current, QTreeWidgetItem*) {
    if (!current) {
        showModelSummary();
        return;
    }

    const auto kind =
        static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt());
    const std::uint64_t id = current->data(0, kNodeIdRole).toULongLong();
    const QString nodeName = current->data(0, kNodeNameRole).toString();

    updatingProperties_ = true;
    preparePropertyTable(propertyTable_);

    switch (kind) {
        case NavigatorNodeKind::Root:
            addPropertyRow(propertyTable_, "Type", "Assembly");
            addPropertyRow(propertyTable_, "Name",
                           QString::fromStdString(model_.assemblyName),
                           PropertyKey::AssemblyName, true);
            break;
        case NavigatorNodeKind::Part:
            addPropertyRow(propertyTable_, "Type", "Part");
            for (const Part& part : model_.parts) {
                if (part.id == id) {
                    addPropertyRow(propertyTable_, "ID", QString::number(part.id));
                    addPropertyRow(propertyTable_, "CAD Name",
                                   QString::fromStdString(part.cadName));
                    addPropertyRow(propertyTable_, "DCS Name",
                                   QString::fromStdString(part.dcsName),
                                   PropertyKey::PartDcsName, true);
                    addPropertyRow(propertyTable_, "Points",
                                   QString::number(part.points.size()));
                    addPropertyRow(propertyTable_, "Tolerances",
                                   QString::number(part.tolerances.size()));
                    break;
                }
            }
            break;
        case NavigatorNodeKind::Point:
            addPropertyRow(propertyTable_, "Type", "Point");
            for (const Part& part : model_.parts) {
                for (const Point& point : part.points) {
                    if (point.id == id) {
                        addPropertyRow(propertyTable_, "ID", QString::number(point.id));
                        addCheckPropertyRow(propertyTable_, "Active", point.active,
                                            PropertyKey::PointActive);
                        addPropertyRow(propertyTable_, "Point Type",
                                       pointKindName(point.kind),
                                       PropertyKey::PointKind, true);
                        addPropertyRow(propertyTable_, "Hole Type",
                                       holeTypeName(point.holeType),
                                       PropertyKey::PointHoleType, true);
                        addPropertyRow(propertyTable_, "X", numberText(point.position.x),
                                       PropertyKey::PointX, true);
                        addPropertyRow(propertyTable_, "Y", numberText(point.position.y),
                                       PropertyKey::PointY, true);
                        addPropertyRow(propertyTable_, "Z", numberText(point.position.z),
                                       PropertyKey::PointZ, true);
                        addPropertyRow(propertyTable_, "Diameter",
                                       numberText(point.diameter),
                                       PropertyKey::PointDiameter, true);
                        addPropertyRow(propertyTable_, "I", numberText(point.ijk.x),
                                       PropertyKey::PointI, true);
                        addPropertyRow(propertyTable_, "J", numberText(point.ijk.y),
                                       PropertyKey::PointJ, true);
                        addPropertyRow(propertyTable_, "K", numberText(point.ijk.z),
                                       PropertyKey::PointK, true);
                        break;
                    }
                }
            }
            break;
        case NavigatorNodeKind::Feature:
            addPropertyRow(propertyTable_, "Type", "Feature");
            for (const Part& part : model_.parts) {
                for (const Feature& feature : part.features) {
                    if (feature.id == id) {
                        addPropertyRow(propertyTable_, "ID",
                                       QString::number(feature.id));
                        addPropertyRow(propertyTable_, "Feature Type",
                                       featureKindName(feature.kind),
                                       PropertyKey::FeatureKind, true);
                        addPropertyRow(propertyTable_, "Defining Points",
                                       pointIdsText(feature.definingPoints),
                                       PropertyKey::FeatureDefiningPoints, true);
                        break;
                    }
                }
            }
            break;
        case NavigatorNodeKind::Tolerance:
            addPropertyRow(propertyTable_, "Type", "Tolerance");
            for (const Part& part : model_.parts) {
                for (const ToleranceDef& tol : part.tolerances) {
                    if (tol.id == id) {
                        addPropertyRow(propertyTable_, "ID", QString::number(tol.id));
                        addPropertyRow(propertyTable_, "Name",
                                       QString::fromStdString(tol.name),
                                       PropertyKey::ToleranceName, true);
                        addCheckPropertyRow(propertyTable_, "Active", tol.active,
                                            PropertyKey::ToleranceActive);
                        const bool hasRand = !tol.ir.rands.empty();
                        const RandSpec* rand = hasRand ? &tol.ir.rands.front() : nullptr;
                        const QString distribution =
                            rand ? distributionName(rand->distribution) : QString();
                        const QString range =
                            rand ? numberText(rand->range) : QString();
                        const QString offset =
                            rand ? numberText(rand->offset) : QString();
                        const QString sigmaNumber =
                            rand ? numberText(rand->sigmaNum) : QString();
                        addPropertyRow(propertyTable_, "Distribution", distribution,
                                       PropertyKey::ToleranceDistribution,
                                       hasRand);
                        addPropertyRow(propertyTable_, "Range", range,
                                       PropertyKey::ToleranceRange,
                                       hasRand);
                        addPropertyRow(propertyTable_, "Offset", offset,
                                       PropertyKey::ToleranceOffset,
                                       hasRand);
                        addPropertyRow(propertyTable_, "Sigma Number", sigmaNumber,
                                       PropertyKey::ToleranceSigmaNumber,
                                       hasRand);
                        addPropertyRow(propertyTable_, "Geom Rule",
                                       geomRuleName(tol.ir.geomRule),
                                       PropertyKey::ToleranceGeomRule, true);
                        addPropertyRow(propertyTable_, "Range Scale",
                                       numberText(tol.ir.rangeScale),
                                       PropertyKey::ToleranceRangeScale, true);
                        addPropertyRow(propertyTable_, "Direction I",
                                       numberText(tol.ir.direction.ijk.x),
                                       PropertyKey::ToleranceI, true);
                        addPropertyRow(propertyTable_, "Direction J",
                                       numberText(tol.ir.direction.ijk.y),
                                       PropertyKey::ToleranceJ, true);
                        addPropertyRow(propertyTable_, "Direction K",
                                       numberText(tol.ir.direction.ijk.z),
                                       PropertyKey::ToleranceK, true);
                        addCheckPropertyRow(propertyTable_, "Truncation Active",
                                            tol.ir.truncation.active,
                                            PropertyKey::ToleranceTruncationActive);
                        addPropertyRow(propertyTable_, "Min Truncation",
                                       numberText(tol.ir.truncation.minTrunc),
                                       PropertyKey::ToleranceMinTruncation, true);
                        addPropertyRow(propertyTable_, "Max Truncation",
                                       numberText(tol.ir.truncation.maxTrunc),
                                       PropertyKey::ToleranceMaxTruncation, true);
                        addPropertyRow(propertyTable_, "Target Features",
                                       featureIdsText(tol.features),
                                       PropertyKey::ToleranceFeatures, true);
                        addPropertyRow(propertyTable_, "Random Variables",
                                       randSpecsText(tol.ir.rands),
                                       PropertyKey::ToleranceRandomVariables, true);
                        break;
                    }
                }
            }
            break;
        case NavigatorNodeKind::Gdt:
            addPropertyRow(propertyTable_, "Type", "GD&T");
            for (const Part& part : model_.parts) {
                for (const GdtDef& gdt : part.gdts) {
                    if (gdt.id == id) {
                        addPropertyRow(propertyTable_, "ID", QString::number(gdt.id));
                        addPropertyRow(propertyTable_, "Name",
                                       QString::fromStdString(gdt.name),
                                       PropertyKey::GdtName, true);
                        addCheckPropertyRow(propertyTable_, "Active", gdt.active,
                                            PropertyKey::GdtActive);
                        addPropertyRow(propertyTable_, "GD&T Type", gdtTypeName(gdt.type),
                                       PropertyKey::GdtType, true);
                        addPropertyRow(propertyTable_, "Range", numberText(gdt.range),
                                       PropertyKey::GdtRange, true);
                        addCheckPropertyRow(propertyTable_, "Diametrical",
                                            gdt.diametrical,
                                            PropertyKey::GdtDiametrical);
                        addPropertyRow(propertyTable_, "DRF Primary",
                                       QString::number(gdt.drf.primary),
                                       PropertyKey::GdtDrfPrimary, true);
                        addPropertyRow(propertyTable_, "DRF Secondary",
                                       QString::number(gdt.drf.secondary),
                                       PropertyKey::GdtDrfSecondary, true);
                        addPropertyRow(propertyTable_, "DRF Tertiary",
                                       QString::number(gdt.drf.tertiary),
                                       PropertyKey::GdtDrfTertiary, true);
                        addPropertyRow(propertyTable_, "Controlled Features",
                                       featureIdsText(gdt.features),
                                       PropertyKey::GdtFeatures, true);
                        break;
                    }
                }
            }
            break;
        case NavigatorNodeKind::Move:
            addPropertyRow(propertyTable_, "Type", "Move");
            for (const MoveDef& move : model_.moves) {
                if (move.id == id) {
                    addPropertyRow(propertyTable_, "ID", QString::number(move.id));
                    addPropertyRow(propertyTable_, "Name",
                                   QString::fromStdString(move.name),
                                   PropertyKey::MoveName, true);
                    addCheckPropertyRow(propertyTable_, "Active", move.active,
                                        PropertyKey::MoveActive);
                    addCheckPropertyRow(propertyTable_, "Nominal Build",
                                        move.inputs.isNominalBuild,
                                        PropertyKey::MoveNominalBuild);
                    addPropertyRow(propertyTable_, "Move Type",
                                   moveTypeName(move.inputs.type),
                                   PropertyKey::MoveType, true);
                    addPropertyRow(propertyTable_, "User DLL Routine",
                                   QString::fromStdString(move.inputs.userDllRoutine),
                                   PropertyKey::MoveUserDllRoutine, true);
                    addPropertyRow(propertyTable_, "Search Accuracy",
                                   numberText(move.inputs.searchAccuracy),
                                   PropertyKey::MoveSearchAccuracy, true);
                    addPropertyRow(propertyTable_, "Max Iterations",
                                   QString::number(move.inputs.maxIterations),
                                   PropertyKey::MoveMaxIterations, true);
                    addCheckPropertyRow(propertyTable_, "Float Active",
                                        move.inputs.hole_pin_float.active,
                                        PropertyKey::MoveFloatActive);
                    addPropertyRow(propertyTable_, "Float Sigma Number",
                                   QString::number(
                                       move.inputs.hole_pin_float.sigmaNumber),
                                   PropertyKey::MoveFloatSigmaNumber, true);
                    addPropertyRow(propertyTable_, "Float Range Scale",
                                   numberText(
                                       move.inputs.hole_pin_float.rangeScale),
                                   PropertyKey::MoveFloatRangeScale, true);
                    addPropertyRow(propertyTable_, "Float Angle Range",
                                   numberText(
                                       move.inputs.hole_pin_float.angleRangeDeg),
                                   PropertyKey::MoveFloatAngleRange, true);
                    addPropertyRow(propertyTable_, "Float Angle Offset",
                                   numberText(
                                       move.inputs.hole_pin_float.angleOffsetDeg),
                                   PropertyKey::MoveFloatAngleOffset, true);
                    addPropertyRow(propertyTable_, "Pairs",
                                   movePairsText(move.inputs.pairs),
                                   PropertyKey::MovePairs, true);
                    if (!move.inputs.pairs.empty()) {
                        const Vec3 objectPoint =
                            move.inputs.pairs.front().objectPoint;
                        const Vec3 targetPoint =
                            move.inputs.pairs.front().targetPoint;
                        const Vec3 direction =
                            move.inputs.pairs.front().direction.ijk;
                        addPropertyRow(propertyTable_, "Object X",
                                       numberText(objectPoint.x),
                                       PropertyKey::MoveObjectX, true);
                        addPropertyRow(propertyTable_, "Object Y",
                                       numberText(objectPoint.y),
                                       PropertyKey::MoveObjectY, true);
                        addPropertyRow(propertyTable_, "Object Z",
                                       numberText(objectPoint.z),
                                       PropertyKey::MoveObjectZ, true);
                        addPropertyRow(propertyTable_, "Target X",
                                       numberText(targetPoint.x),
                                       PropertyKey::MoveTargetX, true);
                        addPropertyRow(propertyTable_, "Target Y",
                                       numberText(targetPoint.y),
                                       PropertyKey::MoveTargetY, true);
                        addPropertyRow(propertyTable_, "Target Z",
                                       numberText(targetPoint.z),
                                       PropertyKey::MoveTargetZ, true);
                        addPropertyRow(propertyTable_, "Direction I",
                                       numberText(direction.x),
                                       PropertyKey::MoveDirectionI, true);
                        addPropertyRow(propertyTable_, "Direction J",
                                       numberText(direction.y),
                                       PropertyKey::MoveDirectionJ, true);
                        addPropertyRow(propertyTable_, "Direction K",
                                       numberText(direction.z),
                                       PropertyKey::MoveDirectionK, true);
                    }
                    addPropertyRow(propertyTable_, "Move Parts",
                                   partIdsText(move.moveParts),
                                   PropertyKey::MoveParts, true);
                    if (move.inputs.type == MoveType::Transform &&
                        !move.inputs.pairs.empty()) {
                        const Vec3 translation = move.inputs.pairs.front().targetPoint;
                        addPropertyRow(propertyTable_, "Translation X",
                                       numberText(translation.x),
                                       PropertyKey::MoveTranslationX, true);
                        addPropertyRow(propertyTable_, "Translation Y",
                                       numberText(translation.y),
                                       PropertyKey::MoveTranslationY, true);
                        addPropertyRow(propertyTable_, "Translation Z",
                                       numberText(translation.z),
                                       PropertyKey::MoveTranslationZ, true);
                    }
                    break;
                }
            }
            break;
        case NavigatorNodeKind::Measure:
            addPropertyRow(propertyTable_, "Type", "Measure");
            for (const MeasureRecord& measure : model_.measures) {
                if (measure.id == id) {
                    addPropertyRow(propertyTable_, "ID", QString::number(measure.id));
                    addPropertyRow(propertyTable_, "Name",
                                   QString::fromStdString(measure.name),
                                   PropertyKey::MeasureName, true);
                    addPropertyRow(propertyTable_, "Measure Type",
                                   measureTypeName(measure.def.type),
                                   PropertyKey::MeasureType, true);
                    addCheckPropertyRow(propertyTable_, "Active", measure.def.active,
                                        PropertyKey::MeasureActive);
                    addCheckPropertyRow(propertyTable_, "Output", measure.def.asOutput,
                                        PropertyKey::MeasureAsOutput);
                    addPropertyRow(propertyTable_, "Input Points",
                                   pointIdsText(measure.def.inputPoints),
                                   PropertyKey::MeasureInputPoints, true);
                    addPropertyRow(propertyTable_, "Input Features",
                                   featureIdsText(measure.def.inputFeatures),
                                   PropertyKey::MeasureInputFeatures, true);
                    addPropertyRow(propertyTable_, "Spec Mode",
                                   specModeName(measure.def.spec.mode),
                                   PropertyKey::MeasureSpecMode, true);
                    addPropertyRow(propertyTable_, "Direction Mode",
                                   directionModeName(measure.def.dirMode),
                                   PropertyKey::MeasureDirectionMode, true);
                    addPropertyRow(propertyTable_, "Direction I",
                                   numberText(measure.def.direction.ijk.x),
                                   PropertyKey::MeasureI, true);
                    addPropertyRow(propertyTable_, "Direction J",
                                   numberText(measure.def.direction.ijk.y),
                                   PropertyKey::MeasureJ, true);
                    addPropertyRow(propertyTable_, "Direction K",
                                   numberText(measure.def.direction.ijk.z),
                                   PropertyKey::MeasureK, true);
                    addPropertyRow(propertyTable_, "Scale",
                                   numberText(measure.def.scale),
                                   PropertyKey::MeasureScale, true);
                    const QString equationLabel =
                        measure.def.type == MeasureType::UserDll
                            ? "User DLL Routine"
                            : "Equation";
                    addPropertyRow(propertyTable_, equationLabel,
                                   QString::fromStdString(measure.def.equation),
                                   PropertyKey::MeasureEquation, true);
                    addPropertyRow(propertyTable_, "Values",
                                   valuesText(measure.def.values),
                                   PropertyKey::MeasureValues, true);
                    addCheckPropertyRow(propertyTable_, "LSL Active",
                                        measure.def.spec.lslActive,
                                        PropertyKey::MeasureLslActive);
                    addPropertyRow(propertyTable_, "LSL", numberText(measure.def.spec.lsl),
                                   PropertyKey::MeasureLsl, true);
                    addCheckPropertyRow(propertyTable_, "USL Active",
                                        measure.def.spec.uslActive,
                                        PropertyKey::MeasureUslActive);
                    addPropertyRow(propertyTable_, "USL", numberText(measure.def.spec.usl),
                                   PropertyKey::MeasureUsl, true);
                    break;
                }
            }
            break;
        case NavigatorNodeKind::Variant:
            addPropertyRow(propertyTable_, "Type", "Model Variant");
            for (const ModelVariant& variant : model_.variants) {
                if (QString::fromStdString(variant.name) == nodeName) {
                    addPropertyRow(propertyTable_, "Name",
                                   QString::fromStdString(variant.name),
                                   PropertyKey::VariantName, true);
                    addCheckPropertyRow(propertyTable_, "Active", variant.active,
                                        PropertyKey::VariantActive);
                    addPropertyRow(propertyTable_, "Moves",
                                   moveIdsText(variant.moves),
                                   PropertyKey::VariantMoves, true);
                    addPropertyRow(propertyTable_, "Tolerances",
                                   toleranceIdsText(variant.tolerances),
                                   PropertyKey::VariantTolerances, true);
                    addPropertyRow(propertyTable_, "Measures",
                                   measureIdsText(variant.measures),
                                   PropertyKey::VariantMeasures, true);
                    break;
                }
            }
            break;
        case NavigatorNodeKind::Category:
        default:
            addPropertyRow(propertyTable_, "Type", "Category");
            break;
    }
    propertyTable_->resizeColumnsToContents();
    updatingProperties_ = false;
}

void MainWindow::refreshUi() {
    populateNavigator();
    viewport_->setModel(model_);
    showModelSummary();
    updateWindowTitle();
}

bool MainWindow::syncMoveOrderFromNavigator(MoveId selectedMove) {
    if (navigator_ == nullptr || navigator_->topLevelItemCount() == 0) {
        return false;
    }
    QTreeWidgetItem* moves =
        childCategory(navigator_->topLevelItem(0), "Moves");
    if (moves == nullptr ||
        static_cast<std::size_t>(moves->childCount()) != model_.moves.size()) {
        refreshUi();
        return false;
    }

    std::set<MoveId> modelMoveIds;
    for (const MoveDef& move : model_.moves) {
        modelMoveIds.insert(move.id);
    }

    std::vector<MoveId> visualOrder;
    for (int index = 0; index < moves->childCount(); ++index) {
        QTreeWidgetItem* child = moves->child(index);
        if (!isMoveItem(child)) {
            refreshUi();
            return false;
        }
        const MoveId id = child->data(0, kNodeIdRole).toULongLong();
        if (modelMoveIds.find(id) == modelMoveIds.end()) {
            refreshUi();
            return false;
        }
        visualOrder.push_back(id);
    }

    std::set<MoveId> uniqueVisualOrder(visualOrder.begin(), visualOrder.end());
    if (uniqueVisualOrder.size() != visualOrder.size()) {
        refreshUi();
        return false;
    }

    bool changed = false;
    for (std::size_t index = 0; index < visualOrder.size(); ++index) {
        if (model_.moves[index].id != visualOrder[index]) {
            if (!opendva::reorderMove(model_, visualOrder[index], index)) {
                refreshUi();
                return false;
            }
            changed = true;
        }
    }

    if (changed) {
        setDirty(true);
    }
    refreshUi();
    restoreNavigatorSelection(navigator_, NavigatorNodeKind::Move, selectedMove,
                              QString());
    statusBar()->showMessage(changed ? "Move reordered" : "Move order unchanged");
    return true;
}

void MainWindow::updateWindowTitle() {
    QString title = "OpenDVA";
    if (!currentPath_.isEmpty()) title += " - " + currentPath_;
    else title += " - " + QString::fromStdString(model_.assemblyName);
    if (dirty_) title += " *";
    setWindowTitle(title);
}

void MainWindow::setDirty(bool dirty) {
    dirty_ = dirty;
    updateWindowTitle();
}

bool MainWindow::maybeSaveDirtyModel() {
    if (!dirty_) return true;

    const QMessageBox::StandardButton choice =
        QMessageBox::warning(this, "Unsaved changes",
                             "The current model has unsaved changes.",
                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Discard) return true;

    saveModel();
    return !dirty_;
}

bool MainWindow::saveModelToPath(const QString& path) {
    if (!opendva::saveModel(model_, path.toStdString())) {
        QMessageBox::warning(this, "Save failed", "Could not write the OpenDVA model.");
        return false;
    }
    currentPath_ = path;
    setDirty(false);
    recordRecentModelPath(path);
    statusBar()->showMessage(QString("Saved %1").arg(path));
    return true;
}

PartId MainWindow::selectedPartContext() const {
    QTreeWidgetItem* current = navigator_ ? navigator_->currentItem() : nullptr;
    for (QTreeWidgetItem* item = current; item != nullptr; item = item->parent()) {
        const auto kind =
            static_cast<NavigatorNodeKind>(item->data(0, kNodeKindRole).toInt());
        const std::uint64_t id = item->data(0, kNodeIdRole).toULongLong();
        switch (kind) {
            case NavigatorNodeKind::Part:
                return id;
            case NavigatorNodeKind::Point:
                return opendva::owningPartOfPoint(model_, id);
            case NavigatorNodeKind::Feature:
                return opendva::owningPartOfFeature(model_, id);
            case NavigatorNodeKind::Tolerance:
                return opendva::owningPartOfTolerance(model_, id);
            case NavigatorNodeKind::Gdt:
                return opendva::owningPartOfGdt(model_, id);
            case NavigatorNodeKind::Category:
            case NavigatorNodeKind::Root:
            case NavigatorNodeKind::Move:
            case NavigatorNodeKind::Measure:
            case NavigatorNodeKind::Variant:
            default:
                break;
        }
    }
    return model_.parts.empty() ? kInvalidId : model_.parts.front().id;
}

PointId MainWindow::selectedPointContext() const {
    QTreeWidgetItem* current = navigator_ ? navigator_->currentItem() : nullptr;
    if (current == nullptr) return kInvalidId;

    const auto kind =
        static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt());
    if (kind != NavigatorNodeKind::Point) return kInvalidId;

    const PointId id = current->data(0, kNodeIdRole).toULongLong();
    return opendva::owningPartOfPoint(model_, id) != kInvalidId ? id
                                                                : kInvalidId;
}

void MainWindow::showModelSummary() {
    std::size_t points = 0;
    std::size_t features = 0;
    std::size_t tolerances = 0;
    std::size_t gdts = 0;
    for (const Part& part : model_.parts) {
        points += part.points.size();
        features += part.features.size();
        tolerances += part.tolerances.size();
        gdts += part.gdts.size();
    }

    updatingProperties_ = true;
    preparePropertyTable(propertyTable_);
    addPropertyRow(propertyTable_, "Assembly",
                   QString::fromStdString(model_.assemblyName),
                   PropertyKey::AssemblyName, true);
    addPropertyRow(propertyTable_, "Parts", QString::number(model_.parts.size()));
    addPropertyRow(propertyTable_, "Points", QString::number(points));
    addPropertyRow(propertyTable_, "Features", QString::number(features));
    addPropertyRow(propertyTable_, "Tolerances", QString::number(tolerances));
    addPropertyRow(propertyTable_, "GD&T", QString::number(gdts));
    addPropertyRow(propertyTable_, "Moves", QString::number(model_.moves.size()));
    addPropertyRow(propertyTable_, "Measures", QString::number(model_.measures.size()));
    addPropertyRow(propertyTable_, "Variants", QString::number(model_.variants.size()));
    const QString activeVariant = activeVariantText(model_);
    addPropertyRow(propertyTable_, "Active Variant",
                   activeVariant.isEmpty() ? "None" : activeVariant);
    propertyTable_->resizeColumnsToContents();
    updatingProperties_ = false;
}

void MainWindow::handlePropertyEdited(QTableWidgetItem* item) {
    if (updatingProperties_ || !item || item->column() != 1) return;

    const auto key = static_cast<PropertyKey>(item->data(kPropertyKeyRole).toInt());
    if (key == PropertyKey::None) return;

    QTreeWidgetItem* current = navigator_->currentItem();
    const bool hadNavigatorSelection = current != nullptr;
    const auto kind = current
        ? static_cast<NavigatorNodeKind>(current->data(0, kNodeKindRole).toInt())
        : NavigatorNodeKind::Root;
    const std::uint64_t id = current ? current->data(0, kNodeIdRole).toULongLong() : 0;
    const QString nodeName =
        current ? current->data(0, kNodeNameRole).toString() : QString();
    QString restoreNodeName = nodeName;
    QTableWidgetItem* fieldItem = propertyTable_->item(item->row(), 0);
    const bool editingSummaryRow =
        fieldItem != nullptr && fieldItem->text() == "Assembly";

    bool changed = false;
    bool numericOk = true;
    const QString text = item->text().trimmed();
    bool ok = false;
    const double value = text.toDouble(&ok);

    switch (key) {
        case PropertyKey::AssemblyName:
            changed = renameAssembly(model_, text.toStdString());
            break;
        case PropertyKey::PartDcsName:
            if (kind == NavigatorNodeKind::Part) {
                changed = renamePart(model_, id, text.toStdString());
            }
            break;
        case PropertyKey::PointX:
        case PropertyKey::PointY:
        case PropertyKey::PointZ:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Point) {
                if (Point* point = findPoint(model_, id)) {
                    Vec3 pos = point->position;
                    if (key == PropertyKey::PointX) pos.x = value;
                    if (key == PropertyKey::PointY) pos.y = value;
                    if (key == PropertyKey::PointZ) pos.z = value;
                    changed = setPointPosition(model_, id, pos);
                }
            }
            break;
        case PropertyKey::PointActive:
            if (kind == NavigatorNodeKind::Point) {
                changed = setPointActive(model_, id, item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::PointKind:
            if (kind == NavigatorNodeKind::Point) {
                const std::optional<opendva::PointKind> parsed =
                    parsePointKind(text);
                if (parsed.has_value()) {
                    changed = setPointKind(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid point type");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::PointHoleType:
            if (kind == NavigatorNodeKind::Point) {
                const std::optional<opendva::HoleType> parsed = parseHoleType(text);
                if (parsed.has_value()) {
                    changed = setPointHoleType(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid point hole type");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::PointDiameter:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Point) {
                changed = setPointDiameter(model_, id, value);
            }
            break;
        case PropertyKey::PointI:
        case PropertyKey::PointJ:
        case PropertyKey::PointK:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Point) {
                if (Point* point = findPoint(model_, id)) {
                    Vec3 direction = point->ijk;
                    if (key == PropertyKey::PointI) direction.x = value;
                    if (key == PropertyKey::PointJ) direction.y = value;
                    if (key == PropertyKey::PointK) direction.z = value;
                    changed = setPointDirection(model_, id, direction);
                }
            }
            break;
        case PropertyKey::FeatureKind:
            if (kind == NavigatorNodeKind::Feature) {
                const std::optional<opendva::FeatureKind> parsed =
                    parseFeatureKind(text);
                if (parsed.has_value()) {
                    changed = setFeatureKind(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid feature type");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::FeatureDefiningPoints:
            if (kind == NavigatorNodeKind::Feature) {
                const std::optional<std::vector<PointId>> parsed =
                    parsePointIds(text);
                if (parsed.has_value()) {
                    changed = setFeatureDefiningPoints(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid feature defining point list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::ToleranceName:
            if (kind == NavigatorNodeKind::Tolerance) {
                changed = renameTolerance(model_, id, text.toStdString());
            }
            break;
        case PropertyKey::ToleranceActive:
            if (kind == NavigatorNodeKind::Tolerance) {
                changed = setToleranceActive(model_, id, item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::ToleranceDistribution:
            if (kind == NavigatorNodeKind::Tolerance) {
                const std::optional<opendva::DistributionType> parsed =
                    parseDistribution(text);
                if (parsed.has_value()) {
                    changed = setToleranceDistribution(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid distribution");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::ToleranceRange:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Tolerance) {
                changed = setToleranceRange(model_, id, value);
            }
            break;
        case PropertyKey::ToleranceOffset:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Tolerance) {
                changed = setToleranceOffset(model_, id, value);
            }
            break;
        case PropertyKey::ToleranceSigmaNumber:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Tolerance) {
                changed = setToleranceSigmaNumber(model_, id, value);
            }
            break;
        case PropertyKey::ToleranceGeomRule:
            if (kind == NavigatorNodeKind::Tolerance) {
                const std::optional<opendva::GeomRule> parsed = parseGeomRule(text);
                if (parsed.has_value()) {
                    changed = setToleranceGeomRule(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid geometry rule");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::ToleranceRangeScale:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Tolerance) {
                changed = setToleranceRangeScale(model_, id, value);
            }
            break;
        case PropertyKey::ToleranceI:
        case PropertyKey::ToleranceJ:
        case PropertyKey::ToleranceK:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Tolerance) {
                if (ToleranceDef* tolerance = findTolerance(model_, id)) {
                    Vec3 direction = tolerance->ir.direction.ijk;
                    if (key == PropertyKey::ToleranceI) direction.x = value;
                    if (key == PropertyKey::ToleranceJ) direction.y = value;
                    if (key == PropertyKey::ToleranceK) direction.z = value;
                    changed = setToleranceDirection(model_, id, direction);
                }
            }
            break;
        case PropertyKey::ToleranceTruncationActive:
            if (kind == NavigatorNodeKind::Tolerance) {
                if (ToleranceDef* tolerance = findTolerance(model_, id)) {
                    const Truncation& truncation = tolerance->ir.truncation;
                    changed = setToleranceTruncation(
                        model_, id, truncation.minTrunc, truncation.maxTrunc,
                        item->checkState() == Qt::Checked);
                }
            }
            break;
        case PropertyKey::ToleranceMinTruncation:
        case PropertyKey::ToleranceMaxTruncation:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Tolerance) {
                if (ToleranceDef* tolerance = findTolerance(model_, id)) {
                    Truncation truncation = tolerance->ir.truncation;
                    if (key == PropertyKey::ToleranceMinTruncation) {
                        truncation.minTrunc = value;
                    }
                    if (key == PropertyKey::ToleranceMaxTruncation) {
                        truncation.maxTrunc = value;
                    }
                    changed = setToleranceTruncation(
                        model_, id, truncation.minTrunc, truncation.maxTrunc,
                        truncation.active);
                }
            }
            break;
        case PropertyKey::ToleranceFeatures:
            if (kind == NavigatorNodeKind::Tolerance) {
                const std::optional<std::vector<FeatureId>> parsed =
                    parseFeatureIds(text);
                if (parsed.has_value()) {
                    changed = setToleranceFeatures(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid tolerance feature list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::ToleranceRandomVariables:
            if (kind == NavigatorNodeKind::Tolerance) {
                const std::optional<std::vector<RandSpec>> parsed =
                    parseRandSpecs(text);
                if (parsed.has_value()) {
                    changed = setToleranceRandomVariables(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid tolerance random variables");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::GdtName:
            if (kind == NavigatorNodeKind::Gdt) {
                changed = renameGdt(model_, id, text.toStdString());
            }
            break;
        case PropertyKey::GdtType:
            if (kind == NavigatorNodeKind::Gdt) {
                const std::optional<opendva::GdtType> parsed = parseGdtType(text);
                if (parsed.has_value()) {
                    changed = setGdtType(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid GD&T type");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::GdtActive:
            if (kind == NavigatorNodeKind::Gdt) {
                changed = setGdtActive(model_, id, item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::GdtRange:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Gdt) {
                changed = setGdtRange(model_, id, value);
            }
            break;
        case PropertyKey::GdtDiametrical:
            if (kind == NavigatorNodeKind::Gdt) {
                changed = setGdtDiametrical(model_, id,
                                            item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::GdtDrfPrimary:
        case PropertyKey::GdtDrfSecondary:
        case PropertyKey::GdtDrfTertiary:
            if (kind == NavigatorNodeKind::Gdt) {
                const std::optional<FeatureId> parsed = parseFeatureId(text);
                if (!parsed.has_value()) {
                    statusBar()->showMessage("Invalid DRF feature id");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
                if (GdtDef* gdt = findGdt(model_, id)) {
                    DatumReferenceFrame drf = gdt->drf;
                    if (key == PropertyKey::GdtDrfPrimary) {
                        drf.primary = parsed.value();
                    }
                    if (key == PropertyKey::GdtDrfSecondary) {
                        drf.secondary = parsed.value();
                    }
                    if (key == PropertyKey::GdtDrfTertiary) {
                        drf.tertiary = parsed.value();
                    }
                    changed = setGdtDrf(model_, id, drf);
                }
            }
            break;
        case PropertyKey::GdtFeatures:
            if (kind == NavigatorNodeKind::Gdt) {
                const std::optional<std::vector<FeatureId>> parsed =
                    parseFeatureIds(text);
                if (parsed.has_value()) {
                    changed = setGdtFeatures(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid controlled feature list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::MoveName:
            if (kind == NavigatorNodeKind::Move) {
                changed = renameMove(model_, id, text.toStdString());
            }
            break;
        case PropertyKey::MoveType:
            if (kind == NavigatorNodeKind::Move) {
                const std::optional<opendva::MoveType> parsed = parseMoveType(text);
                if (parsed.has_value()) {
                    changed = setMoveType(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid move type");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::MoveUserDllRoutine:
            if (kind == NavigatorNodeKind::Move) {
                changed = setMoveUserDllRoutine(model_, id, text.toStdString());
            }
            break;
        case PropertyKey::MoveActive:
            if (kind == NavigatorNodeKind::Move) {
                changed = setMoveActive(model_, id, item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::MoveNominalBuild:
            if (kind == NavigatorNodeKind::Move) {
                changed = setMoveNominalBuild(model_, id,
                                              item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::MovePairs:
            if (kind == NavigatorNodeKind::Move) {
                const std::optional<std::vector<MovePair>> parsed =
                    parseMovePairs(text);
                if (parsed.has_value()) {
                    changed = setMovePairs(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid move pair list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::MoveParts:
            if (kind == NavigatorNodeKind::Move) {
                const std::optional<std::vector<PartId>> parsed = parsePartIds(text);
                if (parsed.has_value()) {
                    changed = setMoveParts(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid move part list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::MoveSearchAccuracy:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Move) {
                changed = setMoveSearchAccuracy(model_, id, value);
            }
            break;
        case PropertyKey::MoveMaxIterations:
            if (kind == NavigatorNodeKind::Move) {
                bool intOk = false;
                const int intValue = text.toInt(&intOk);
                numericOk = intOk;
                if (numericOk) {
                    changed = setMoveMaxIterations(model_, id, intValue);
                }
            }
            break;
        case PropertyKey::MoveFloatActive:
            if (kind == NavigatorNodeKind::Move) {
                changed =
                    setMoveFloatActive(model_, id, item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::MoveFloatSigmaNumber:
            if (kind == NavigatorNodeKind::Move) {
                bool intOk = false;
                const int intValue = text.toInt(&intOk);
                numericOk = intOk;
                if (numericOk) {
                    changed = setMoveFloatSigmaNumber(model_, id, intValue);
                }
            }
            break;
        case PropertyKey::MoveFloatRangeScale:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Move) {
                changed = setMoveFloatRangeScale(model_, id, value);
            }
            break;
        case PropertyKey::MoveFloatAngleRange:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Move) {
                changed = setMoveFloatAngleRange(model_, id, value);
            }
            break;
        case PropertyKey::MoveFloatAngleOffset:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Move) {
                changed = setMoveFloatAngleOffset(model_, id, value);
            }
            break;
        case PropertyKey::MoveObjectX:
        case PropertyKey::MoveObjectY:
        case PropertyKey::MoveObjectZ:
        case PropertyKey::MoveTargetX:
        case PropertyKey::MoveTargetY:
        case PropertyKey::MoveTargetZ:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Move) {
                for (const MoveDef& move : model_.moves) {
                    if (move.id == id && !move.inputs.pairs.empty()) {
                        if (key == PropertyKey::MoveObjectX ||
                            key == PropertyKey::MoveObjectY ||
                            key == PropertyKey::MoveObjectZ) {
                            Vec3 point = move.inputs.pairs.front().objectPoint;
                            if (key == PropertyKey::MoveObjectX) point.x = value;
                            if (key == PropertyKey::MoveObjectY) point.y = value;
                            if (key == PropertyKey::MoveObjectZ) point.z = value;
                            changed = setMovePairObjectPoint(model_, id, 0, point);
                        } else {
                            Vec3 point = move.inputs.pairs.front().targetPoint;
                            if (key == PropertyKey::MoveTargetX) point.x = value;
                            if (key == PropertyKey::MoveTargetY) point.y = value;
                            if (key == PropertyKey::MoveTargetZ) point.z = value;
                            changed = setMovePairTargetPoint(model_, id, 0, point);
                        }
                        break;
                    }
                }
            }
            break;
        case PropertyKey::MoveDirectionI:
        case PropertyKey::MoveDirectionJ:
        case PropertyKey::MoveDirectionK:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Move) {
                for (const MoveDef& move : model_.moves) {
                    if (move.id == id && !move.inputs.pairs.empty()) {
                        Vec3 direction = move.inputs.pairs.front().direction.ijk;
                        if (key == PropertyKey::MoveDirectionI) direction.x = value;
                        if (key == PropertyKey::MoveDirectionJ) direction.y = value;
                        if (key == PropertyKey::MoveDirectionK) direction.z = value;
                        changed = setMovePairDirection(model_, id, 0, direction);
                        break;
                    }
                }
            }
            break;
        case PropertyKey::MoveTranslationX:
        case PropertyKey::MoveTranslationY:
        case PropertyKey::MoveTranslationZ:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Move) {
                for (const MoveDef& move : model_.moves) {
                    if (move.id == id && move.inputs.type == MoveType::Transform &&
                        !move.inputs.pairs.empty()) {
                        Vec3 translation = move.inputs.pairs.front().targetPoint;
                        if (key == PropertyKey::MoveTranslationX) translation.x = value;
                        if (key == PropertyKey::MoveTranslationY) translation.y = value;
                        if (key == PropertyKey::MoveTranslationZ) translation.z = value;
                        changed = setTransformMoveTranslation(model_, id, translation);
                        break;
                    }
                }
            }
            break;
        case PropertyKey::MeasureName:
            if (kind == NavigatorNodeKind::Measure) {
                changed = renameMeasure(model_, id, text.toStdString());
            }
            break;
        case PropertyKey::MeasureType:
            if (kind == NavigatorNodeKind::Measure) {
                const std::optional<opendva::MeasureType> parsed =
                    parseMeasureType(text);
                if (parsed.has_value()) {
                    changed = setMeasureType(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid measure type");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::MeasureActive:
            if (kind == NavigatorNodeKind::Measure) {
                changed = setMeasureActive(model_, id, item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::MeasureAsOutput:
            if (kind == NavigatorNodeKind::Measure) {
                changed =
                    setMeasureAsOutput(model_, id, item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::MeasureInputPoints:
            if (kind == NavigatorNodeKind::Measure) {
                const std::optional<std::vector<PointId>> parsed =
                    parsePointIds(text);
                if (parsed.has_value()) {
                    changed = setMeasureInputPoints(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid input point list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::MeasureInputFeatures:
            if (kind == NavigatorNodeKind::Measure) {
                const std::optional<std::vector<FeatureId>> parsed =
                    parseFeatureIds(text);
                if (parsed.has_value()) {
                    changed = setMeasureInputFeatures(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid input feature list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::MeasureSpecMode:
            if (kind == NavigatorNodeKind::Measure) {
                const std::optional<opendva::SpecMode> parsed = parseSpecMode(text);
                if (parsed.has_value()) {
                    changed = setMeasureSpecMode(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid spec mode");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::MeasureDirectionMode:
            if (kind == NavigatorNodeKind::Measure) {
                const std::optional<opendva::DirectionMode> parsed =
                    parseDirectionMode(text);
                if (parsed.has_value()) {
                    changed = setMeasureDirectionMode(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid direction mode");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::MeasureI:
        case PropertyKey::MeasureJ:
        case PropertyKey::MeasureK:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Measure) {
                if (MeasureRecord* measure = findMeasure(model_, id)) {
                    Vec3 direction = measure->def.direction.ijk;
                    if (key == PropertyKey::MeasureI) direction.x = value;
                    if (key == PropertyKey::MeasureJ) direction.y = value;
                    if (key == PropertyKey::MeasureK) direction.z = value;
                    changed = setMeasureDirection(model_, id, direction);
                }
            }
            break;
        case PropertyKey::MeasureScale:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Measure) {
                changed = setMeasureScale(model_, id, value);
            }
            break;
        case PropertyKey::MeasureEquation:
            if (kind == NavigatorNodeKind::Measure) {
                changed = setMeasureEquation(model_, id, text.toStdString());
            }
            break;
        case PropertyKey::MeasureValues:
            if (kind == NavigatorNodeKind::Measure) {
                const std::optional<std::vector<double>> parsed = parseValues(text);
                if (parsed.has_value()) {
                    changed = setMeasureValues(model_, id, parsed.value());
                } else {
                    statusBar()->showMessage("Invalid values list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::MeasureLslActive:
            if (kind == NavigatorNodeKind::Measure) {
                changed =
                    setMeasureLslActive(model_, id, item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::MeasureUslActive:
            if (kind == NavigatorNodeKind::Measure) {
                changed =
                    setMeasureUslActive(model_, id, item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::MeasureLsl:
        case PropertyKey::MeasureUsl:
            numericOk = ok;
            if (numericOk && kind == NavigatorNodeKind::Measure) {
                if (MeasureRecord* measure = findMeasure(model_, id)) {
                    double lsl = measure->def.spec.lsl;
                    double usl = measure->def.spec.usl;
                    if (key == PropertyKey::MeasureLsl) lsl = value;
                    if (key == PropertyKey::MeasureUsl) usl = value;
                    changed = setMeasureSpec(model_, id, lsl, usl,
                                             measure->def.spec.lslActive,
                                             measure->def.spec.uslActive);
                }
            }
            break;
        case PropertyKey::VariantName:
            if (kind == NavigatorNodeKind::Variant && !nodeName.isEmpty()) {
                changed = renameModelVariant(model_, nodeName.toStdString(),
                                             text.toStdString());
                if (changed) restoreNodeName = text;
            }
            break;
        case PropertyKey::VariantActive:
            if (kind == NavigatorNodeKind::Variant && !nodeName.isEmpty()) {
                changed = setModelVariantActive(model_, nodeName.toStdString(),
                                                item->checkState() == Qt::Checked);
            }
            break;
        case PropertyKey::VariantMoves:
            if (kind == NavigatorNodeKind::Variant && !nodeName.isEmpty()) {
                const std::optional<std::vector<MoveId>> parsed = parseMoveIds(text);
                if (parsed.has_value()) {
                    changed = setModelVariantMoves(model_, nodeName.toStdString(),
                                                   parsed.value());
                } else {
                    statusBar()->showMessage("Invalid variant move list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::VariantTolerances:
            if (kind == NavigatorNodeKind::Variant && !nodeName.isEmpty()) {
                const std::optional<std::vector<ToleranceId>> parsed =
                    parseToleranceIds(text);
                if (parsed.has_value()) {
                    changed =
                        setModelVariantTolerances(model_, nodeName.toStdString(),
                                                  parsed.value());
                } else {
                    statusBar()->showMessage("Invalid variant tolerance list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::VariantMeasures:
            if (kind == NavigatorNodeKind::Variant && !nodeName.isEmpty()) {
                const std::optional<std::vector<MeasureId>> parsed =
                    parseMeasureIds(text);
                if (parsed.has_value()) {
                    changed = setModelVariantMeasures(model_, nodeName.toStdString(),
                                                      parsed.value());
                } else {
                    statusBar()->showMessage("Invalid variant measure list");
                    updateSelectionDetails(current, nullptr);
                    return;
                }
            }
            break;
        case PropertyKey::None:
        default:
            break;
    }

    if (!numericOk) {
        statusBar()->showMessage("Invalid numeric value");
        updateSelectionDetails(current, nullptr);
        return;
    }

    if (changed) {
        setDirty(true);
        refreshUi();
        if (hadNavigatorSelection && !editingSummaryRow) {
            restoreNavigatorSelection(navigator_, kind, id, restoreNodeName);
            updateSelectionDetails(navigator_->currentItem(), nullptr);
        }
        statusBar()->showMessage("Property updated");
    } else {
        updateSelectionDetails(current, nullptr);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSaveDirtyModel()) {
        event->accept();
    } else {
        event->ignore();
    }
}

}  // namespace opendva::ui
