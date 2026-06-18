// Qt main-window property table coverage for the desktop editing surface.
#include <cmath>
#include <cstdio>
#include <string>

#include <QAction>
#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include "dva_test.h"
#include "opendva/domain/ModelEditing.h"
#include "opendva/domain/ModelSerializer.h"
#include "opendva/domain/ModelValidation.h"
#include "BatchProcessorDialog.h"
#include "ColorContourLegendWidget.h"
#include "MainWindow.h"
#include "ModelViewport.h"
#include "PreferencesDialog.h"
#include "RunAnalysisDialog.h"
#include "SimulationResultsDialog.h"
#include "ValidationResultsDialog.h"

using namespace opendva;

namespace {

int propertyRow(ui::MainWindow& window, const QString& label) {
    QTableWidget* table = window.propertyTableForTesting();
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem* field = table->item(row, 0);
        if (field != nullptr && field->text() == label) return row;
    }
    return -1;
}

QTreeWidgetItem* childByText(QTreeWidgetItem* parent, const QString& label) {
    if (parent == nullptr) return nullptr;
    for (int index = 0; index < parent->childCount(); ++index) {
        QTreeWidgetItem* child = parent->child(index);
        if (child != nullptr && child->text(0) == label) return child;
    }
    return nullptr;
}

QString normalizedActionText(const QString& text) {
    QString normalized;
    for (int index = 0; index < text.size(); ++index) {
        if (text[index] == QLatin1Char('&')) {
            if (index + 1 < text.size() &&
                text[index + 1] == QLatin1Char('&')) {
                normalized.append(QLatin1Char('&'));
                ++index;
            }
            continue;
        }
        normalized.append(text[index]);
    }
    return normalized;
}

QAction* actionByText(ui::MainWindow& window, const QString& label) {
    const QList<QAction*> actions = window.findChildren<QAction*>();
    for (QAction* action : actions) {
        if (action != nullptr && normalizedActionText(action->text()) == label) {
            return action;
        }
    }
    return nullptr;
}

Model modelWithUserDllMove() {
    Model model = createStarterModel();
    const PartId targetPart = addPart(model, "Target");
    (void)targetPart;
    const MoveId move = addTransformMove(model, {1.0, 2.0, 3.0},
                                         model.parts[0].id, model.parts[1].id);
    setMoveActive(model, move, false);
    setMoveType(model, move, MoveType::UserDll);
    setMoveUserDllRoutine(model, move, "externalMove");
    return model;
}

Model modelWithInactiveTransformMove() {
    Model model = createStarterModel();
    const PartId targetPart = addPart(model, "Target");
    (void)targetPart;
    const MoveId move = addTransformMove(model, {1.0, 2.0, 3.0},
                                         model.parts[0].id, model.parts[1].id);
    setMoveActive(model, move, false);
    return model;
}

Model modelWithUserDllMeasure() {
    Model model = createStarterModel();
    const MeasureId measure = model.measures.front().id;
    setMeasureActive(model, measure, false);
    setMeasureType(model, measure, MeasureType::UserDll);
    setMeasureEquation(model, measure, "externalMeasure");
    return model;
}

Model modelWithInactiveMeasure() {
    Model model = createStarterModel();
    const MeasureId measure = model.measures.front().id;
    setMeasureActive(model, measure, false);
    return model;
}

Model modelWithVariant() {
    Model model = createStarterModel();
    addOrReplaceModelVariant(model, captureActiveVariant(model, "Baseline"));
    return model;
}

Model modelWithTwoToleranceVariant() {
    Model model = createStarterModel();
    addLinearTolerance(model, 0.75, model.parts.front().id);
    addOrReplaceModelVariant(model, captureActiveVariant(model, "Baseline"));
    return model;
}

Model modelWithTwoMeasureVariant() {
    Model model = createStarterModel();
    addPointPointMeasure(model, 9.0, 11.0);
    addOrReplaceModelVariant(model, captureActiveVariant(model, "Baseline"));
    return model;
}

Model modelWithMoveVariant() {
    Model model = createStarterModel();
    const PartId targetPart = addPart(model, "Target");
    (void)targetPart;
    addTransformMove(model, {1.0, 2.0, 3.0}, model.parts[0].id,
                     model.parts[1].id);
    addOrReplaceModelVariant(model, captureActiveVariant(model, "Baseline"));
    return model;
}

Model modelWithTwoMoveVariant() {
    Model model = createStarterModel();
    const PartId targetPart = addPart(model, "Target");
    (void)targetPart;
    addTransformMove(model, {1.0, 2.0, 3.0}, model.parts[0].id,
                     model.parts[1].id);
    addTransformMove(model, {4.0, 5.0, 6.0}, model.parts[0].id,
                     model.parts[1].id);
    addOrReplaceModelVariant(model, captureActiveVariant(model, "Baseline"));
    return model;
}

Model modelWithEditablePoint() {
    return createStarterModel();
}

Model modelWithTwoFeatures() {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    addCoordinatePoint(model, {1.0, 0.0, 0.0}, partId);
    return model;
}

Model modelWithTwoParts() {
    Model model;
    addPart(model, "Base");
    addPart(model, "Target");
    return model;
}

Model modelWithDeactivatablePoint() {
    Model model;
    const PartId partId = addPart(model, "Part");
    addCoordinatePoint(model, {0.0, 0.0, 0.0}, partId);
    return model;
}

Model modelWithEditableGdt() {
    Model model = createStarterModel();
    const PartId partId = model.parts.front().id;
    addGdt(model, GdtType::Flatness, 0.3, false, partId);
    return model;
}

Model modelWithTwoGdt() {
    Model model = createStarterModel();
    const PartId partId = model.parts.front().id;
    addGdt(model, GdtType::Flatness, 0.3, false, partId);
    addGdt(model, GdtType::Straightness, 0.4, false, partId);
    return model;
}

Model modelWithEditableGdtDrf() {
    Model model = modelWithEditableGdt();
    setGdtActive(model, model.parts.front().gdts.front().id, false);
    return model;
}

Model modelWithEditableGdtDrfSecondary() {
    Model model = modelWithEditableGdtDrf();
    DatumReferenceFrame drf;
    drf.primary = model.parts.front().features[0].id;
    setGdtDrf(model, model.parts.front().gdts.front().id, drf);
    return model;
}

Model modelWithEditableGdtDrfTertiary() {
    Model model = modelWithEditableGdtDrfSecondary();
    const PartId partId = model.parts.front().id;
    addCoordinatePoint(model, {20.0, 0.0, 0.0}, partId);
    DatumReferenceFrame drf;
    drf.primary = model.parts.front().features[0].id;
    drf.secondary = model.parts.front().features[1].id;
    setGdtDrf(model, model.parts.front().gdts.front().id, drf);
    return model;
}

Model modelWithEditableTolerance() {
    Model model = createStarterModel();
    const PartId partId = model.parts.front().id;
    addLinearTolerance(model, 0.5, partId);
    return model;
}

}  // namespace

TEST("main window summary assembly rename keeps summary properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());

    const int row = propertyRow(window, "Assembly");
    dvatest::check(row >= 0, "summary properties include assembly name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "summary assembly value item exists");

    value->setText("Summary Assembly");

    const Model& model = window.modelForTesting();
    dvatest::check(model.assemblyName == "Summary Assembly",
                   "summary assembly edit is saved to the model");

    const int updatedRow = propertyRow(window, "Assembly");
    dvatest::check(updatedRow >= 0,
                   "summary properties stay selected after assembly rename");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated summary assembly value item exists");
    dvatest::check(updated->text().toStdString() == "Summary Assembly",
                   "updated summary assembly name remains visible");
}

TEST("main window summary count rows remain read only") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());

    const int assemblyRow = propertyRow(window, "Assembly");
    dvatest::check(assemblyRow >= 0, "summary properties include assembly");
    QTableWidgetItem* assembly =
        window.propertyTableForTesting()->item(assemblyRow, 1);
    dvatest::check(assembly != nullptr, "summary assembly value item exists");
    dvatest::check((assembly->flags() & Qt::ItemIsEditable) != 0,
                   "summary assembly value remains editable");

    const QStringList readOnlyRows = {
        "Parts", "Points", "Features", "Tolerances", "GD&T",
        "Moves", "Measures", "Variants", "Active Variant"};
    for (const QString& label : readOnlyRows) {
        const int row = propertyRow(window, label);
        dvatest::check(row >= 0,
                       QString("summary properties include %1").arg(label)
                           .toStdString());
        QTableWidgetItem* value =
            window.propertyTableForTesting()->item(row, 1);
        dvatest::check(value != nullptr,
                       QString("summary %1 value item exists").arg(label)
                           .toStdString());
        dvatest::check((value->flags() & Qt::ItemIsEditable) == 0,
                       QString("summary %1 value remains read only").arg(label)
                           .toStdString());
    }
}

TEST("main window assembly rename keeps assembly properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "assembly properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "assembly name value item exists");

    value->setText("Renamed Assembly");

    const Model& model = window.modelForTesting();
    dvatest::check(model.assemblyName == "Renamed Assembly",
                   "edited assembly name is saved to the model");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(updatedRow >= 0,
                   "assembly properties stay selected after rename");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated assembly name value item exists");
    dvatest::check(updated->text().toStdString() == "Renamed Assembly",
                   "updated assembly name remains visible");
}

TEST("main window unchanged assembly name edit refreshes assembly properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "assembly properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "assembly name value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(QString::fromStdString(model.assemblyName) == initialText,
                   "unchanged assembly name edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(
        updatedRow >= 0,
        "assembly properties stay selected after unchanged name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed assembly name value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged assembly name edit restores canonical value");
}

TEST("main window selected identity rows remain read only") {
    ui::MainWindow window;
    auto checkEditable = [&](const QString& label, bool editable) {
        const int row = propertyRow(window, label);
        dvatest::check(row >= 0,
                       QString("properties include %1").arg(label).toStdString());
        QTableWidgetItem* value =
            window.propertyTableForTesting()->item(row, 1);
        dvatest::check(value != nullptr,
                       QString("%1 value item exists").arg(label).toStdString());
        const bool actual = (value->flags() & Qt::ItemIsEditable) != 0;
        dvatest::check(actual == editable,
                       QString("%1 editability matches expectation")
                           .arg(label)
                           .toStdString());
    };
    auto checkSelectedIdentityRows = [&]() {
        checkEditable("Type", false);
        checkEditable("ID", false);
    };

    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");
    checkEditable("Type", false);
    checkEditable("Name", true);

    dvatest::check(window.selectFirstPartForTesting(),
                   "starter UI model has a part item");
    checkSelectedIdentityRows();
    checkEditable("CAD Name", false);
    checkEditable("DCS Name", true);

    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");
    checkSelectedIdentityRows();
    checkEditable("Point Type", true);

    dvatest::check(window.selectFirstFeatureForTesting(),
                   "starter UI model has a feature item");
    checkSelectedIdentityRows();
    checkEditable("Feature Type", true);

    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");
    checkSelectedIdentityRows();
    checkEditable("Measure Type", true);

    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");
    checkSelectedIdentityRows();
    checkEditable("Name", true);

    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");
    checkSelectedIdentityRows();
    checkEditable("GD&T Type", true);

    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");
    checkSelectedIdentityRows();
    checkEditable("Move Type", true);

    window.setModelForTesting(modelWithVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");
    checkEditable("Type", false);
    checkEditable("Name", true);
}

TEST("main window category rows remain read only") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());

    auto checkCategory = [&](QTreeWidgetItem* category,
                             const QString& label) {
        dvatest::check(category != nullptr,
                       QString("%1 category exists").arg(label).toStdString());
        QTreeWidget* navigator = window.findChild<QTreeWidget*>();
        dvatest::check(navigator != nullptr, "navigator exists");
        navigator->setCurrentItem(category);

        const int row = propertyRow(window, "Type");
        dvatest::check(row >= 0,
                       QString("%1 category properties include type")
                           .arg(label)
                           .toStdString());
        QTableWidgetItem* value =
            window.propertyTableForTesting()->item(row, 1);
        dvatest::check(value != nullptr,
                       QString("%1 category type value item exists")
                           .arg(label)
                           .toStdString());
        dvatest::check(value->text().toStdString() == "Category",
                       QString("%1 category type is shown").arg(label)
                           .toStdString());
        dvatest::check((value->flags() & Qt::ItemIsEditable) == 0,
                       QString("%1 category type remains read only")
                           .arg(label)
                           .toStdString());
    };

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");

    QTreeWidgetItem* parts = childByText(root, "Parts");
    checkCategory(parts, "Parts");

    dvatest::check(parts != nullptr && parts->childCount() > 0,
                   "parts category has a part child");
    QTreeWidgetItem* part = parts->child(0);
    checkCategory(childByText(part, "Points"), "Points");
    checkCategory(childByText(part, "Features"), "Features");
    checkCategory(childByText(part, "Tolerances"), "Tolerances");

    checkCategory(childByText(root, "Moves"), "Moves");
    checkCategory(childByText(root, "Measures"), "Measures");
    checkCategory(childByText(root, "Variants"), "Variants");
}

TEST("main window add part action cancellation keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    const std::size_t partCount = initial.parts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();
    dvatest::check(partCount == 1, "starter UI model has one part");

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr, "add part dialog appears");
        if (dialog != nullptr) {
            dialog->reject();
        }
    });

    QAction* addPart = actionByText(window, "Add Part");
    dvatest::check(addPart != nullptr, "add part action exists");
    addPart->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == partCount,
                   "cancelled add part leaves part count unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "cancelled add part leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "cancelled add part leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "cancelled add part leaves variants unchanged");
}

TEST("main window add part action blank name keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    const std::size_t partCount = initial.parts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();
    dvatest::check(partCount == 1, "starter UI model has one part");

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr, "add part dialog appears");
        QLineEdit* nameEdit =
            dialog != nullptr ? dialog->findChild<QLineEdit*>() : nullptr;
        dvatest::check(nameEdit != nullptr,
                       "add part dialog has a name editor");
        if (nameEdit != nullptr) {
            nameEdit->setText("   ");
        }
        if (dialog != nullptr) {
            dialog->accept();
        }
    });

    QAction* addPart = actionByText(window, "Add Part");
    dvatest::check(addPart != nullptr, "add part action exists");
    addPart->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == partCount,
                   "blank-name add part leaves part count unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "blank-name add part leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "blank-name add part leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "blank-name add part leaves variants unchanged");
}

TEST("main window add part action trims created part name") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    const PartId expectedPartId = nextPartId(initial);
    const std::size_t partCount = initial.parts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();
    dvatest::check(partCount == 1, "starter UI model has one part");

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr, "add part dialog appears");
        QLineEdit* nameEdit =
            dialog != nullptr ? dialog->findChild<QLineEdit*>() : nullptr;
        dvatest::check(nameEdit != nullptr,
                       "add part dialog has a name editor");
        if (nameEdit != nullptr) {
            nameEdit->setText("  Trimmed Part  ");
        }
        if (dialog != nullptr) {
            dialog->accept();
        }
    });

    QAction* addPart = actionByText(window, "Add Part");
    dvatest::check(addPart != nullptr, "add part action exists");
    addPart->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == partCount + 1,
                   "trimmed-name add part creates one part");
    dvatest::check(model.moves.size() == moveCount,
                   "trimmed-name add part leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "trimmed-name add part leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "trimmed-name add part leaves variants unchanged");

    const Part& part = model.parts.back();
    dvatest::check(part.id == expectedPartId,
                   "trimmed-name created part uses next id");
    dvatest::check(part.cadName == "Trimmed Part",
                   "trimmed-name created part uses trimmed CAD name");
    dvatest::check(part.dcsName == "Trimmed Part",
                   "trimmed-name created part uses trimmed DCS name");
}

TEST("main window add part action creates part") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    const PartId expectedPartId = nextPartId(initial);
    const std::string expectedName =
        "Part " + std::to_string(expectedPartId);
    const std::size_t partCount = initial.parts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();
    dvatest::check(partCount == 1, "starter UI model has one part");

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr, "add part dialog appears");
        if (dialog != nullptr) {
            dialog->accept();
        }
    });

    QAction* addPart = actionByText(window, "Add Part");
    dvatest::check(addPart != nullptr, "add part action exists");
    addPart->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == partCount + 1,
                   "add part creates one part");
    dvatest::check(model.moves.size() == moveCount,
                   "add part leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "add part leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "add part leaves variants unchanged");

    const Part& part = model.parts.back();
    dvatest::check(part.id == expectedPartId,
                   "created part uses next id");
    dvatest::check(part.cadName == expectedName,
                   "created part uses default CAD name");
    dvatest::check(part.dcsName == expectedName,
                   "created part uses default DCS name");
    dvatest::check(part.points.empty(),
                   "created part has no points");
    dvatest::check(part.features.empty(),
                   "created part has no features");
    dvatest::check(part.tolerances.empty(),
                   "created part has no tolerances");
    dvatest::check(part.gdts.empty(),
                   "created part has no GD&T callouts");
}

TEST("main window add point action cancellation keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr, "add point X dialog appears");
        if (dialog != nullptr) {
            dialog->reject();
        }
    });

    QAction* addPoint = actionByText(window, "Add Point");
    dvatest::check(addPoint != nullptr, "add point action exists");
    addPoint->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "cancelled add point leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "cancelled add point leaves point count unchanged");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "cancelled add point leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "cancelled add point leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.size() == gdtCount,
                   "cancelled add point leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "cancelled add point leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "cancelled add point leaves measures unchanged");
}

TEST("main window add point action Y cancellation keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* xDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(xDialog != nullptr, "add point X dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* yDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(yDialog != nullptr, "add point Y dialog appears");
            if (yDialog != nullptr) {
                yDialog->reject();
            }
        });
        if (xDialog != nullptr) {
            xDialog->accept();
        }
    });

    QAction* addPoint = actionByText(window, "Add Point");
    dvatest::check(addPoint != nullptr, "add point action exists");
    addPoint->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "Y-cancelled add point leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "Y-cancelled add point leaves point count unchanged");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "Y-cancelled add point leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "Y-cancelled add point leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.size() == gdtCount,
                   "Y-cancelled add point leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "Y-cancelled add point leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "Y-cancelled add point leaves measures unchanged");
}

TEST("main window add point action Z cancellation keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* xDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(xDialog != nullptr, "add point X dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* yDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(yDialog != nullptr, "add point Y dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* zDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(zDialog != nullptr,
                               "add point Z dialog appears");
                if (zDialog != nullptr) {
                    zDialog->reject();
                }
            });
            if (yDialog != nullptr) {
                yDialog->accept();
            }
        });
        if (xDialog != nullptr) {
            xDialog->accept();
        }
    });

    QAction* addPoint = actionByText(window, "Add Point");
    dvatest::check(addPoint != nullptr, "add point action exists");
    addPoint->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "Z-cancelled add point leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "Z-cancelled add point leaves point count unchanged");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "Z-cancelled add point leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "Z-cancelled add point leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.size() == gdtCount,
                   "Z-cancelled add point leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "Z-cancelled add point leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "Z-cancelled add point leaves measures unchanged");
}

TEST("main window add point action creates point") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const PointId expectedPointId = nextPointId(initial);
    const FeatureId expectedFeatureId = nextFeatureId(initial);

    QTimer::singleShot(0, []() {
        QDialog* xDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(xDialog != nullptr, "add point X dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* yDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(yDialog != nullptr, "add point Y dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* zDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(zDialog != nullptr,
                               "add point Z dialog appears");
                if (zDialog != nullptr) {
                    zDialog->accept();
                }
            });
            if (yDialog != nullptr) {
                yDialog->accept();
            }
        });
        if (xDialog != nullptr) {
            xDialog->accept();
        }
    });

    QAction* addPoint = actionByText(window, "Add Point");
    dvatest::check(addPoint != nullptr, "add point action exists");
    addPoint->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "add point leaves part count unchanged");
    const Part& part = model.parts.front();
    dvatest::check(part.points.size() == pointCount + 1,
                   "add point creates one point");
    dvatest::check(part.features.size() == featureCount + 1,
                   "add point creates one point-based feature");
    dvatest::check(part.tolerances.size() == toleranceCount,
                   "add point leaves tolerances unchanged");
    dvatest::check(part.gdts.size() == gdtCount,
                   "add point leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "add point leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "add point leaves measures unchanged");

    const Point& point = part.points.back();
    dvatest::check(point.id == expectedPointId,
                   "created point uses next id");
    dvatest::check(point.kind == PointKind::Coordinate,
                   "created point is coordinate kind");
    dvatest::checkNear(point.position.x, 0.0, 1e-12,
                       "created point default X");
    dvatest::checkNear(point.position.y, 0.0, 1e-12,
                       "created point default Y");
    dvatest::checkNear(point.position.z, 0.0, 1e-12,
                       "created point default Z");
    dvatest::checkNear(point.ijk.x, 0.0, 1e-12,
                       "created point default direction X");
    dvatest::checkNear(point.ijk.y, 0.0, 1e-12,
                       "created point default direction Y");
    dvatest::checkNear(point.ijk.z, 1.0, 1e-12,
                       "created point default direction Z");
    dvatest::check(point.active,
                   "created point is active");

    const Feature& feature = part.features.back();
    dvatest::check(feature.id == expectedFeatureId,
                   "created feature uses next id");
    dvatest::check(feature.kind == FeatureKind::PointBased,
                   "created feature is point-based");
    dvatest::check(feature.definingPoints.size() == 1,
                   "created feature has one defining point");
    dvatest::check(feature.definingPoints.front() == expectedPointId,
                   "created feature references the new point");
}

TEST("main window add point action creates point in selected part") {
    Model modelWithTargetPart = createStarterModel();
    const PartId targetPartId = addPart(modelWithTargetPart, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithTargetPart);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    if (navigator != nullptr && targetPart != nullptr) {
        navigator->setCurrentItem(targetPart);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    const std::size_t basePointCount = basePart.points.size();
    const std::size_t baseFeatureCount = basePart.features.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const PointId expectedPointId = nextPointId(initial);
    const FeatureId expectedFeatureId = nextFeatureId(initial);

    QTimer::singleShot(0, []() {
        QDialog* xDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(xDialog != nullptr, "add point X dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* yDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(yDialog != nullptr, "add point Y dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* zDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(zDialog != nullptr,
                               "add point Z dialog appears");
                if (zDialog != nullptr) {
                    zDialog->accept();
                }
            });
            if (yDialog != nullptr) {
                yDialog->accept();
            }
        });
        if (xDialog != nullptr) {
            xDialog->accept();
        }
    });

    QAction* addPoint = actionByText(window, "Add Point");
    dvatest::check(addPoint != nullptr, "add point action exists");
    addPoint->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected-part add point leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == basePointCount,
                   "selected-part add point leaves base points unchanged");
    dvatest::check(model.parts.front().features.size() == baseFeatureCount,
                   "selected-part add point leaves base features unchanged");
    const Part& updatedTarget = model.parts.back();
    dvatest::check(updatedTarget.points.size() == targetPointCount + 1,
                   "selected-part add point creates one target point");
    dvatest::check(updatedTarget.features.size() == targetFeatureCount + 1,
                   "selected-part add point creates one target feature");
    dvatest::check(updatedTarget.tolerances.size() == targetToleranceCount,
                   "selected-part add point leaves target tolerances unchanged");
    dvatest::check(updatedTarget.gdts.size() == targetGdtCount,
                   "selected-part add point leaves target GD&T unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-part add point leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected-part add point leaves measures unchanged");

    const Point& point = updatedTarget.points.back();
    dvatest::check(point.id == expectedPointId,
                   "selected-part created point uses next id");
    dvatest::check(point.kind == PointKind::Coordinate,
                   "selected-part created point is coordinate kind");
    dvatest::checkNear(point.position.x, 0.0, 1e-12,
                       "selected-part created point default X");
    dvatest::checkNear(point.position.y, 0.0, 1e-12,
                       "selected-part created point default Y");
    dvatest::checkNear(point.position.z, 0.0, 1e-12,
                       "selected-part created point default Z");
    dvatest::check(point.active,
                   "selected-part created point is active");

    const Feature& feature = updatedTarget.features.back();
    dvatest::check(feature.id == expectedFeatureId,
                   "selected-part created feature uses next id");
    dvatest::check(feature.kind == FeatureKind::PointBased,
                   "selected-part created feature is point-based");
    dvatest::check(feature.definingPoints.size() == 1,
                   "selected-part created feature has one defining point");
    dvatest::check(feature.definingPoints.front() == expectedPointId,
                   "selected-part created feature references the new point");
}

TEST("main window add point action creates point in selected part category") {
    Model modelWithTargetPart = createStarterModel();
    const PartId targetPartId = addPart(modelWithTargetPart, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithTargetPart);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    QTreeWidgetItem* targetPoints = childByText(targetPart, "Points");
    dvatest::check(targetPoints != nullptr, "target points category exists");
    if (navigator != nullptr && targetPoints != nullptr) {
        navigator->setCurrentItem(targetPoints);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    const std::size_t basePointCount = basePart.points.size();
    const std::size_t baseFeatureCount = basePart.features.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const PointId expectedPointId = nextPointId(initial);
    const FeatureId expectedFeatureId = nextFeatureId(initial);

    QTimer::singleShot(0, []() {
        QDialog* xDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(xDialog != nullptr, "add point X dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* yDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(yDialog != nullptr, "add point Y dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* zDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(zDialog != nullptr,
                               "add point Z dialog appears");
                if (zDialog != nullptr) {
                    zDialog->accept();
                }
            });
            if (yDialog != nullptr) {
                yDialog->accept();
            }
        });
        if (xDialog != nullptr) {
            xDialog->accept();
        }
    });

    QAction* addPoint = actionByText(window, "Add Point");
    dvatest::check(addPoint != nullptr, "add point action exists");
    addPoint->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected-category add point leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == basePointCount,
                   "selected-category add point leaves base points unchanged");
    dvatest::check(model.parts.front().features.size() == baseFeatureCount,
                   "selected-category add point leaves base features unchanged");
    const Part& updatedTarget = model.parts.back();
    dvatest::check(updatedTarget.points.size() == targetPointCount + 1,
                   "selected-category add point creates one target point");
    dvatest::check(updatedTarget.features.size() == targetFeatureCount + 1,
                   "selected-category add point creates one target feature");
    dvatest::check(updatedTarget.tolerances.size() == targetToleranceCount,
                   "selected-category add point leaves target tolerances unchanged");
    dvatest::check(updatedTarget.gdts.size() == targetGdtCount,
                   "selected-category add point leaves target GD&T unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-category add point leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected-category add point leaves measures unchanged");

    const Point& point = updatedTarget.points.back();
    dvatest::check(point.id == expectedPointId,
                   "selected-category created point uses next id");
    dvatest::check(point.kind == PointKind::Coordinate,
                   "selected-category created point is coordinate kind");
    const Feature& feature = updatedTarget.features.back();
    dvatest::check(feature.id == expectedFeatureId,
                   "selected-category created feature uses next id");
    dvatest::check(feature.kind == FeatureKind::PointBased,
                   "selected-category created feature is point-based");
    dvatest::check(feature.definingPoints.size() == 1,
                   "selected-category created feature has one defining point");
    dvatest::check(feature.definingPoints.front() == expectedPointId,
                   "selected-category created feature references the new point");
}

TEST("main window add point action creates point from selected point") {
    Model modelWithTargetPart = createStarterModel();
    const PartId targetPartId = addPart(modelWithTargetPart, "Target");
    const PointId selectedPointId =
        addCoordinatePoint(modelWithTargetPart, {1.0, 2.0, 3.0},
                           targetPartId);

    ui::MainWindow window;
    window.setModelForTesting(modelWithTargetPart);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    QTreeWidgetItem* targetPoints = childByText(targetPart, "Points");
    dvatest::check(targetPoints != nullptr, "target points category exists");
    dvatest::check(targetPoints != nullptr && targetPoints->childCount() == 1,
                   "target points category shows one point");
    QTreeWidgetItem* selectedPoint =
        targetPoints != nullptr && targetPoints->childCount() > 0
            ? targetPoints->child(0)
            : nullptr;
    dvatest::check(selectedPoint != nullptr, "target point item exists");
    if (navigator != nullptr && selectedPoint != nullptr) {
        navigator->setCurrentItem(selectedPoint);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    dvatest::check(target.points.front().id == selectedPointId,
                   "target point is the selected model point");
    const std::size_t basePointCount = basePart.points.size();
    const std::size_t baseFeatureCount = basePart.features.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const PointId expectedPointId = nextPointId(initial);
    const FeatureId expectedFeatureId = nextFeatureId(initial);

    QTimer::singleShot(0, []() {
        QDialog* xDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(xDialog != nullptr, "add point X dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* yDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(yDialog != nullptr, "add point Y dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* zDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(zDialog != nullptr,
                               "add point Z dialog appears");
                if (zDialog != nullptr) {
                    zDialog->accept();
                }
            });
            if (yDialog != nullptr) {
                yDialog->accept();
            }
        });
        if (xDialog != nullptr) {
            xDialog->accept();
        }
    });

    QAction* addPoint = actionByText(window, "Add Point");
    dvatest::check(addPoint != nullptr, "add point action exists");
    addPoint->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected-point add point leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == basePointCount,
                   "selected-point add point leaves base points unchanged");
    dvatest::check(model.parts.front().features.size() == baseFeatureCount,
                   "selected-point add point leaves base features unchanged");
    const Part& updatedTarget = model.parts.back();
    dvatest::check(updatedTarget.points.size() == targetPointCount + 1,
                   "selected-point add point creates one target point");
    dvatest::check(updatedTarget.features.size() == targetFeatureCount + 1,
                   "selected-point add point creates one target feature");
    dvatest::check(updatedTarget.tolerances.size() == targetToleranceCount,
                   "selected-point add point leaves target tolerances unchanged");
    dvatest::check(updatedTarget.gdts.size() == targetGdtCount,
                   "selected-point add point leaves target GD&T unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-point add point leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected-point add point leaves measures unchanged");

    const Point& point = updatedTarget.points.back();
    dvatest::check(point.id == expectedPointId,
                   "selected-point created point uses next id");
    dvatest::check(point.kind == PointKind::Coordinate,
                   "selected-point created point is coordinate kind");
    const Feature& feature = updatedTarget.features.back();
    dvatest::check(feature.id == expectedFeatureId,
                   "selected-point created feature uses next id");
    dvatest::check(feature.kind == FeatureKind::PointBased,
                   "selected-point created feature is point-based");
    dvatest::check(feature.definingPoints.size() == 1,
                   "selected-point created feature has one defining point");
    dvatest::check(feature.definingPoints.front() == expectedPointId,
                   "selected-point created feature references the new point");
}

TEST("main window add feature action requires a point") {
    Model emptyPartModel;
    addPart(emptyPartModel, "Block");

    ui::MainWindow window;
    window.setModelForTesting(emptyPartModel);
    dvatest::check(window.selectRootForTesting(),
                   "empty-part UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "empty-part UI model has one part");
    dvatest::check(initial.parts.front().points.empty(),
                   "empty-part UI model has no points");
    dvatest::check(initial.parts.front().features.empty(),
                   "empty-part UI model has no features");
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "add feature guard message appears");
        QAbstractButton* okButton =
            box != nullptr ? box->button(QMessageBox::Ok) : nullptr;
        dvatest::check(okButton != nullptr,
                       "add feature guard message has an ok button");
        if (okButton != nullptr) {
            okButton->click();
        }
    });

    QAction* addFeature = actionByText(window, "Add Feature");
    dvatest::check(addFeature != nullptr,
                   "add feature action exists");
    addFeature->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "add feature guard leaves part count unchanged");
    dvatest::check(model.parts.front().points.empty(),
                   "add feature guard leaves points empty");
    dvatest::check(model.parts.front().features.empty(),
                   "add feature guard leaves features empty");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "add feature guard leaves tolerances unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "add feature guard leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "add feature guard leaves measures unchanged");
}

TEST("main window add feature action selected empty part requires a point") {
    Model modelWithEmptyTarget = createStarterModel();
    const PartId targetPartId = addPart(modelWithEmptyTarget, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithEmptyTarget);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    if (navigator != nullptr && targetPart != nullptr) {
        navigator->setCurrentItem(targetPart);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(!basePart.points.empty(),
                   "base part has points");
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    dvatest::check(target.points.empty(),
                   "target part has no points");
    const std::size_t baseFeatureCount = basePart.features.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* partDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(partDialog != nullptr,
                       "add feature part dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* typeDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(typeDialog != nullptr,
                           "add feature type dialog appears");
            QTimer::singleShot(0, []() {
                QMessageBox* box =
                    qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                dvatest::check(box != nullptr,
                               "selected empty part feature guard appears");
                QAbstractButton* okButton =
                    box != nullptr ? box->button(QMessageBox::Ok) : nullptr;
                dvatest::check(okButton != nullptr,
                               "selected empty part feature guard has ok");
                if (okButton != nullptr) {
                    okButton->click();
                }
            });
            if (typeDialog != nullptr) {
                typeDialog->accept();
            }
        });
        if (partDialog != nullptr) {
            partDialog->accept();
        }
    });

    QAction* addFeature = actionByText(window, "Add Feature");
    dvatest::check(addFeature != nullptr,
                   "add feature action exists");
    addFeature->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected empty part feature guard leaves part count unchanged");
    dvatest::check(model.parts.front().features.size() == baseFeatureCount,
                   "selected empty part feature guard leaves base features unchanged");
    dvatest::check(model.parts.back().points.empty(),
                   "selected empty part feature guard leaves target points empty");
    dvatest::check(model.parts.back().features.size() == targetFeatureCount,
                   "selected empty part feature guard leaves target features unchanged");
    dvatest::check(model.parts.back().tolerances.size() ==
                       targetToleranceCount,
                   "selected empty part feature guard leaves target tolerances unchanged");
    dvatest::check(model.parts.back().gdts.size() == targetGdtCount,
                   "selected empty part feature guard leaves target GD&T unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected empty part feature guard leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected empty part feature guard leaves measures unchanged");
}

TEST("main window add feature action part cancellation keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr,
                       "add feature part dialog appears");
        if (dialog != nullptr) {
            dialog->reject();
        }
    });

    QAction* addFeature = actionByText(window, "Add Feature");
    dvatest::check(addFeature != nullptr,
                   "add feature action exists");
    addFeature->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "part-cancelled add feature leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "part-cancelled add feature leaves points unchanged");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "part-cancelled add feature leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "part-cancelled add feature leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.size() == gdtCount,
                   "part-cancelled add feature leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "part-cancelled add feature leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "part-cancelled add feature leaves measures unchanged");
}

TEST("main window add feature action type cancellation keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* partDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(partDialog != nullptr,
                       "add feature part dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* typeDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(typeDialog != nullptr,
                           "add feature type dialog appears");
            if (typeDialog != nullptr) {
                typeDialog->reject();
            }
        });
        if (partDialog != nullptr) {
            partDialog->accept();
        }
    });

    QAction* addFeature = actionByText(window, "Add Feature");
    dvatest::check(addFeature != nullptr,
                   "add feature action exists");
    addFeature->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "type-cancelled add feature leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "type-cancelled add feature leaves points unchanged");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "type-cancelled add feature leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "type-cancelled add feature leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.size() == gdtCount,
                   "type-cancelled add feature leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "type-cancelled add feature leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "type-cancelled add feature leaves measures unchanged");
}

TEST("main window add feature action creates feature") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const FeatureId expectedFeatureId = nextFeatureId(initial);
    const PointId expectedPointId = initial.parts.front().points.back().id;

    QTimer::singleShot(0, []() {
        QDialog* partDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(partDialog != nullptr,
                       "add feature part dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* typeDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(typeDialog != nullptr,
                           "add feature type dialog appears");
            if (typeDialog != nullptr) {
                typeDialog->accept();
            }
        });
        if (partDialog != nullptr) {
            partDialog->accept();
        }
    });

    QAction* addFeature = actionByText(window, "Add Feature");
    dvatest::check(addFeature != nullptr,
                   "add feature action exists");
    addFeature->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "add feature leaves part count unchanged");
    const Part& part = model.parts.front();
    dvatest::check(part.points.size() == pointCount,
                   "add feature leaves points unchanged");
    dvatest::check(part.features.size() == featureCount + 1,
                   "add feature creates one feature");
    dvatest::check(part.tolerances.size() == toleranceCount,
                   "add feature leaves tolerances unchanged");
    dvatest::check(part.gdts.size() == gdtCount,
                   "add feature leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "add feature leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "add feature leaves measures unchanged");

    const Feature& feature = part.features.back();
    dvatest::check(feature.id == expectedFeatureId,
                   "created feature uses next id");
    dvatest::check(feature.kind == FeatureKind::Plane,
                   "created feature default type is Plane");
    dvatest::check(feature.definingPoints.size() == 1,
                   "created feature has one defining point");
    dvatest::check(feature.definingPoints.front() == expectedPointId,
                   "created feature references the latest point");
}

TEST("main window add feature action creates feature in selected part") {
    Model modelWithTargetPart = createStarterModel();
    const PartId targetPartId = addPart(modelWithTargetPart, "Target");
    const PointId targetPointId =
        addCoordinatePoint(modelWithTargetPart, {1.0, 2.0, 3.0},
                           targetPartId);

    ui::MainWindow window;
    window.setModelForTesting(modelWithTargetPart);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    if (navigator != nullptr && targetPart != nullptr) {
        navigator->setCurrentItem(targetPart);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    const std::size_t basePointCount = basePart.points.size();
    const std::size_t baseFeatureCount = basePart.features.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const FeatureId expectedFeatureId = nextFeatureId(initial);

    QTimer::singleShot(0, []() {
        QDialog* partDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(partDialog != nullptr,
                       "add feature part dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* typeDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(typeDialog != nullptr,
                           "add feature type dialog appears");
            if (typeDialog != nullptr) {
                typeDialog->accept();
            }
        });
        if (partDialog != nullptr) {
            partDialog->accept();
        }
    });

    QAction* addFeature = actionByText(window, "Add Feature");
    dvatest::check(addFeature != nullptr,
                   "add feature action exists");
    addFeature->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected-part add feature leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == basePointCount,
                   "selected-part add feature leaves base points unchanged");
    dvatest::check(model.parts.front().features.size() == baseFeatureCount,
                   "selected-part add feature leaves base features unchanged");
    const Part& updatedTarget = model.parts.back();
    dvatest::check(updatedTarget.points.size() == targetPointCount,
                   "selected-part add feature leaves target points unchanged");
    dvatest::check(updatedTarget.features.size() == targetFeatureCount + 1,
                   "selected-part add feature creates one target feature");
    dvatest::check(updatedTarget.tolerances.size() == targetToleranceCount,
                   "selected-part add feature leaves target tolerances unchanged");
    dvatest::check(updatedTarget.gdts.size() == targetGdtCount,
                   "selected-part add feature leaves target GD&T unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-part add feature leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected-part add feature leaves measures unchanged");

    const Feature& feature = updatedTarget.features.back();
    dvatest::check(feature.id == expectedFeatureId,
                   "selected-part created feature uses next id");
    dvatest::check(feature.kind == FeatureKind::Plane,
                   "selected-part created feature default type is Plane");
    dvatest::check(feature.definingPoints.size() == 1,
                   "selected-part created feature has one defining point");
    dvatest::check(feature.definingPoints.front() == targetPointId,
                   "selected-part created feature references target point");
}

TEST("main window add feature action creates feature from selected point") {
    Model modelWithTargetPart = createStarterModel();
    const PartId targetPartId = addPart(modelWithTargetPart, "Target");
    const PointId targetPointId =
        addCoordinatePoint(modelWithTargetPart, {1.0, 2.0, 3.0},
                           targetPartId);

    ui::MainWindow window;
    window.setModelForTesting(modelWithTargetPart);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    QTreeWidgetItem* targetPoints = childByText(targetPart, "Points");
    dvatest::check(targetPoints != nullptr, "target points category exists");
    dvatest::check(targetPoints != nullptr && targetPoints->childCount() == 1,
                   "target points category shows one point");
    QTreeWidgetItem* selectedPoint =
        targetPoints != nullptr && targetPoints->childCount() > 0
            ? targetPoints->child(0)
            : nullptr;
    dvatest::check(selectedPoint != nullptr, "target point item exists");
    if (navigator != nullptr && selectedPoint != nullptr) {
        navigator->setCurrentItem(selectedPoint);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    const std::size_t basePointCount = basePart.points.size();
    const std::size_t baseFeatureCount = basePart.features.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const FeatureId expectedFeatureId = nextFeatureId(initial);

    QTimer::singleShot(0, []() {
        QDialog* partDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(partDialog != nullptr,
                       "add feature part dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* typeDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(typeDialog != nullptr,
                           "add feature type dialog appears");
            if (typeDialog != nullptr) {
                typeDialog->accept();
            }
        });
        if (partDialog != nullptr) {
            partDialog->accept();
        }
    });

    QAction* addFeature = actionByText(window, "Add Feature");
    dvatest::check(addFeature != nullptr,
                   "add feature action exists");
    addFeature->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected-point add feature leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == basePointCount,
                   "selected-point add feature leaves base points unchanged");
    dvatest::check(model.parts.front().features.size() == baseFeatureCount,
                   "selected-point add feature leaves base features unchanged");
    const Part& updatedTarget = model.parts.back();
    dvatest::check(updatedTarget.points.size() == targetPointCount,
                   "selected-point add feature leaves target points unchanged");
    dvatest::check(updatedTarget.features.size() == targetFeatureCount + 1,
                   "selected-point add feature creates one target feature");
    dvatest::check(updatedTarget.tolerances.size() == targetToleranceCount,
                   "selected-point add feature leaves target tolerances unchanged");
    dvatest::check(updatedTarget.gdts.size() == targetGdtCount,
                   "selected-point add feature leaves target GD&T unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-point add feature leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected-point add feature leaves measures unchanged");

    const Feature& feature = updatedTarget.features.back();
    dvatest::check(feature.id == expectedFeatureId,
                   "selected-point created feature uses next id");
    dvatest::check(feature.kind == FeatureKind::Plane,
                   "selected-point created feature default type is Plane");
    dvatest::check(feature.definingPoints.size() == 1,
                   "selected-point created feature has one defining point");
    dvatest::check(feature.definingPoints.front() == targetPointId,
                   "selected-point created feature references target point");
}

TEST("main window add linear tolerance action requires a point") {
    Model emptyPartModel;
    addPart(emptyPartModel, "Block");

    ui::MainWindow window;
    window.setModelForTesting(emptyPartModel);
    dvatest::check(window.selectRootForTesting(),
                   "empty-part UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "empty-part UI model has one part");
    dvatest::check(initial.parts.front().points.empty(),
                   "empty-part UI model has no points");
    dvatest::check(initial.parts.front().tolerances.empty(),
                   "empty-part UI model has no tolerances");
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr,
                       "add tolerance guard message appears");
        QAbstractButton* okButton =
            box != nullptr ? box->button(QMessageBox::Ok) : nullptr;
        dvatest::check(okButton != nullptr,
                       "add tolerance guard message has an ok button");
        if (okButton != nullptr) {
            okButton->click();
        }
    });

    QAction* addTolerance = actionByText(window, "Add Linear Tolerance");
    dvatest::check(addTolerance != nullptr,
                   "add linear tolerance action exists");
    addTolerance->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "add tolerance guard leaves part count unchanged");
    dvatest::check(model.parts.front().points.empty(),
                   "add tolerance guard leaves points empty");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "add tolerance guard leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.empty(),
                   "add tolerance guard leaves tolerances empty");
    dvatest::check(model.moves.size() == moveCount,
                   "add tolerance guard leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "add tolerance guard leaves measures unchanged");
}

TEST("main window add linear tolerance action selected empty part requires a point") {
    Model modelWithEmptyTarget = createStarterModel();
    const PartId targetPartId = addPart(modelWithEmptyTarget, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithEmptyTarget);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    if (navigator != nullptr && targetPart != nullptr) {
        navigator->setCurrentItem(targetPart);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(!basePart.points.empty(),
                   "base part has points");
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    dvatest::check(target.points.empty(),
                   "target part has no points");
    const std::size_t baseFeatureCount = basePart.features.size();
    const std::size_t baseToleranceCount = basePart.tolerances.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr,
                       "selected empty part tolerance guard message appears");
        QAbstractButton* okButton =
            box != nullptr ? box->button(QMessageBox::Ok) : nullptr;
        dvatest::check(okButton != nullptr,
                       "selected empty part tolerance guard has an ok button");
        if (okButton != nullptr) {
            okButton->click();
        }
    });

    QAction* addTolerance = actionByText(window, "Add Linear Tolerance");
    dvatest::check(addTolerance != nullptr,
                   "add linear tolerance action exists");
    addTolerance->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected empty part tolerance guard leaves part count unchanged");
    dvatest::check(model.parts.front().features.size() == baseFeatureCount,
                   "selected empty part tolerance guard leaves base features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == baseToleranceCount,
                   "selected empty part tolerance guard leaves base tolerances unchanged");
    dvatest::check(model.parts.back().points.empty(),
                   "selected empty part tolerance guard leaves target points empty");
    dvatest::check(model.parts.back().features.size() == targetFeatureCount,
                   "selected empty part tolerance guard leaves target features unchanged");
    dvatest::check(model.parts.back().tolerances.size() == targetToleranceCount,
                   "selected empty part tolerance guard leaves target tolerances unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected empty part tolerance guard leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected empty part tolerance guard leaves measures unchanged");
}

TEST("main window add linear tolerance action cancellation keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr,
                       "add linear tolerance range dialog appears");
        if (dialog != nullptr) {
            dialog->reject();
        }
    });

    QAction* addTolerance = actionByText(window, "Add Linear Tolerance");
    dvatest::check(addTolerance != nullptr,
                   "add linear tolerance action exists");
    addTolerance->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "cancelled add tolerance leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "cancelled add tolerance leaves points unchanged");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "cancelled add tolerance leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "cancelled add tolerance leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.size() == gdtCount,
                   "cancelled add tolerance leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "cancelled add tolerance leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "cancelled add tolerance leaves measures unchanged");
}

TEST("main window add linear tolerance action creates tolerance") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr,
                       "add linear tolerance range dialog appears");
        if (dialog != nullptr) {
            dialog->accept();
        }
    });

    QAction* addTolerance = actionByText(window, "Add Linear Tolerance");
    dvatest::check(addTolerance != nullptr,
                   "add linear tolerance action exists");
    addTolerance->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "add tolerance leaves part count unchanged");
    const Part& part = model.parts.front();
    dvatest::check(part.points.size() == pointCount,
                   "add tolerance leaves points unchanged");
    dvatest::check(part.features.size() == featureCount,
                   "add tolerance leaves features unchanged");
    dvatest::check(part.tolerances.size() == toleranceCount + 1,
                   "add tolerance creates one tolerance");
    dvatest::check(part.gdts.size() == gdtCount,
                   "add tolerance leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "add tolerance leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "add tolerance leaves measures unchanged");

    const ToleranceDef& tolerance = part.tolerances.back();
    dvatest::check(tolerance.active,
                   "created tolerance is active");
    dvatest::check(!tolerance.features.empty(),
                   "created tolerance references a feature");
    dvatest::check(tolerance.features.front() == part.features.back().id,
                   "created tolerance references the latest feature");
    dvatest::check(tolerance.ir.geomRule == GeomRule::TranslateAlongVector,
                   "created tolerance uses linear translate rule");
    dvatest::check(tolerance.ir.direction.type == DirectionType::TypeIn,
                   "created tolerance uses typed direction");
    dvatest::checkNear(tolerance.ir.direction.ijk.x, 0.0, 1e-12,
                       "created tolerance direction X default");
    dvatest::checkNear(tolerance.ir.direction.ijk.y, 0.0, 1e-12,
                       "created tolerance direction Y default");
    dvatest::checkNear(tolerance.ir.direction.ijk.z, 1.0, 1e-12,
                       "created tolerance direction Z default");
    dvatest::check(tolerance.ir.rands.size() == 1,
                   "created tolerance has one rand");
    dvatest::check(
        tolerance.ir.rands.front().distribution == DistributionType::Normal,
        "created tolerance uses normal distribution");
    dvatest::checkNear(tolerance.ir.rands.front().range, 1.0, 1e-12,
                       "created tolerance default range");
    dvatest::checkNear(tolerance.ir.rands.front().offset, 0.0, 1e-12,
                       "created tolerance default offset");
    dvatest::checkNear(tolerance.ir.rands.front().sigmaNum, 3.0, 1e-12,
                       "created tolerance default sigma number");
}

TEST("main window add linear tolerance action creates tolerance in selected part") {
    Model modelWithTargetPart = createStarterModel();
    const PartId targetPartId = addPart(modelWithTargetPart, "Target");
    addCoordinatePoint(modelWithTargetPart, {1.0, 2.0, 3.0}, targetPartId);
    const FeatureId targetFeatureId =
        addFeature(modelWithTargetPart, FeatureKind::Plane, targetPartId);

    ui::MainWindow window;
    window.setModelForTesting(modelWithTargetPart);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    if (navigator != nullptr && targetPart != nullptr) {
        navigator->setCurrentItem(targetPart);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    const std::size_t baseToleranceCount = basePart.tolerances.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const ToleranceId expectedToleranceId = nextToleranceId(initial);

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr,
                       "add linear tolerance range dialog appears");
        if (dialog != nullptr) {
            dialog->accept();
        }
    });

    QAction* addTolerance = actionByText(window, "Add Linear Tolerance");
    dvatest::check(addTolerance != nullptr,
                   "add linear tolerance action exists");
    addTolerance->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected-part add tolerance leaves part count unchanged");
    dvatest::check(model.parts.front().tolerances.size() == baseToleranceCount,
                   "selected-part add tolerance leaves base tolerances unchanged");
    const Part& updatedTarget = model.parts.back();
    dvatest::check(updatedTarget.points.size() == targetPointCount,
                   "selected-part add tolerance leaves target points unchanged");
    dvatest::check(updatedTarget.features.size() == targetFeatureCount,
                   "selected-part add tolerance leaves target features unchanged");
    dvatest::check(updatedTarget.tolerances.size() == targetToleranceCount + 1,
                   "selected-part add tolerance creates one target tolerance");
    dvatest::check(updatedTarget.gdts.size() == targetGdtCount,
                   "selected-part add tolerance leaves target GD&T unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-part add tolerance leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected-part add tolerance leaves measures unchanged");

    const ToleranceDef& tolerance = updatedTarget.tolerances.back();
    dvatest::check(tolerance.id == expectedToleranceId,
                   "selected-part created tolerance uses next id");
    dvatest::check(tolerance.active,
                   "selected-part created tolerance is active");
    dvatest::check(tolerance.features.size() == 1,
                   "selected-part created tolerance references one feature");
    dvatest::check(tolerance.features.front() == targetFeatureId,
                   "selected-part created tolerance references target feature");
    dvatest::checkNear(tolerance.ir.rands.front().range, 1.0, 1e-12,
                       "selected-part created tolerance default range");
}

TEST("main window add linear tolerance action creates tolerance from selected feature") {
    Model modelWithTargetPart = createStarterModel();
    const PartId targetPartId = addPart(modelWithTargetPart, "Target");
    addCoordinatePoint(modelWithTargetPart, {1.0, 2.0, 3.0}, targetPartId);
    const FeatureId targetFeatureId =
        addFeature(modelWithTargetPart, FeatureKind::Plane, targetPartId);

    ui::MainWindow window;
    window.setModelForTesting(modelWithTargetPart);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    QTreeWidgetItem* targetFeatures = childByText(targetPart, "Features");
    dvatest::check(targetFeatures != nullptr,
                   "target features category exists");
    dvatest::check(targetFeatures != nullptr &&
                       targetFeatures->childCount() >= 2,
                   "target features category shows generated features");
    QTreeWidgetItem* selectedFeature =
        targetFeatures != nullptr && targetFeatures->childCount() > 1
            ? targetFeatures->child(1)
            : nullptr;
    dvatest::check(selectedFeature != nullptr,
                   "target feature item exists");
    if (navigator != nullptr && selectedFeature != nullptr) {
        navigator->setCurrentItem(selectedFeature);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    const std::size_t baseToleranceCount = basePart.tolerances.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const ToleranceId expectedToleranceId = nextToleranceId(initial);

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr,
                       "add linear tolerance range dialog appears");
        if (dialog != nullptr) {
            dialog->accept();
        }
    });

    QAction* addTolerance = actionByText(window, "Add Linear Tolerance");
    dvatest::check(addTolerance != nullptr,
                   "add linear tolerance action exists");
    addTolerance->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected-feature add tolerance leaves part count unchanged");
    dvatest::check(model.parts.front().tolerances.size() == baseToleranceCount,
                   "selected-feature add tolerance leaves base tolerances unchanged");
    const Part& updatedTarget = model.parts.back();
    dvatest::check(updatedTarget.points.size() == targetPointCount,
                   "selected-feature add tolerance leaves target points unchanged");
    dvatest::check(updatedTarget.features.size() == targetFeatureCount,
                   "selected-feature add tolerance leaves target features unchanged");
    dvatest::check(updatedTarget.tolerances.size() == targetToleranceCount + 1,
                   "selected-feature add tolerance creates one target tolerance");
    dvatest::check(updatedTarget.gdts.size() == targetGdtCount,
                   "selected-feature add tolerance leaves target GD&T unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-feature add tolerance leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected-feature add tolerance leaves measures unchanged");

    const ToleranceDef& tolerance = updatedTarget.tolerances.back();
    dvatest::check(tolerance.id == expectedToleranceId,
                   "selected-feature created tolerance uses next id");
    dvatest::check(tolerance.features.size() == 1,
                   "selected-feature created tolerance references one feature");
    dvatest::check(tolerance.features.front() == targetFeatureId,
                   "selected-feature created tolerance references target feature");
}

TEST("main window add gdt action requires a feature") {
    Model emptyPartModel;
    addPart(emptyPartModel, "Block");

    ui::MainWindow window;
    window.setModelForTesting(emptyPartModel);
    dvatest::check(window.selectRootForTesting(),
                   "empty-part UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "empty-part UI model has one part");
    dvatest::check(initial.parts.front().features.empty(),
                   "empty-part UI model has no features");
    dvatest::check(initial.parts.front().gdts.empty(),
                   "empty-part UI model has no GD&T callouts");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "add GD&T guard message appears");
        QAbstractButton* okButton =
            box != nullptr ? box->button(QMessageBox::Ok) : nullptr;
        dvatest::check(okButton != nullptr,
                       "add GD&T guard message has an ok button");
        if (okButton != nullptr) {
            okButton->click();
        }
    });

    QAction* addGdt = actionByText(window, "Add GD&T");
    dvatest::check(addGdt != nullptr, "add GD&T action exists");
    addGdt->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "add GD&T guard leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "add GD&T guard leaves points unchanged");
    dvatest::check(model.parts.front().features.empty(),
                   "add GD&T guard leaves features empty");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "add GD&T guard leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.empty(),
                   "add GD&T guard leaves GD&T callouts empty");
    dvatest::check(model.moves.size() == moveCount,
                   "add GD&T guard leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "add GD&T guard leaves measures unchanged");
}

TEST("main window add gdt action selected empty part requires a feature") {
    Model modelWithEmptyTarget = createStarterModel();
    const PartId targetPartId = addPart(modelWithEmptyTarget, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithEmptyTarget);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    if (navigator != nullptr && targetPart != nullptr) {
        navigator->setCurrentItem(targetPart);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(!basePart.features.empty(),
                   "base part has features");
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    dvatest::check(target.features.empty(),
                   "target part has no features");
    const std::size_t baseGdtCount = basePart.gdts.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr,
                       "selected empty part GD&T guard message appears");
        QAbstractButton* okButton =
            box != nullptr ? box->button(QMessageBox::Ok) : nullptr;
        dvatest::check(okButton != nullptr,
                       "selected empty part GD&T guard has an ok button");
        if (okButton != nullptr) {
            okButton->click();
        }
    });

    QAction* addGdt = actionByText(window, "Add GD&T");
    dvatest::check(addGdt != nullptr, "add GD&T action exists");
    addGdt->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected empty part GD&T guard leaves part count unchanged");
    dvatest::check(model.parts.front().gdts.size() == baseGdtCount,
                   "selected empty part GD&T guard leaves base GD&T unchanged");
    dvatest::check(model.parts.back().points.size() == targetPointCount,
                   "selected empty part GD&T guard leaves target points unchanged");
    dvatest::check(model.parts.back().features.size() == targetFeatureCount,
                   "selected empty part GD&T guard leaves target features unchanged");
    dvatest::check(model.parts.back().tolerances.size() ==
                       targetToleranceCount,
                   "selected empty part GD&T guard leaves target tolerances unchanged");
    dvatest::check(model.parts.back().gdts.size() == targetGdtCount,
                   "selected empty part GD&T guard leaves target GD&T unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected empty part GD&T guard leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected empty part GD&T guard leaves measures unchanged");
}

TEST("main window add gdt action type cancellation keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(!initial.parts.front().features.empty(),
                   "starter UI model has a feature");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr,
                       "add GD&T type dialog appears");
        if (dialog != nullptr) {
            dialog->reject();
        }
    });

    QAction* addGdt = actionByText(window, "Add GD&T");
    dvatest::check(addGdt != nullptr, "add GD&T action exists");
    addGdt->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "type-cancelled add GD&T leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "type-cancelled add GD&T leaves points unchanged");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "type-cancelled add GD&T leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "type-cancelled add GD&T leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.size() == gdtCount,
                   "type-cancelled add GD&T leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "type-cancelled add GD&T leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "type-cancelled add GD&T leaves measures unchanged");
}

TEST("main window add gdt action range cancellation keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(!initial.parts.front().features.empty(),
                   "starter UI model has a feature");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* typeDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(typeDialog != nullptr,
                       "add GD&T type dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* rangeDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(rangeDialog != nullptr,
                           "add GD&T range dialog appears");
            if (rangeDialog != nullptr) {
                rangeDialog->reject();
            }
        });
        if (typeDialog != nullptr) {
            typeDialog->accept();
        }
    });

    QAction* addGdt = actionByText(window, "Add GD&T");
    dvatest::check(addGdt != nullptr, "add GD&T action exists");
    addGdt->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "range-cancelled add GD&T leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "range-cancelled add GD&T leaves points unchanged");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "range-cancelled add GD&T leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "range-cancelled add GD&T leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.size() == gdtCount,
                   "range-cancelled add GD&T leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "range-cancelled add GD&T leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "range-cancelled add GD&T leaves measures unchanged");
}

TEST("main window add gdt action zone cancellation keeps model") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(!initial.parts.front().features.empty(),
                   "starter UI model has a feature");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* typeDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(typeDialog != nullptr,
                       "add GD&T type dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* rangeDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(rangeDialog != nullptr,
                           "add GD&T range dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* zoneDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(zoneDialog != nullptr,
                               "add GD&T zone dialog appears");
                if (zoneDialog != nullptr) {
                    zoneDialog->reject();
                }
            });
            if (rangeDialog != nullptr) {
                rangeDialog->accept();
            }
        });
        if (typeDialog != nullptr) {
            typeDialog->accept();
        }
    });

    QAction* addGdt = actionByText(window, "Add GD&T");
    dvatest::check(addGdt != nullptr, "add GD&T action exists");
    addGdt->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "zone-cancelled add GD&T leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "zone-cancelled add GD&T leaves points unchanged");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "zone-cancelled add GD&T leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "zone-cancelled add GD&T leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.size() == gdtCount,
                   "zone-cancelled add GD&T leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "zone-cancelled add GD&T leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "zone-cancelled add GD&T leaves measures unchanged");
}

TEST("main window add gdt action creates gdt") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(!initial.parts.front().features.empty(),
                   "starter UI model has a feature");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QDialog* typeDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(typeDialog != nullptr,
                       "add GD&T type dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* rangeDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(rangeDialog != nullptr,
                           "add GD&T range dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* zoneDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(zoneDialog != nullptr,
                               "add GD&T zone dialog appears");
                if (zoneDialog != nullptr) {
                    zoneDialog->accept();
                }
            });
            if (rangeDialog != nullptr) {
                rangeDialog->accept();
            }
        });
        if (typeDialog != nullptr) {
            typeDialog->accept();
        }
    });

    QAction* addGdt = actionByText(window, "Add GD&T");
    dvatest::check(addGdt != nullptr, "add GD&T action exists");
    addGdt->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "add GD&T leaves part count unchanged");
    const Part& part = model.parts.front();
    dvatest::check(part.points.size() == pointCount,
                   "add GD&T leaves points unchanged");
    dvatest::check(part.features.size() == featureCount,
                   "add GD&T leaves features unchanged");
    dvatest::check(part.tolerances.size() == toleranceCount,
                   "add GD&T leaves tolerances unchanged");
    dvatest::check(part.gdts.size() == gdtCount + 1,
                   "add GD&T creates one GD&T callout");
    dvatest::check(model.moves.size() == moveCount,
                   "add GD&T leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "add GD&T leaves measures unchanged");

    const GdtDef& gdt = part.gdts.back();
    dvatest::check(gdt.active,
                   "created GD&T is active");
    dvatest::check(gdt.type == GdtType::Position,
                   "created GD&T default type is Position");
    dvatest::checkNear(gdt.range, 1.0, 1e-12,
                       "created GD&T default range");
    dvatest::check(gdt.diametrical,
                   "created GD&T default zone is diametrical");
    dvatest::check(!gdt.features.empty(),
                   "created GD&T references a feature");
    dvatest::check(gdt.features.front() == part.features.back().id,
                   "created GD&T references the latest feature");
}

TEST("main window add gdt action creates gdt in selected part") {
    Model modelWithTargetPart = createStarterModel();
    const PartId targetPartId = addPart(modelWithTargetPart, "Target");
    addCoordinatePoint(modelWithTargetPart, {1.0, 2.0, 3.0}, targetPartId);
    const FeatureId targetFeatureId =
        addFeature(modelWithTargetPart, FeatureKind::Plane, targetPartId);

    ui::MainWindow window;
    window.setModelForTesting(modelWithTargetPart);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    if (navigator != nullptr && targetPart != nullptr) {
        navigator->setCurrentItem(targetPart);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    const std::size_t baseGdtCount = basePart.gdts.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const GdtId expectedGdtId = nextGdtId(initial);

    QTimer::singleShot(0, []() {
        QDialog* typeDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(typeDialog != nullptr,
                       "add GD&T type dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* rangeDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(rangeDialog != nullptr,
                           "add GD&T range dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* zoneDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(zoneDialog != nullptr,
                               "add GD&T zone dialog appears");
                if (zoneDialog != nullptr) {
                    zoneDialog->accept();
                }
            });
            if (rangeDialog != nullptr) {
                rangeDialog->accept();
            }
        });
        if (typeDialog != nullptr) {
            typeDialog->accept();
        }
    });

    QAction* addGdt = actionByText(window, "Add GD&T");
    dvatest::check(addGdt != nullptr, "add GD&T action exists");
    addGdt->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected-part add GD&T leaves part count unchanged");
    dvatest::check(model.parts.front().gdts.size() == baseGdtCount,
                   "selected-part add GD&T leaves base GD&T unchanged");
    const Part& updatedTarget = model.parts.back();
    dvatest::check(updatedTarget.points.size() == targetPointCount,
                   "selected-part add GD&T leaves target points unchanged");
    dvatest::check(updatedTarget.features.size() == targetFeatureCount,
                   "selected-part add GD&T leaves target features unchanged");
    dvatest::check(updatedTarget.tolerances.size() == targetToleranceCount,
                   "selected-part add GD&T leaves target tolerances unchanged");
    dvatest::check(updatedTarget.gdts.size() == targetGdtCount + 1,
                   "selected-part add GD&T creates one target callout");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-part add GD&T leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected-part add GD&T leaves measures unchanged");

    const GdtDef& gdt = updatedTarget.gdts.back();
    dvatest::check(gdt.id == expectedGdtId,
                   "selected-part created GD&T uses next id");
    dvatest::check(gdt.active,
                   "selected-part created GD&T is active");
    dvatest::check(gdt.type == GdtType::Position,
                   "selected-part created GD&T default type is Position");
    dvatest::check(gdt.diametrical,
                   "selected-part created GD&T default zone is diametrical");
    dvatest::check(gdt.features.size() == 1,
                   "selected-part created GD&T references one feature");
    dvatest::check(gdt.features.front() == targetFeatureId,
                   "selected-part created GD&T references target feature");
}

TEST("main window add gdt action creates gdt from selected feature") {
    Model modelWithTargetPart = createStarterModel();
    const PartId targetPartId = addPart(modelWithTargetPart, "Target");
    addCoordinatePoint(modelWithTargetPart, {1.0, 2.0, 3.0}, targetPartId);
    const FeatureId targetFeatureId =
        addFeature(modelWithTargetPart, FeatureKind::Plane, targetPartId);

    ui::MainWindow window;
    window.setModelForTesting(modelWithTargetPart);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    QTreeWidgetItem* targetFeatures = childByText(targetPart, "Features");
    dvatest::check(targetFeatures != nullptr,
                   "target features category exists");
    dvatest::check(targetFeatures != nullptr &&
                       targetFeatures->childCount() >= 2,
                   "target features category shows generated features");
    QTreeWidgetItem* selectedFeature =
        targetFeatures != nullptr && targetFeatures->childCount() > 1
            ? targetFeatures->child(1)
            : nullptr;
    dvatest::check(selectedFeature != nullptr,
                   "target feature item exists");
    if (navigator != nullptr && selectedFeature != nullptr) {
        navigator->setCurrentItem(selectedFeature);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    const std::size_t baseGdtCount = basePart.gdts.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const GdtId expectedGdtId = nextGdtId(initial);

    QTimer::singleShot(0, []() {
        QDialog* typeDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(typeDialog != nullptr,
                       "add GD&T type dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* rangeDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(rangeDialog != nullptr,
                           "add GD&T range dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* zoneDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(zoneDialog != nullptr,
                               "add GD&T zone dialog appears");
                if (zoneDialog != nullptr) {
                    zoneDialog->accept();
                }
            });
            if (rangeDialog != nullptr) {
                rangeDialog->accept();
            }
        });
        if (typeDialog != nullptr) {
            typeDialog->accept();
        }
    });

    QAction* addGdt = actionByText(window, "Add GD&T");
    dvatest::check(addGdt != nullptr, "add GD&T action exists");
    addGdt->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected-feature add GD&T leaves part count unchanged");
    dvatest::check(model.parts.front().gdts.size() == baseGdtCount,
                   "selected-feature add GD&T leaves base GD&T unchanged");
    const Part& updatedTarget = model.parts.back();
    dvatest::check(updatedTarget.points.size() == targetPointCount,
                   "selected-feature add GD&T leaves target points unchanged");
    dvatest::check(updatedTarget.features.size() == targetFeatureCount,
                   "selected-feature add GD&T leaves target features unchanged");
    dvatest::check(updatedTarget.tolerances.size() == targetToleranceCount,
                   "selected-feature add GD&T leaves target tolerances unchanged");
    dvatest::check(updatedTarget.gdts.size() == targetGdtCount + 1,
                   "selected-feature add GD&T creates one target callout");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-feature add GD&T leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "selected-feature add GD&T leaves measures unchanged");

    const GdtDef& gdt = updatedTarget.gdts.back();
    dvatest::check(gdt.id == expectedGdtId,
                   "selected-feature created GD&T uses next id");
    dvatest::check(gdt.type == GdtType::Position,
                   "selected-feature created GD&T default type is Position");
    dvatest::check(gdt.diametrical,
                   "selected-feature created GD&T default zone is diametrical");
    dvatest::check(gdt.features.size() == 1,
                   "selected-feature created GD&T references one feature");
    dvatest::check(gdt.features.front() == targetFeatureId,
                   "selected-feature created GD&T references target feature");
}

TEST("main window add point-point measure action requires two points") {
    Model singlePointModel;
    const PartId partId = addPart(singlePointModel, "Block");
    addCoordinatePoint(singlePointModel, {0.0, 0.0, 0.0}, partId);

    ui::MainWindow window;
    window.setModelForTesting(singlePointModel);
    dvatest::check(window.selectRootForTesting(),
                   "single-point UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "single-point UI model has one part");
    dvatest::check(initial.parts.front().points.size() == 1,
                   "single-point UI model has one point");
    dvatest::check(initial.measures.empty(),
                   "single-point UI model has no measures");
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr,
                       "add measure guard message appears");
        QAbstractButton* okButton =
            box != nullptr ? box->button(QMessageBox::Ok) : nullptr;
        dvatest::check(okButton != nullptr,
                       "add measure guard message has an ok button");
        if (okButton != nullptr) {
            okButton->click();
        }
    });

    QAction* addMeasure = actionByText(window, "Add Point-Point Measure");
    dvatest::check(addMeasure != nullptr,
                   "add point-point measure action exists");
    addMeasure->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "add measure guard leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == 1,
                   "add measure guard leaves point count unchanged");
    dvatest::check(model.parts.front().features.size() == featureCount,
                   "add measure guard leaves features unchanged");
    dvatest::check(model.parts.front().tolerances.size() == toleranceCount,
                   "add measure guard leaves tolerances unchanged");
    dvatest::check(model.parts.front().gdts.size() == gdtCount,
                   "add measure guard leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "add measure guard leaves moves unchanged");
    dvatest::check(model.measures.empty(),
                   "add measure guard leaves measures empty");
}

TEST("main window add point-point measure action creates measure") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(initial.parts.front().points.size() >= 2,
                   "starter UI model has at least two points");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const PointId firstPoint = initial.parts.front().points[0].id;
    const PointId secondPoint = initial.parts.front().points[1].id;

    QAction* addMeasure = actionByText(window, "Add Point-Point Measure");
    dvatest::check(addMeasure != nullptr,
                   "add point-point measure action exists");
    addMeasure->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "add measure leaves part count unchanged");
    const Part& part = model.parts.front();
    dvatest::check(part.points.size() == pointCount,
                   "add measure leaves points unchanged");
    dvatest::check(part.features.size() == featureCount,
                   "add measure leaves features unchanged");
    dvatest::check(part.tolerances.size() == toleranceCount,
                   "add measure leaves tolerances unchanged");
    dvatest::check(part.gdts.size() == gdtCount,
                   "add measure leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "add measure leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount + 1,
                   "add measure creates one measure");

    const MeasureRecord& measure = model.measures.back();
    dvatest::check(measure.def.active,
                   "created measure is active");
    dvatest::check(measure.def.asOutput,
                   "created measure is an output");
    dvatest::check(measure.def.type == MeasureType::PointPoint,
                   "created measure type is Point-Point");
    dvatest::check(measure.def.inputPoints.size() == 2,
                   "created measure has two input points");
    dvatest::check(measure.def.inputPoints[0] == firstPoint,
                   "created measure uses first model point");
    dvatest::check(measure.def.inputPoints[1] == secondPoint,
                   "created measure uses second model point");
    dvatest::check(measure.def.direction.type == DirectionType::TypeIn,
                   "created measure uses typed direction");
    dvatest::checkNear(measure.def.direction.ijk.x, 0.0, 1e-12,
                       "created measure direction X default");
    dvatest::checkNear(measure.def.direction.ijk.y, 0.0, 1e-12,
                       "created measure direction Y default");
    dvatest::checkNear(measure.def.direction.ijk.z, 1.0, 1e-12,
                       "created measure direction Z default");
    dvatest::check(measure.def.dirMode == DirectionMode::ProjectedOnVector,
                   "created measure default direction mode");
    dvatest::checkNear(measure.def.spec.lsl, 0.0, 1e-12,
                       "created measure default LSL");
    dvatest::checkNear(measure.def.spec.usl, 100.0, 1e-12,
                       "created measure default USL");
    dvatest::check(measure.def.spec.lslActive,
                   "created measure default LSL is active");
    dvatest::check(measure.def.spec.uslActive,
                   "created measure default USL is active");
}

TEST("main window add point-point measure action starts from selected point") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    QTreeWidgetItem* partItem =
        parts != nullptr && parts->childCount() > 0 ? parts->child(0)
                                                    : nullptr;
    dvatest::check(partItem != nullptr, "starter part item exists");
    QTreeWidgetItem* points = childByText(partItem, "Points");
    dvatest::check(points != nullptr, "starter points category exists");
    dvatest::check(points != nullptr && points->childCount() >= 2,
                   "starter points category shows at least two points");
    QTreeWidgetItem* selectedPoint =
        points != nullptr && points->childCount() >= 2 ? points->child(1)
                                                       : nullptr;
    dvatest::check(selectedPoint != nullptr, "second point item exists");
    if (navigator != nullptr && selectedPoint != nullptr) {
        navigator->setCurrentItem(selectedPoint);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    dvatest::check(initial.parts.front().points.size() >= 2,
                   "starter UI model has at least two points");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t featureCount = initial.parts.front().features.size();
    const std::size_t toleranceCount =
        initial.parts.front().tolerances.size();
    const std::size_t gdtCount = initial.parts.front().gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const PointId firstPoint = initial.parts.front().points[0].id;
    const PointId secondPoint = initial.parts.front().points[1].id;

    QAction* addMeasure = actionByText(window, "Add Point-Point Measure");
    dvatest::check(addMeasure != nullptr,
                   "add point-point measure action exists");
    addMeasure->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "selected-point add measure leaves part count unchanged");
    const Part& part = model.parts.front();
    dvatest::check(part.points.size() == pointCount,
                   "selected-point add measure leaves points unchanged");
    dvatest::check(part.features.size() == featureCount,
                   "selected-point add measure leaves features unchanged");
    dvatest::check(part.tolerances.size() == toleranceCount,
                   "selected-point add measure leaves tolerances unchanged");
    dvatest::check(part.gdts.size() == gdtCount,
                   "selected-point add measure leaves GD&T callouts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-point add measure leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount + 1,
                   "selected-point add measure creates one measure");

    const MeasureRecord& measure = model.measures.back();
    dvatest::check(measure.def.type == MeasureType::PointPoint,
                   "selected-point created measure type is Point-Point");
    dvatest::check(measure.def.inputPoints.size() == 2,
                   "selected-point created measure has two input points");
    dvatest::check(measure.def.inputPoints[0] == secondPoint,
                   "selected-point created measure starts with selected point");
    dvatest::check(measure.def.inputPoints[1] == firstPoint,
                   "selected-point created measure uses next distinct point");
}

TEST("main window add point-point measure action creates measure in selected part") {
    Model modelWithTargetPart = createStarterModel();
    const PartId targetPartId = addPart(modelWithTargetPart, "Target");
    const PointId targetFirst =
        addCoordinatePoint(modelWithTargetPart, {10.0, 0.0, 0.0},
                           targetPartId);
    const PointId targetSecond =
        addCoordinatePoint(modelWithTargetPart, {20.0, 0.0, 0.0},
                           targetPartId);

    ui::MainWindow window;
    window.setModelForTesting(modelWithTargetPart);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* targetPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(targetPart != nullptr, "target part item exists");
    if (navigator != nullptr && targetPart != nullptr) {
        navigator->setCurrentItem(targetPart);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const Part& basePart = initial.parts.front();
    const Part& target = initial.parts.back();
    dvatest::check(target.id == targetPartId,
                   "target part is the second model part");
    dvatest::check(target.points.size() == 2,
                   "target part has two points");
    const std::size_t basePointCount = basePart.points.size();
    const std::size_t baseFeatureCount = basePart.features.size();
    const std::size_t targetPointCount = target.points.size();
    const std::size_t targetFeatureCount = target.features.size();
    const std::size_t targetToleranceCount = target.tolerances.size();
    const std::size_t targetGdtCount = target.gdts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QAction* addMeasure = actionByText(window, "Add Point-Point Measure");
    dvatest::check(addMeasure != nullptr,
                   "add point-point measure action exists");
    addMeasure->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "selected-part add measure leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == basePointCount,
                   "selected-part add measure leaves base points unchanged");
    dvatest::check(model.parts.front().features.size() == baseFeatureCount,
                   "selected-part add measure leaves base features unchanged");
    const Part& updatedTarget = model.parts.back();
    dvatest::check(updatedTarget.points.size() == targetPointCount,
                   "selected-part add measure leaves target points unchanged");
    dvatest::check(updatedTarget.features.size() == targetFeatureCount,
                   "selected-part add measure leaves target features unchanged");
    dvatest::check(updatedTarget.tolerances.size() == targetToleranceCount,
                   "selected-part add measure leaves target tolerances unchanged");
    dvatest::check(updatedTarget.gdts.size() == targetGdtCount,
                   "selected-part add measure leaves target GD&T unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "selected-part add measure leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount + 1,
                   "selected-part add measure creates one measure");

    const MeasureRecord& measure = model.measures.back();
    dvatest::check(measure.def.type == MeasureType::PointPoint,
                   "selected-part created measure type is Point-Point");
    dvatest::check(measure.def.inputPoints.size() == 2,
                   "selected-part created measure has two input points");
    dvatest::check(measure.def.inputPoints[0] == targetFirst,
                   "selected-part created measure uses first target point");
    dvatest::check(measure.def.inputPoints[1] == targetSecond,
                   "selected-part created measure uses second target point");
}

TEST("main window add transform move action requires two parts") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 1,
                   "starter UI model has one part");
    const std::size_t pointCount = initial.parts.front().points.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "add move guard message appears");
        QAbstractButton* okButton =
            box != nullptr ? box->button(QMessageBox::Ok) : nullptr;
        dvatest::check(okButton != nullptr,
                       "add move guard message has an ok button");
        if (okButton != nullptr) {
            okButton->click();
        }
    });

    QAction* addMove = actionByText(window, "Add Transform Move");
    dvatest::check(addMove != nullptr,
                   "add transform move action exists");
    addMove->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "add move guard leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == pointCount,
                   "add move guard leaves points unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "add move guard leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "add move guard leaves measures unchanged");
}

TEST("main window add transform move action object cancellation keeps model") {
    Model modelWithTwoParts = createStarterModel();
    addPart(modelWithTwoParts, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const std::size_t firstPartPointCount =
        initial.parts.front().points.size();
    const std::size_t secondPartPointCount =
        initial.parts.back().points.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();

    QTimer::singleShot(0, []() {
        QDialog* dialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(dialog != nullptr,
                       "add transform move object dialog appears");
        if (dialog != nullptr) {
            dialog->reject();
        }
    });

    QAction* addMove = actionByText(window, "Add Transform Move");
    dvatest::check(addMove != nullptr,
                   "add transform move action exists");
    addMove->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "object-cancelled add move leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == firstPartPointCount,
                   "object-cancelled add move leaves first-part points unchanged");
    dvatest::check(model.parts.back().points.size() == secondPartPointCount,
                   "object-cancelled add move leaves second-part points unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "object-cancelled add move leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "object-cancelled add move leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "object-cancelled add move leaves variants unchanged");
}

TEST("main window add transform move action target cancellation keeps model") {
    Model modelWithTwoParts = createStarterModel();
    addPart(modelWithTwoParts, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const std::size_t firstPartPointCount =
        initial.parts.front().points.size();
    const std::size_t secondPartPointCount =
        initial.parts.back().points.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();

    QTimer::singleShot(0, []() {
        QDialog* objectDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(objectDialog != nullptr,
                       "add transform move object dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* targetDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(targetDialog != nullptr,
                           "add transform move target dialog appears");
            if (targetDialog != nullptr) {
                targetDialog->reject();
            }
        });
        if (objectDialog != nullptr) {
            objectDialog->accept();
        }
    });

    QAction* addMove = actionByText(window, "Add Transform Move");
    dvatest::check(addMove != nullptr,
                   "add transform move action exists");
    addMove->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "target-cancelled add move leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == firstPartPointCount,
                   "target-cancelled add move leaves first-part points unchanged");
    dvatest::check(model.parts.back().points.size() == secondPartPointCount,
                   "target-cancelled add move leaves second-part points unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "target-cancelled add move leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "target-cancelled add move leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "target-cancelled add move leaves variants unchanged");
}

TEST("main window add transform move action X cancellation keeps model") {
    Model modelWithTwoParts = createStarterModel();
    addPart(modelWithTwoParts, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const std::size_t firstPartPointCount =
        initial.parts.front().points.size();
    const std::size_t secondPartPointCount =
        initial.parts.back().points.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();

    QTimer::singleShot(0, []() {
        QDialog* objectDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(objectDialog != nullptr,
                       "add transform move object dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* targetDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(targetDialog != nullptr,
                           "add transform move target dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* xDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(xDialog != nullptr,
                               "add transform move X dialog appears");
                if (xDialog != nullptr) {
                    xDialog->reject();
                }
            });
            if (targetDialog != nullptr) {
                targetDialog->accept();
            }
        });
        if (objectDialog != nullptr) {
            objectDialog->accept();
        }
    });

    QAction* addMove = actionByText(window, "Add Transform Move");
    dvatest::check(addMove != nullptr,
                   "add transform move action exists");
    addMove->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "X-cancelled add move leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == firstPartPointCount,
                   "X-cancelled add move leaves first-part points unchanged");
    dvatest::check(model.parts.back().points.size() == secondPartPointCount,
                   "X-cancelled add move leaves second-part points unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "X-cancelled add move leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "X-cancelled add move leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "X-cancelled add move leaves variants unchanged");
}

TEST("main window add transform move action Y cancellation keeps model") {
    Model modelWithTwoParts = createStarterModel();
    addPart(modelWithTwoParts, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const std::size_t firstPartPointCount =
        initial.parts.front().points.size();
    const std::size_t secondPartPointCount =
        initial.parts.back().points.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();

    QTimer::singleShot(0, []() {
        QDialog* objectDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(objectDialog != nullptr,
                       "add transform move object dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* targetDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(targetDialog != nullptr,
                           "add transform move target dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* xDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(xDialog != nullptr,
                               "add transform move X dialog appears");
                QTimer::singleShot(0, []() {
                    QDialog* yDialog =
                        qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    dvatest::check(yDialog != nullptr,
                                   "add transform move Y dialog appears");
                    if (yDialog != nullptr) {
                        yDialog->reject();
                    }
                });
                if (xDialog != nullptr) {
                    xDialog->accept();
                }
            });
            if (targetDialog != nullptr) {
                targetDialog->accept();
            }
        });
        if (objectDialog != nullptr) {
            objectDialog->accept();
        }
    });

    QAction* addMove = actionByText(window, "Add Transform Move");
    dvatest::check(addMove != nullptr,
                   "add transform move action exists");
    addMove->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "Y-cancelled add move leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == firstPartPointCount,
                   "Y-cancelled add move leaves first-part points unchanged");
    dvatest::check(model.parts.back().points.size() == secondPartPointCount,
                   "Y-cancelled add move leaves second-part points unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "Y-cancelled add move leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "Y-cancelled add move leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "Y-cancelled add move leaves variants unchanged");
}

TEST("main window add transform move action Z cancellation keeps model") {
    Model modelWithTwoParts = createStarterModel();
    addPart(modelWithTwoParts, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const std::size_t firstPartPointCount =
        initial.parts.front().points.size();
    const std::size_t secondPartPointCount =
        initial.parts.back().points.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();

    QTimer::singleShot(0, []() {
        QDialog* objectDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(objectDialog != nullptr,
                       "add transform move object dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* targetDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(targetDialog != nullptr,
                           "add transform move target dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* xDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(xDialog != nullptr,
                               "add transform move X dialog appears");
                QTimer::singleShot(0, []() {
                    QDialog* yDialog =
                        qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    dvatest::check(yDialog != nullptr,
                                   "add transform move Y dialog appears");
                    QTimer::singleShot(0, []() {
                        QDialog* zDialog =
                            qobject_cast<QDialog*>(
                                QApplication::activeModalWidget());
                        dvatest::check(zDialog != nullptr,
                                       "add transform move Z dialog appears");
                        if (zDialog != nullptr) {
                            zDialog->reject();
                        }
                    });
                    if (yDialog != nullptr) {
                        yDialog->accept();
                    }
                });
                if (xDialog != nullptr) {
                    xDialog->accept();
                }
            });
            if (targetDialog != nullptr) {
                targetDialog->accept();
            }
        });
        if (objectDialog != nullptr) {
            objectDialog->accept();
        }
    });

    QAction* addMove = actionByText(window, "Add Transform Move");
    dvatest::check(addMove != nullptr,
                   "add transform move action exists");
    addMove->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "Z-cancelled add move leaves part count unchanged");
    dvatest::check(model.parts.front().points.size() == firstPartPointCount,
                   "Z-cancelled add move leaves first-part points unchanged");
    dvatest::check(model.parts.back().points.size() == secondPartPointCount,
                   "Z-cancelled add move leaves second-part points unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "Z-cancelled add move leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "Z-cancelled add move leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "Z-cancelled add move leaves variants unchanged");
}

TEST("main window add transform move action creates move") {
    Model modelWithTwoParts = createStarterModel();
    const PartId targetPartId = addPart(modelWithTwoParts, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const PartId objectPartId = initial.parts.front().id;
    dvatest::check(initial.parts.back().id == targetPartId,
                   "target part is second in UI model");
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();

    QTimer::singleShot(0, []() {
        QDialog* objectDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(objectDialog != nullptr,
                       "add transform move object dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* targetDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(targetDialog != nullptr,
                           "add transform move target dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* xDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(xDialog != nullptr,
                               "add transform move X dialog appears");
                QTimer::singleShot(0, []() {
                    QDialog* yDialog =
                        qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    dvatest::check(yDialog != nullptr,
                                   "add transform move Y dialog appears");
                    QTimer::singleShot(0, []() {
                        QDialog* zDialog =
                            qobject_cast<QDialog*>(
                                QApplication::activeModalWidget());
                        dvatest::check(zDialog != nullptr,
                                       "add transform move Z dialog appears");
                        if (zDialog != nullptr) {
                            zDialog->accept();
                        }
                    });
                    if (yDialog != nullptr) {
                        yDialog->accept();
                    }
                });
                if (xDialog != nullptr) {
                    xDialog->accept();
                }
            });
            if (targetDialog != nullptr) {
                targetDialog->accept();
            }
        });
        if (objectDialog != nullptr) {
            objectDialog->accept();
        }
    });

    QAction* addMove = actionByText(window, "Add Transform Move");
    dvatest::check(addMove != nullptr,
                   "add transform move action exists");
    addMove->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == moveCount + 1,
                   "accepted add move creates one move");
    dvatest::check(model.measures.size() == measureCount,
                   "accepted add move leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "accepted add move leaves variants unchanged");
    const MoveDef& move = model.moves.back();
    dvatest::check(move.inputs.type == MoveType::Transform,
                   "accepted add move creates a transform move");
    dvatest::check(move.moveParts.size() == 2,
                   "accepted add move stores object and target parts");
    dvatest::check(move.moveParts[0] == objectPartId,
                   "accepted add move stores object part");
    dvatest::check(move.moveParts[1] == targetPartId,
                   "accepted add move stores target part");
    dvatest::check(move.inputs.pairs.size() == 1,
                   "accepted add move stores one move pair");
    dvatest::checkNear(move.inputs.pairs.front().objectPoint.x, 0.0,
                       1e-12, "accepted add move object X defaults to zero");
    dvatest::checkNear(move.inputs.pairs.front().objectPoint.y, 0.0,
                       1e-12, "accepted add move object Y defaults to zero");
    dvatest::checkNear(move.inputs.pairs.front().objectPoint.z, 0.0,
                       1e-12, "accepted add move object Z defaults to zero");
    dvatest::checkNear(move.inputs.pairs.front().targetPoint.x, 10.0,
                       1e-12, "accepted add move target X defaults to ten");
    dvatest::checkNear(move.inputs.pairs.front().targetPoint.y, 0.0,
                       1e-12, "accepted add move target Y defaults to zero");
    dvatest::checkNear(move.inputs.pairs.front().targetPoint.z, 0.0,
                       1e-12, "accepted add move target Z defaults to zero");
}

TEST("main window add transform move action defaults object to selected part") {
    Model modelWithTwoParts = createStarterModel();
    const PartId selectedPartId = addPart(modelWithTwoParts, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() == 2,
                   "two-part UI model shows two parts");
    QTreeWidgetItem* selectedPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(selectedPart != nullptr, "selected part item exists");
    if (navigator != nullptr && selectedPart != nullptr) {
        navigator->setCurrentItem(selectedPart);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const PartId fallbackTargetId = initial.parts.front().id;
    dvatest::check(initial.parts.back().id == selectedPartId,
                   "selected part is second in UI model");
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();
    const std::size_t variantCount = initial.variants.size();

    QTimer::singleShot(0, []() {
        QDialog* objectDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(objectDialog != nullptr,
                       "selected-part add move object dialog appears");
        QTimer::singleShot(0, []() {
            QDialog* targetDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(targetDialog != nullptr,
                           "selected-part add move target dialog appears");
            QTimer::singleShot(0, []() {
                QDialog* xDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(xDialog != nullptr,
                               "selected-part add move X dialog appears");
                QTimer::singleShot(0, []() {
                    QDialog* yDialog =
                        qobject_cast<QDialog*>(
                            QApplication::activeModalWidget());
                    dvatest::check(yDialog != nullptr,
                                   "selected-part add move Y dialog appears");
                    QTimer::singleShot(0, []() {
                        QDialog* zDialog =
                            qobject_cast<QDialog*>(
                                QApplication::activeModalWidget());
                        dvatest::check(zDialog != nullptr,
                                       "selected-part add move Z dialog appears");
                        if (zDialog != nullptr) {
                            zDialog->accept();
                        }
                    });
                    if (yDialog != nullptr) {
                        yDialog->accept();
                    }
                });
                if (xDialog != nullptr) {
                    xDialog->accept();
                }
            });
            if (targetDialog != nullptr) {
                targetDialog->accept();
            }
        });
        if (objectDialog != nullptr) {
            objectDialog->accept();
        }
    });

    QAction* addMove = actionByText(window, "Add Transform Move");
    dvatest::check(addMove != nullptr,
                   "add transform move action exists");
    addMove->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == moveCount + 1,
                   "selected-part add move creates one move");
    dvatest::check(model.measures.size() == measureCount,
                   "selected-part add move leaves measures unchanged");
    dvatest::check(model.variants.size() == variantCount,
                   "selected-part add move leaves variants unchanged");
    const MoveDef& move = model.moves.back();
    dvatest::check(move.inputs.type == MoveType::Transform,
                   "selected-part add move creates a transform move");
    dvatest::check(move.moveParts.size() == 2,
                   "selected-part add move stores object and target parts");
    dvatest::check(move.moveParts[0] == selectedPartId,
                   "selected-part add move stores selected object part");
    dvatest::check(move.moveParts[1] == fallbackTargetId,
                   "selected-part add move stores fallback target part");
}

TEST("main window add transform move action updates target default after object change") {
    Model modelWithTwoParts = createStarterModel();
    const PartId selectedPartId = addPart(modelWithTwoParts, "Target");

    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts);
    dvatest::check(window.selectRootForTesting(),
                   "two-part UI model has an assembly root item");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root =
        navigator != nullptr ? navigator->topLevelItem(0) : nullptr;
    dvatest::check(root != nullptr, "navigator has a root item");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    QTreeWidgetItem* selectedPart =
        parts != nullptr && parts->childCount() > 1 ? parts->child(1)
                                                    : nullptr;
    dvatest::check(selectedPart != nullptr, "selected part item exists");
    if (navigator != nullptr && selectedPart != nullptr) {
        navigator->setCurrentItem(selectedPart);
    }

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "two-part UI model has two parts");
    const PartId changedObjectId = initial.parts.front().id;
    dvatest::check(initial.parts.back().id == selectedPartId,
                   "selected part is second in UI model");
    const std::size_t moveCount = initial.moves.size();

    QTimer::singleShot(0, []() {
        QDialog* objectDialog =
            qobject_cast<QDialog*>(QApplication::activeModalWidget());
        dvatest::check(objectDialog != nullptr,
                       "changed-object add move object dialog appears");
        QComboBox* objectCombo =
            objectDialog != nullptr ? objectDialog->findChild<QComboBox*>()
                                    : nullptr;
        dvatest::check(objectCombo != nullptr,
                       "changed-object add move object combo exists");
        if (objectCombo != nullptr) {
            dvatest::check(objectCombo->currentIndex() == 1,
                           "changed-object add move starts on selected part");
            objectCombo->setCurrentIndex(0);
        }
        QTimer::singleShot(0, []() {
            QDialog* targetDialog =
                qobject_cast<QDialog*>(QApplication::activeModalWidget());
            dvatest::check(targetDialog != nullptr,
                           "changed-object add move target dialog appears");
            QComboBox* targetCombo =
                targetDialog != nullptr ? targetDialog->findChild<QComboBox*>()
                                        : nullptr;
            dvatest::check(targetCombo != nullptr,
                           "changed-object add move target combo exists");
            if (targetCombo != nullptr) {
                dvatest::check(targetCombo->currentIndex() == 1,
                               "changed-object add move target defaults away from object");
            }
            QTimer::singleShot(0, []() {
                QDialog* xDialog =
                    qobject_cast<QDialog*>(QApplication::activeModalWidget());
                dvatest::check(xDialog != nullptr,
                               "changed-object add move X dialog appears");
                QTimer::singleShot(0, []() {
                    QDialog* yDialog =
                        qobject_cast<QDialog*>(
                            QApplication::activeModalWidget());
                    dvatest::check(yDialog != nullptr,
                                   "changed-object add move Y dialog appears");
                    QTimer::singleShot(0, []() {
                        QDialog* zDialog =
                            qobject_cast<QDialog*>(
                                QApplication::activeModalWidget());
                        dvatest::check(zDialog != nullptr,
                                       "changed-object add move Z dialog appears");
                        if (zDialog != nullptr) {
                            zDialog->accept();
                        }
                    });
                    if (yDialog != nullptr) {
                        yDialog->accept();
                    }
                });
                if (xDialog != nullptr) {
                    xDialog->accept();
                }
            });
            if (targetDialog != nullptr) {
                targetDialog->accept();
            }
        });
        if (objectDialog != nullptr) {
            objectDialog->accept();
        }
    });

    QAction* addMove = actionByText(window, "Add Transform Move");
    dvatest::check(addMove != nullptr,
                   "add transform move action exists");
    addMove->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == moveCount + 1,
                   "changed-object add move creates one move");
    const MoveDef& move = model.moves.back();
    dvatest::check(move.moveParts.size() == 2,
                   "changed-object add move stores object and target parts");
    dvatest::check(move.moveParts[0] == changedObjectId,
                   "changed-object add move stores changed object part");
    dvatest::check(move.moveParts[1] == selectedPartId,
                   "changed-object add move stores selected target part");
}

TEST("main window move reorder actions require move selection") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    QAction* moveUp = actionByText(window, "Move Selected Move Up");
    dvatest::check(moveUp != nullptr, "move-up action exists");
    moveUp->trigger();
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Select a move to reorder",
                   "move-up action reports missing move selection");

    QAction* moveDown = actionByText(window, "Move Selected Move Down");
    dvatest::check(moveDown != nullptr, "move-down action exists");
    moveDown->trigger();
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Select a move to reorder",
                   "move-down action reports missing move selection");

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == 2,
                   "move reorder guard leaves move count unchanged");
    dvatest::checkNear(model.moves[0].inputs.pairs.front().targetPoint.x,
                       1.0, 1e-12,
                       "move reorder guard leaves first move in place");
    dvatest::checkNear(model.moves[1].inputs.pairs.front().targetPoint.x,
                       4.0, 1e-12,
                       "move reorder guard leaves second move in place");
}

TEST("main window move up action leaves first move in place") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    QAction* moveUp = actionByText(window, "Move Selected Move Up");
    dvatest::check(moveUp != nullptr, "move-up action exists");
    moveUp->trigger();
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Move is already first",
                   "move-up action reports first move boundary");

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == 2,
                   "first-move guard leaves move count unchanged");
    dvatest::checkNear(model.moves[0].inputs.pairs.front().targetPoint.x,
                       1.0, 1e-12,
                       "first-move guard leaves first move in place");
    dvatest::checkNear(model.moves[1].inputs.pairs.front().targetPoint.x,
                       4.0, 1e-12,
                       "first-move guard leaves second move in place");
}

TEST("main window move down action leaves last move in place") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* moves = childByText(root, "Moves");
    dvatest::check(moves != nullptr, "moves category exists");
    dvatest::check(moves->childCount() >= 2,
                   "starter UI model has a second move");
    navigator->setCurrentItem(moves->child(1));

    QAction* moveDown = actionByText(window, "Move Selected Move Down");
    dvatest::check(moveDown != nullptr, "move-down action exists");
    moveDown->trigger();
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Move is already last",
                   "move-down action reports last move boundary");

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == 2,
                   "last-move guard leaves move count unchanged");
    dvatest::checkNear(model.moves[0].inputs.pairs.front().targetPoint.x,
                       1.0, 1e-12,
                       "last-move guard leaves first move in place");
    dvatest::checkNear(model.moves[1].inputs.pairs.front().targetPoint.x,
                       4.0, 1e-12,
                       "last-move guard leaves second move in place");
}

TEST("main window move up action reorders and keeps moved move selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* moves = childByText(root, "Moves");
    dvatest::check(moves != nullptr, "moves category exists");
    dvatest::check(moves->childCount() >= 2,
                   "starter UI model has a second move");
    QTreeWidgetItem* movedItem = moves->child(1);
    dvatest::check(movedItem != nullptr, "second move item exists");
    const Model& initial = window.modelForTesting();
    dvatest::check(initial.moves.size() == 2,
                   "starter UI model has two moves");
    const MoveId movedId = initial.moves.size() > 1 ? initial.moves[1].id
                                                    : kInvalidId;
    const QString movedName = initial.moves.size() > 1
                                  ? QString::fromStdString(initial.moves[1].name)
                                  : QString();
    if (navigator != nullptr && movedItem != nullptr) {
        navigator->setCurrentItem(movedItem);
    }

    QAction* moveUp = actionByText(window, "Move Selected Move Up");
    dvatest::check(moveUp != nullptr, "move-up action exists");
    moveUp->trigger();
    dvatest::check(window.statusBar()->currentMessage() == "Move reordered",
                   "move-up action reports reorder");

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == 2,
                   "move-up reorder keeps move count");
    dvatest::check(model.moves.front().id == movedId,
                   "move-up reorder moves selected move to first position");
    dvatest::checkNear(model.moves[0].inputs.pairs.front().targetPoint.x,
                       4.0, 1e-12,
                       "move-up reorder moves second move contents first");
    dvatest::checkNear(model.moves[1].inputs.pairs.front().targetPoint.x,
                       1.0, 1e-12,
                       "move-up reorder moves first move contents second");

    QTreeWidgetItem* selected = navigator != nullptr ? navigator->currentItem()
                                                     : nullptr;
    dvatest::check(selected != nullptr,
                   "move-up reorder leaves a navigator item selected");
    if (selected != nullptr) {
        dvatest::check(selected->text(0) == movedName,
                       "move-up reorder keeps moved move selected");
    }
}

TEST("main window navigator move drag sync reorders model and keeps moved move selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.moves.size() == 2,
                   "starter UI model has two moves");
    const MoveId movedId = initial.moves.size() > 1 ? initial.moves[1].id
                                                    : kInvalidId;
    const QString movedName = initial.moves.size() > 1
                                  ? QString::fromStdString(initial.moves[1].name)
                                  : QString();

    dvatest::check(window.moveNavigatorMoveForTesting(1, 0),
                   "navigator drag simulation moves second move before first");

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == 2,
                   "navigator drag sync keeps move count");
    dvatest::check(model.moves.front().id == movedId,
                   "navigator drag sync moves selected move to first position");
    dvatest::checkNear(model.moves[0].inputs.pairs.front().targetPoint.x,
                       4.0, 1e-12,
                       "navigator drag sync moves second move contents first");
    dvatest::checkNear(model.moves[1].inputs.pairs.front().targetPoint.x,
                       1.0, 1e-12,
                       "navigator drag sync moves first move contents second");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    QTreeWidgetItem* selected = navigator != nullptr ? navigator->currentItem()
                                                     : nullptr;
    dvatest::check(selected != nullptr,
                   "navigator drag sync leaves a navigator item selected");
    if (selected != nullptr) {
        dvatest::check(selected->text(0) == movedName,
                       "navigator drag sync keeps moved move selected");
    }
}

TEST("main window navigator no-op move drag keeps model clean") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.moves.size() == 2,
                   "starter UI model has two moves");
    const MoveId firstMove = initial.moves.size() > 0 ? initial.moves[0].id
                                                      : kInvalidId;
    const MoveId secondMove = initial.moves.size() > 1 ? initial.moves[1].id
                                                       : kInvalidId;
    dvatest::check(!window.windowTitle().contains("*"),
                   "fresh test window starts clean");

    dvatest::check(window.moveNavigatorMoveForTesting(0, 0),
                   "navigator drag simulation accepts no-op move");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Move order unchanged",
                   "no-op move drag reports unchanged order");
    dvatest::check(!window.windowTitle().contains("*"),
                   "no-op move drag leaves model clean");

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == 2,
                   "no-op move drag keeps move count");
    dvatest::check(model.moves[0].id == firstMove,
                   "no-op move drag leaves first move in place");
    dvatest::check(model.moves[1].id == secondMove,
                   "no-op move drag leaves second move in place");
}

TEST("main window validate model action shows validation issues") {
    Model invalidModel = createStarterModel();
    invalidModel.measures.clear();
    const ValidationSummary expectedSummary =
        summarizeIssues(validateModelForSimulation(invalidModel));
    const QString expectedStatus =
        QString("Validation: %1 error(s), %2 warning(s), %3 info")
            .arg(expectedSummary.errorCount)
            .arg(expectedSummary.warningCount)
            .arg(expectedSummary.infoCount);

    ui::MainWindow window;
    window.setModelForTesting(invalidModel);
    dvatest::check(window.selectRootForTesting(),
                   "validation test model has a root item");

    QAction* validate = actionByText(window, "Validate Model");
    dvatest::check(validate != nullptr, "validate model action exists");
    validate->trigger();
    QApplication::processEvents();

    QDialog* dialog = nullptr;
    const QList<QDialog*> dialogs = window.findChildren<QDialog*>();
    for (QDialog* candidate : dialogs) {
        if (candidate != nullptr &&
            candidate->windowTitle() == "Model Validation") {
            dialog = candidate;
            break;
        }
    }
    dvatest::check(dialog != nullptr, "validation dialog appears");
    QTableWidget* table =
        dialog != nullptr ? dialog->findChild<QTableWidget*>() : nullptr;
    dvatest::check(table != nullptr, "validation dialog has an issue table");

    bool foundMissingMeasures = false;
    if (table != nullptr) {
        for (int row = 0; row < table->rowCount(); ++row) {
            QTableWidgetItem* code = table->item(row, 2);
            if (code != nullptr && code->text() == "model.no_active_measures") {
                foundMissingMeasures = true;
            }
        }
    }
    dvatest::check(foundMissingMeasures,
                   "validation dialog lists missing active measures issue");
    dvatest::check(window.statusBar()->currentMessage() == expectedStatus,
                   "validate model action reports issue counts");
    if (dialog != nullptr) {
        dialog->close();
    }
}

TEST("validation results dialog exposes empty-state summary") {
    ui::ValidationResultsDialog dialog({});

    QLabel* summary = dialog.findChild<QLabel*>("summaryLabel");
    QLabel* categories = dialog.findChild<QLabel*>("categoryLabel");
    QLabel* emptyState = dialog.findChild<QLabel*>("emptyStateLabel");
    QTableWidget* table = dialog.findChild<QTableWidget*>("issueTable");

    dvatest::check(summary != nullptr,
                   "validation dialog exposes summary label");
    dvatest::check(categories != nullptr,
                   "validation dialog exposes category label");
    dvatest::check(emptyState != nullptr,
                   "validation dialog exposes empty-state label");
    dvatest::check(table != nullptr, "validation dialog exposes issue table");

    dvatest::check(summary != nullptr &&
                       summary->text() == "Errors 0   Warnings 0   Info 0",
                   "validation dialog shows zero summary counts");
    dvatest::check(categories != nullptr &&
                       categories->text() == "No categorized issues.",
                   "validation dialog shows empty category summary");
    dvatest::check(emptyState != nullptr &&
                       emptyState->text() == "No validation issues found.",
                   "validation dialog shows empty-state message");
    dvatest::check(table != nullptr && table->rowCount() == 0,
                   "validation dialog issue table starts empty");
}

TEST("validation results dialog exposes stable dialog object name") {
    ui::ValidationResultsDialog dialog({});

    dvatest::check(dialog.objectName() == "validationResultsDialog",
                   "validation dialog exposes stable object name");
}

TEST("validation results dialog exposes stable issue table header") {
    ui::ValidationResultsDialog dialog({});

    QHeaderView* header = dialog.findChild<QHeaderView*>("issueTableHeader");
    dvatest::check(header != nullptr,
                   "validation dialog exposes issue table header");
    dvatest::check(header != nullptr && header->count() == 4,
                   "validation dialog issue table header exposes issue columns");
}

TEST("validation results dialog exposes stable button box") {
    ui::ValidationResultsDialog dialog({});

    QDialogButtonBox* buttons =
        dialog.findChild<QDialogButtonBox*>("validationResultsButtonBox");
    dvatest::check(buttons != nullptr,
                   "validation dialog exposes button box");
    dvatest::check(buttons != nullptr &&
                       buttons->button(QDialogButtonBox::Close) != nullptr,
                   "validation dialog exposes Close button");
}

TEST("validation results dialog exposes stable close button") {
    ui::ValidationResultsDialog dialog({});

    QAbstractButton* close =
        dialog.findChild<QAbstractButton*>("validationResultsCloseButton");
    dvatest::check(close != nullptr,
                   "validation dialog exposes stable close button");
    dvatest::check(close != nullptr && close->text() == "Close",
                   "validation dialog labels stable close button");
}

TEST("color contour legend exposes range controls and clear action") {
    ui::ColorContourLegendWidget legend;
    legend.setRangeControls(true, -2.5, 3.5);

    QCheckBox* autoScale = legend.findChild<QCheckBox*>("autoScaleCheck");
    QDoubleSpinBox* minSpin =
        legend.findChild<QDoubleSpinBox*>("manualMinSpin");
    QDoubleSpinBox* maxSpin =
        legend.findChild<QDoubleSpinBox*>("manualMaxSpin");
    QAbstractButton* clear =
        legend.findChild<QAbstractButton*>("clearContourButton");

    dvatest::check(autoScale != nullptr,
                   "contour legend exposes auto-scale checkbox");
    dvatest::check(minSpin != nullptr,
                   "contour legend exposes manual min spin");
    dvatest::check(maxSpin != nullptr,
                   "contour legend exposes manual max spin");
    dvatest::check(clear != nullptr,
                   "contour legend exposes clear button");

    dvatest::check(autoScale != nullptr && autoScale->isChecked(),
                   "contour legend starts in auto-scale mode");
    dvatest::check(minSpin != nullptr && !minSpin->isEnabled(),
                   "auto-scale disables manual min");
    dvatest::check(maxSpin != nullptr && !maxSpin->isEnabled(),
                   "auto-scale disables manual max");

    int rangeSignals = 0;
    bool emittedAuto = true;
    double emittedMin = 0.0;
    double emittedMax = 0.0;
    QObject::connect(&legend, &ui::ColorContourLegendWidget::rangeSettingsChanged,
                     [&](bool autoScaleValue, double minValue, double maxValue) {
                         ++rangeSignals;
                         emittedAuto = autoScaleValue;
                         emittedMin = minValue;
                         emittedMax = maxValue;
                     });

    if (autoScale != nullptr) autoScale->setChecked(false);
    dvatest::check(minSpin != nullptr && minSpin->isEnabled(),
                   "manual range enables min spin");
    dvatest::check(maxSpin != nullptr && maxSpin->isEnabled(),
                   "manual range enables max spin");
    dvatest::check(rangeSignals == 1 && !emittedAuto &&
                       emittedMin == -2.5 && emittedMax == 3.5,
                   "manual range toggle emits current range settings");

    int clearSignals = 0;
    QObject::connect(&legend, &ui::ColorContourLegendWidget::clearRequested,
                     [&]() { ++clearSignals; });
    if (clear != nullptr) clear->click();
    dvatest::check(clearSignals == 1,
                   "contour legend clear button emits clear request");
}

TEST("model viewport clear contour hides legend") {
    ui::ModelViewport viewport;
    viewport.setPointDeviations({{101, 2.0}}, 0.0, 4.0);

    auto* legend =
        viewport.findChild<ui::ColorContourLegendWidget*>("contourLegend");
    dvatest::check(legend != nullptr, "model viewport exposes contour legend");
    dvatest::check(legend != nullptr && !legend->isHidden(),
                   "model viewport shows contour legend for deviations");

    QAbstractButton* clear =
        legend != nullptr
            ? legend->findChild<QAbstractButton*>("clearContourButton")
            : nullptr;
    dvatest::check(clear != nullptr,
                   "model viewport exposes contour clear action");
    if (clear != nullptr) clear->click();

    dvatest::check(legend != nullptr && legend->isHidden(),
                   "model viewport hides contour legend after clear");
}

TEST("main window exposes central model viewport") {
    ui::MainWindow window;

    ui::ModelViewport* viewport =
        window.findChild<ui::ModelViewport*>("modelViewport");
    dvatest::check(viewport != nullptr,
                   "main window exposes central model viewport");

    auto* legend =
        viewport != nullptr
            ? viewport->findChild<ui::ColorContourLegendWidget*>("contourLegend")
            : nullptr;
    dvatest::check(legend != nullptr,
                   "main window viewport exposes contour legend");
    dvatest::check(legend != nullptr && legend->isHidden(),
                   "main window contour legend starts hidden");
}

TEST("main window exposes shell widgets") {
    ui::MainWindow window;

    QMenuBar* menuBar = window.findChild<QMenuBar*>("mainMenuBar");
    QStatusBar* statusBar = window.findChild<QStatusBar*>("mainStatusBar");

    dvatest::check(menuBar != nullptr, "main window exposes menu bar");
    dvatest::check(statusBar != nullptr, "main window exposes status bar");
    dvatest::check(statusBar != nullptr && statusBar->currentMessage() == "Ready",
                   "main status bar starts ready");
}

TEST("main window exposes primary toolbar actions") {
    ui::MainWindow window;

    QToolBar* toolbar = window.findChild<QToolBar*>("mainToolBar");
    dvatest::check(toolbar != nullptr, "main window exposes primary toolbar");

    QStringList actionLabels;
    if (toolbar != nullptr) {
        for (QAction* action : toolbar->actions()) {
            if (action != nullptr) actionLabels << normalizedActionText(action->text());
        }
    }

    const QStringList expected{"New",     "Open",    "Save",    "Point",
                               "Feature", "Tolerance", "GD&T",  "Move",
                               "Measure", "Variant", "Validate", "Run",
                               "Batch"};
    for (const QString& label : expected) {
        dvatest::check(actionLabels.contains(label),
                       ("main toolbar exposes " + label).toStdString());
    }
}

TEST("main window exposes toolbar command action names") {
    ui::MainWindow window;

    QToolBar* toolbar = window.findChild<QToolBar*>("mainToolBar");
    dvatest::check(toolbar != nullptr, "main window exposes named toolbar");

    const std::vector<std::pair<const char*, QString>> expected{
        {"newModelToolAction", "New"},
        {"openModelToolAction", "Open"},
        {"saveModelToolAction", "Save"},
        {"addPointToolAction", "Point"},
        {"addFeatureToolAction", "Feature"},
        {"addLinearToleranceToolAction", "Tolerance"},
        {"addGdtToolAction", "GD&T"},
        {"addTransformMoveToolAction", "Move"},
        {"addPointPointMeasureToolAction", "Measure"},
        {"captureModelVariantToolAction", "Variant"},
        {"validateModelToolAction", "Validate"},
        {"runMonteCarloToolAction", "Run"},
        {"runBatchProcessorToolAction", "Batch"}};

    auto toolbarAction = [toolbar](const char* name) -> QAction* {
        if (toolbar == nullptr) return nullptr;
        for (QAction* action : toolbar->actions()) {
            if (action != nullptr && action->objectName() == name) return action;
        }
        return nullptr;
    };

    for (const auto& entry : expected) {
        QAction* action = toolbarAction(entry.first);
        dvatest::check(action != nullptr,
                       std::string("main toolbar exposes ") + entry.first);
        dvatest::check(action != nullptr &&
                           normalizedActionText(action->text()) == entry.second,
                       std::string("main toolbar labels ") + entry.first);
    }
}

TEST("main toolbar actions use generated icon assets") {
    ui::MainWindow window;

    QToolBar* toolbar = window.findChild<QToolBar*>("mainToolBar");
    dvatest::check(toolbar != nullptr, "main window exposes named toolbar");

    const QStringList expected{
        "newModelToolAction",
        "openModelToolAction",
        "saveModelToolAction",
        "addPointToolAction",
        "addFeatureToolAction",
        "addLinearToleranceToolAction",
        "addGdtToolAction",
        "addTransformMoveToolAction",
        "addPointPointMeasureToolAction",
        "captureModelVariantToolAction",
        "validateModelToolAction",
        "runMonteCarloToolAction",
        "runBatchProcessorToolAction"};

    for (const QString& name : expected) {
        QAction* action = toolbar != nullptr ? toolbar->findChild<QAction*>(name) : nullptr;
        if (action == nullptr && toolbar != nullptr) {
            for (QAction* candidate : toolbar->actions()) {
                if (candidate != nullptr && candidate->objectName() == name) {
                    action = candidate;
                    break;
                }
            }
        }
        dvatest::check(action != nullptr,
                       ("main toolbar finds " + name).toStdString());
        dvatest::check(action != nullptr && !action->icon().isNull(),
                       ("main toolbar has generated icon for " + name)
                           .toStdString());
    }
}

TEST("main window applies product design techno theme") {
    ui::MainWindow window;

    const QString style = window.styleSheet();
    dvatest::check(style.contains("OpenDVA Tech3D"),
                   "main window stylesheet declares Tech3D theme");
    dvatest::check(style.contains("QToolBar#mainToolBar"),
                   "Tech3D theme styles the main toolbar");
    dvatest::check(style.contains("QDockWidget"),
                   "Tech3D theme styles dock surfaces");
    dvatest::check(style.contains("QTableWidget"),
                   "Tech3D theme styles data tables");
}

TEST("main window exposes primary menus") {
    ui::MainWindow window;

    QMenu* fileMenu = window.findChild<QMenu*>("fileMenu");
    QMenu* recentMenu = window.findChild<QMenu*>("recentFilesMenu");
    QMenu* analysisMenu = window.findChild<QMenu*>("analysisMenu");
    QMenu* toolsMenu = window.findChild<QMenu*>("toolsMenu");
    QMenu* modelMenu = window.findChild<QMenu*>("modelMenu");

    dvatest::check(fileMenu != nullptr, "main window exposes file menu");
    dvatest::check(recentMenu != nullptr,
                   "main window exposes recent files menu");
    dvatest::check(analysisMenu != nullptr,
                   "main window exposes analysis menu");
    dvatest::check(toolsMenu != nullptr, "main window exposes tools menu");
    dvatest::check(modelMenu != nullptr, "main window exposes model menu");

    auto menuActions = [](QMenu* menu) {
        QStringList labels;
        if (menu == nullptr) return labels;
        for (QAction* action : menu->actions()) {
            if (action != nullptr) labels << normalizedActionText(action->text());
        }
        return labels;
    };

    dvatest::check(menuActions(fileMenu).contains("New"),
                   "file menu exposes New");
    dvatest::check(menuActions(fileMenu).contains("Open..."),
                   "file menu exposes Open");
    dvatest::check(menuActions(analysisMenu).contains("Validate Model"),
                   "analysis menu exposes validation");
    dvatest::check(menuActions(analysisMenu).contains("Run Monte Carlo"),
                   "analysis menu exposes run");
    dvatest::check(menuActions(toolsMenu).contains("Preferences..."),
                   "tools menu exposes preferences");
    dvatest::check(menuActions(modelMenu).contains("Add Part"),
                   "model menu exposes Add Part");
}

TEST("main window exposes documented module window menus") {
    ui::MainWindow window;

    const std::vector<const char*> expectedMenus{
        "displayMenu",
        "reportMenu",
        "aaoMenu",
        "mechanicalMenu",
        "feaMenu",
        "helpMenu"};

    for (const char* name : expectedMenus) {
        QMenu* menu = window.findChild<QMenu*>(name);
        dvatest::check(menu != nullptr,
                       std::string("main window exposes ") + name);
    }

    const std::vector<const char*> expectedActions{
        "displayOptionsAction",
        "animationWindowAction",
        "generateReportAction",
        "specStudyAction",
        "geoFactorMatrixAction",
        "ctiAnalyzerAction",
        "toleranceOptimizerAction",
        "sequenceOptimizerAction",
        "datumOptimizerAction",
        "dofCounterAction",
        "kinematicsAction",
        "collisionDetectionAction",
        "stiffGenAction",
        "loadFeaDataAction",
        "pointLinkWizardAction",
        "validateCompliantModelAction",
        "userDllManagerAction",
        "welcomeAction",
        "licenseStatusAction",
        "systemInformationAction"};

    for (const char* name : expectedActions) {
        QAction* action = window.findChild<QAction*>(name);
        dvatest::check(action != nullptr,
                       std::string("main window exposes ") + name);
    }
}

TEST("main window documented module actions open overview windows") {
    ui::MainWindow window;

    QAction* action = window.findChild<QAction*>("geoFactorMatrixAction");
    dvatest::check(action != nullptr,
                   "main window exposes GeoFactor Matrix action");
    if (action != nullptr) action->trigger();

    QDialog* dialog = window.findChild<QDialog*>("geoFactorMatrixWindow");
    dvatest::check(dialog != nullptr,
                   "GeoFactor Matrix action opens its module window");
    dvatest::check(dialog != nullptr &&
                       dialog->windowTitle() == "GeoFactor Matrix",
                   "GeoFactor Matrix module window has expected title");
}

TEST("main window exposes navigator and properties docks") {
    ui::MainWindow window;

    QDockWidget* navigatorDock =
        window.findChild<QDockWidget*>("ModelNavigatorDock");
    QDockWidget* propertiesDock =
        window.findChild<QDockWidget*>("PropertiesDock");
    QTreeWidget* navigator = window.findChild<QTreeWidget*>("modelNavigator");
    QTableWidget* properties = window.findChild<QTableWidget*>("propertyTable");

    dvatest::check(navigatorDock != nullptr,
                   "main window exposes navigator dock");
    dvatest::check(propertiesDock != nullptr,
                   "main window exposes properties dock");
    dvatest::check(navigatorDock != nullptr &&
                       navigatorDock->windowTitle() == "Model Navigator",
                   "navigator dock has expected title");
    dvatest::check(propertiesDock != nullptr &&
                       propertiesDock->windowTitle() == "Properties",
                   "properties dock has expected title");
    dvatest::check(navigator != nullptr,
                   "main window exposes navigator tree");
    dvatest::check(properties != nullptr,
                   "main window exposes property table");
    dvatest::check(navigator != nullptr &&
                       navigator->headerItem()->text(0) == "Model Navigator",
                   "navigator tree has expected header");
    dvatest::check(properties != nullptr && properties->columnCount() == 2,
                   "property table has property/value columns");
    dvatest::check(properties != nullptr &&
                       properties->horizontalHeaderItem(0)->text() ==
                           "Property" &&
                       properties->horizontalHeaderItem(1)->text() == "Value",
                   "property table exposes property/value headers");
}

TEST("main window exposes navigator context action names") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectRootForTesting(),
                   "main window selects root for context action names");

    const QStringList actionNames =
        window.navigatorContextActionNamesForTesting();

    const QStringList expected{
        "addPartContextAction",
        "addPointContextAction",
        "addFeatureContextAction",
        "addLinearToleranceContextAction",
        "addGdtContextAction",
        "addTransformMoveContextAction",
        "addPointPointMeasureContextAction",
        "moveSelectedMoveUpContextAction",
        "moveSelectedMoveDownContextAction",
        "deleteSelectedContextAction",
        "captureModelVariantContextAction",
        "applyModelVariantContextAction",
        "deleteModelVariantContextAction"};

    for (const QString& name : expected) {
        dvatest::check(actionNames.contains(name),
                       ("navigator context menu exposes " + name).toStdString());
    }
}

TEST("main window exposes navigator context action labels") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectRootForTesting(),
                   "main window selects root for context action labels");

    const QStringList actionTexts =
        window.navigatorContextActionTextsForTesting();

    const QStringList expected{"Add Part",
                               "Add Point",
                               "Add Feature",
                               "Add Linear Tolerance",
                               "Add GD&T",
                               "Add Transform Move",
                               "Add Point-Point Measure",
                               "Move Up",
                               "Move Down",
                               "Delete Selected",
                               "Capture Model Variant",
                               "Apply Model Variant",
                               "Delete Model Variant"};

    for (const QString& text : expected) {
        dvatest::check(actionTexts.contains(text),
                       ("navigator context menu labels " + text).toStdString());
    }
}

TEST("main window navigator context action sequence keeps groups") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectRootForTesting(),
                   "main window selects root for context action sequence");

    const QStringList sequence =
        window.navigatorContextActionSequenceForTesting();

    const QStringList expected{"addPartContextAction",
                               "addPointContextAction",
                               "addFeatureContextAction",
                               "addLinearToleranceContextAction",
                               "addGdtContextAction",
                               "addTransformMoveContextAction",
                               "addPointPointMeasureContextAction",
                               "<separator>",
                               "moveSelectedMoveUpContextAction",
                               "moveSelectedMoveDownContextAction",
                               "deleteSelectedContextAction",
                               "<separator>",
                               "captureModelVariantContextAction",
                               "applyModelVariantContextAction",
                               "deleteModelVariantContextAction"};

    dvatest::check(sequence == expected,
                   "navigator context menu preserves grouped action order");
}

TEST("main window navigator context action enabled states follow selection") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());

    dvatest::check(window.selectRootForTesting(),
                   "main window selects root for context enabled states");
    QStringList enabledNames =
        window.navigatorContextEnabledActionNamesForTesting();
    dvatest::check(!enabledNames.contains("moveSelectedMoveUpContextAction"),
                   "root context disables move up action");
    dvatest::check(!enabledNames.contains("moveSelectedMoveDownContextAction"),
                   "root context disables move down action");
    dvatest::check(!enabledNames.contains("deleteSelectedContextAction"),
                   "root context disables delete action");
    dvatest::check(enabledNames.contains("addPartContextAction"),
                   "root context keeps add part action enabled");

    dvatest::check(window.selectFirstMoveForTesting(),
                   "main window selects move for context enabled states");
    enabledNames = window.navigatorContextEnabledActionNamesForTesting();
    dvatest::check(enabledNames.contains("moveSelectedMoveUpContextAction"),
                   "move context enables move up action");
    dvatest::check(enabledNames.contains("moveSelectedMoveDownContextAction"),
                   "move context enables move down action");
    dvatest::check(enabledNames.contains("deleteSelectedContextAction"),
                   "move context enables delete action");
}

TEST("main window exposes primary command action names") {
    ui::MainWindow window;

    const std::vector<std::pair<const char*, QString>> expected{
        {"newModelAction", "New"},
        {"openModelAction", "Open..."},
        {"saveModelAction", "Save"},
        {"validateModelAction", "Validate Model"},
        {"runMonteCarloAction", "Run Monte Carlo"},
        {"preferencesAction", "Preferences..."},
        {"addPartAction", "Add Part"}};

    for (const auto& entry : expected) {
        QAction* action = window.findChild<QAction*>(entry.first);
        dvatest::check(action != nullptr,
                       std::string("main window exposes ") + entry.first);
        dvatest::check(action != nullptr &&
                           normalizedActionText(action->text()) == entry.second,
                       std::string("main window labels ") + entry.first);
    }
}

TEST("main window exposes secondary command action names") {
    ui::MainWindow window;

    const std::vector<std::pair<const char*, QString>> expected{
        {"saveModelAsAction", "Save As..."},
        {"exitAction", "Exit"},
        {"runBatchProcessorAction", "Batch Processor..."},
        {"addPointAction", "Add Point"},
        {"addFeatureAction", "Add Feature"},
        {"addLinearToleranceAction", "Add Linear Tolerance"},
        {"addGdtAction", "Add GD&T"},
        {"addTransformMoveAction", "Add Transform Move"},
        {"addPointPointMeasureAction", "Add Point-Point Measure"},
        {"moveSelectedMoveUpAction", "Move Selected Move Up"},
        {"moveSelectedMoveDownAction", "Move Selected Move Down"},
        {"captureModelVariantAction", "Capture Model Variant..."},
        {"applyModelVariantAction", "Apply Model Variant..."},
        {"deleteModelVariantAction", "Delete Model Variant..."}};

    for (const auto& entry : expected) {
        QAction* action = window.findChild<QAction*>(entry.first);
        dvatest::check(action != nullptr,
                       std::string("main window exposes ") + entry.first);
        dvatest::check(action != nullptr &&
                           normalizedActionText(action->text()) == entry.second,
                       std::string("main window labels ") + entry.first);
    }
}

TEST("preferences dialog exposes configured defaults") {
    AppPreferences preferences;
    preferences.lengthUnit = LengthUnit::Inch;
    preferences.analysisDefaults.monteCarloEnabled = false;
    preferences.analysisDefaults.contributorEnabled = true;
    preferences.analysisDefaults.totalRuns = 2468;
    preferences.analysisDefaults.initialSeed = 13579;
    preferences.analysisDefaults.threads = 6;
    preferences.defaultReportPath = "reports/custom.html";

    ui::PreferencesDialog dialog(preferences);

    QComboBox* unit = dialog.findChild<QComboBox*>("lengthUnitCombo");
    dvatest::check(unit != nullptr, "preferences dialog exposes unit combo");
    dvatest::check(unit != nullptr && unit->currentText() == "Inch",
                   "preferences dialog shows configured unit");

    QCheckBox* monteCarlo =
        dialog.findChild<QCheckBox*>("monteCarloCheck");
    QCheckBox* contributor =
        dialog.findChild<QCheckBox*>("contributorCheck");
    dvatest::check(monteCarlo != nullptr && !monteCarlo->isChecked(),
                   "preferences dialog shows Monte Carlo default");
    dvatest::check(contributor != nullptr && contributor->isChecked(),
                   "preferences dialog shows Contributor default");

    QSpinBox* totalRuns = dialog.findChild<QSpinBox*>("totalRunsSpin");
    QLineEdit* seed = dialog.findChild<QLineEdit*>("seedEdit");
    QSpinBox* threads = dialog.findChild<QSpinBox*>("threadsSpin");
    QLineEdit* reportPath = dialog.findChild<QLineEdit*>("reportPathEdit");
    dvatest::check(totalRuns != nullptr && totalRuns->value() == 2468,
                   "preferences dialog shows default run count");
    dvatest::check(seed != nullptr && seed->text() == "13579",
                   "preferences dialog shows default seed");
    dvatest::check(threads != nullptr && threads->value() == 6,
                   "preferences dialog shows default thread count");
    dvatest::check(reportPath != nullptr &&
                       reportPath->text() == "reports/custom.html",
                   "preferences dialog shows default report path");
}

TEST("preferences dialog returns edited defaults") {
    AppPreferences preferences;
    preferences.lengthUnit = LengthUnit::Millimeter;
    preferences.analysisDefaults.monteCarloEnabled = true;
    preferences.analysisDefaults.contributorEnabled = false;
    preferences.analysisDefaults.totalRuns = 1000;
    preferences.analysisDefaults.initialSeed = 12345;
    preferences.analysisDefaults.threads = 0;
    preferences.defaultReportPath = "reports/default.html";

    ui::PreferencesDialog dialog(preferences);

    QComboBox* unit = dialog.findChild<QComboBox*>("lengthUnitCombo");
    QCheckBox* monteCarlo =
        dialog.findChild<QCheckBox*>("monteCarloCheck");
    QCheckBox* contributor =
        dialog.findChild<QCheckBox*>("contributorCheck");
    QSpinBox* totalRuns = dialog.findChild<QSpinBox*>("totalRunsSpin");
    QLineEdit* seed = dialog.findChild<QLineEdit*>("seedEdit");
    QSpinBox* threads = dialog.findChild<QSpinBox*>("threadsSpin");
    QLineEdit* reportPath = dialog.findChild<QLineEdit*>("reportPathEdit");
    dvatest::check(unit != nullptr && monteCarlo != nullptr &&
                       contributor != nullptr && totalRuns != nullptr &&
                       seed != nullptr && threads != nullptr &&
                       reportPath != nullptr,
                   "preferences dialog exposes editable controls");

    if (unit != nullptr) unit->setCurrentText("Inch");
    if (monteCarlo != nullptr) monteCarlo->setChecked(false);
    if (contributor != nullptr) contributor->setChecked(true);
    if (totalRuns != nullptr) totalRuns->setValue(4321);
    if (seed != nullptr) seed->setText("98765");
    if (threads != nullptr) threads->setValue(3);
    if (reportPath != nullptr) reportPath->setText(" reports/edited.html ");

    const AppPreferences edited = dialog.preferences();
    dvatest::check(edited.lengthUnit == LengthUnit::Inch,
                   "edited preferences update length unit");
    dvatest::check(!edited.analysisDefaults.monteCarloEnabled,
                   "edited preferences update Monte Carlo flag");
    dvatest::check(edited.analysisDefaults.contributorEnabled,
                   "edited preferences update Contributor flag");
    dvatest::check(edited.analysisDefaults.totalRuns == 4321,
                   "edited preferences update run count");
    dvatest::check(edited.analysisDefaults.initialSeed == 98765,
                   "edited preferences update seed");
    dvatest::check(edited.analysisDefaults.threads == 3,
                   "edited preferences update thread count");
    dvatest::check(edited.defaultReportPath == "reports/edited.html",
                   "edited preferences trim report path");
}

TEST("preferences dialog normalizes malformed seed edit") {
    AppPreferences preferences;
    preferences.analysisDefaults.initialSeed = 12345;

    ui::PreferencesDialog dialog(preferences);
    QLineEdit* seed = dialog.findChild<QLineEdit*>("seedEdit");
    dvatest::check(seed != nullptr, "preferences dialog exposes seed edit");
    if (seed != nullptr) seed->setText("not-a-number");

    const AppPreferences edited = dialog.preferences();
    dvatest::check(edited.analysisDefaults.initialSeed == 1,
                   "malformed preference seed falls back to minimum");
}

TEST("preferences dialog clamps initial thread default") {
    AppPreferences preferences;
    preferences.analysisDefaults.threads = 999;

    ui::PreferencesDialog dialog(preferences);
    QSpinBox* threads = dialog.findChild<QSpinBox*>("threadsSpin");
    dvatest::check(threads != nullptr, "preferences dialog exposes thread spin");
    dvatest::check(threads != nullptr && threads->value() == 64,
                   "preferences dialog clamps thread default");
}

TEST("preferences dialog exposes stable dialog object name") {
    ui::PreferencesDialog dialog(AppPreferences{});

    dvatest::check(dialog.objectName() == "preferencesDialog",
                   "preferences dialog exposes stable object name");
}

TEST("preferences dialog exposes stable button box") {
    ui::PreferencesDialog dialog(AppPreferences{});

    QDialogButtonBox* buttons =
        dialog.findChild<QDialogButtonBox*>("preferencesButtonBox");
    dvatest::check(buttons != nullptr,
                   "preferences dialog exposes button box");
    dvatest::check(buttons != nullptr &&
                       buttons->button(QDialogButtonBox::Ok) != nullptr,
                   "preferences dialog exposes OK button");
    dvatest::check(buttons != nullptr &&
                       buttons->button(QDialogButtonBox::Cancel) != nullptr,
                   "preferences dialog exposes Cancel button");
}

TEST("preferences dialog exposes stable command buttons") {
    ui::PreferencesDialog dialog(AppPreferences{});

    const std::vector<std::pair<const char*, QString>> expected{
        {"preferencesOkButton", "OK"},
        {"preferencesCancelButton", "Cancel"}};

    for (const auto& entry : expected) {
        QAbstractButton* button =
            dialog.findChild<QAbstractButton*>(entry.first);
        dvatest::check(button != nullptr,
                       std::string("preferences dialog exposes ") +
                           entry.first);
        dvatest::check(button != nullptr && button->text() == entry.second,
                       std::string("preferences dialog labels ") +
                           entry.first);
    }
}

TEST("run analysis dialog exposes normalized defaults") {
    SimulationSettings settings;
    settings.monteCarloEnabled = false;
    settings.contributorEnabled = true;
    settings.totalRuns = 0;
    settings.initialSeed = 0;
    settings.threads = 999;

    ui::RunAnalysisDialog dialog(settings);

    QCheckBox* monteCarlo = dialog.findChild<QCheckBox*>("monteCarloCheck");
    QCheckBox* contributor = dialog.findChild<QCheckBox*>("contributorCheck");
    QSpinBox* totalRuns = dialog.findChild<QSpinBox*>("totalRunsSpin");
    QLineEdit* seed = dialog.findChild<QLineEdit*>("seedEdit");
    QSpinBox* threads = dialog.findChild<QSpinBox*>("threadsSpin");

    dvatest::check(monteCarlo != nullptr,
                   "run analysis dialog exposes Monte Carlo checkbox");
    dvatest::check(contributor != nullptr,
                   "run analysis dialog exposes Contributor checkbox");
    dvatest::check(totalRuns != nullptr,
                   "run analysis dialog exposes total runs spin");
    dvatest::check(seed != nullptr, "run analysis dialog exposes seed edit");
    dvatest::check(threads != nullptr,
                   "run analysis dialog exposes thread spin");

    dvatest::check(monteCarlo != nullptr && !monteCarlo->isChecked(),
                   "run analysis dialog shows Monte Carlo setting");
    dvatest::check(contributor != nullptr && contributor->isChecked(),
                   "run analysis dialog shows Contributor setting");
    const SimulationSettings normalized = normalizeSimulationSettings(settings, 64);

    dvatest::check(totalRuns != nullptr &&
                       totalRuns->value() == normalized.totalRuns,
                   "run analysis dialog normalizes total runs");
    dvatest::check(seed != nullptr &&
                       seed->text().toULongLong() == normalized.initialSeed,
                   "run analysis dialog normalizes seed");
    dvatest::check(threads != nullptr && threads->value() == normalized.threads,
                   "run analysis dialog clamps thread default");
}

TEST("run analysis dialog exposes stable dialog object name") {
    ui::RunAnalysisDialog dialog(SimulationSettings{});

    dvatest::check(dialog.objectName() == "runAnalysisDialog",
                   "run analysis dialog exposes stable object name");
}

TEST("run analysis dialog exposes stable button box") {
    ui::RunAnalysisDialog dialog(SimulationSettings{});

    QDialogButtonBox* buttons =
        dialog.findChild<QDialogButtonBox*>("runAnalysisButtonBox");
    dvatest::check(buttons != nullptr,
                   "run analysis dialog exposes button box");
    dvatest::check(buttons != nullptr &&
                       buttons->button(QDialogButtonBox::Ok) != nullptr,
                   "run analysis dialog exposes OK button");
    dvatest::check(buttons != nullptr &&
                       buttons->button(QDialogButtonBox::Cancel) != nullptr,
                   "run analysis dialog exposes Cancel button");
}

TEST("run analysis dialog exposes stable command buttons") {
    ui::RunAnalysisDialog dialog(SimulationSettings{});

    const std::vector<std::pair<const char*, QString>> expected{
        {"runAnalysisOkButton", "OK"},
        {"runAnalysisCancelButton", "Cancel"}};

    for (const auto& entry : expected) {
        QAbstractButton* button =
            dialog.findChild<QAbstractButton*>(entry.first);
        dvatest::check(button != nullptr,
                       std::string("run analysis dialog exposes ") +
                           entry.first);
        dvatest::check(button != nullptr && button->text() == entry.second,
                       std::string("run analysis dialog labels ") +
                           entry.first);
    }
}

TEST("batch processor dialog exposes normalized defaults") {
    SimulationSettings settings;
    settings.totalRuns = 0;
    settings.initialSeed = 0;
    settings.threads = 999;

    ui::BatchProcessorDialog dialog(settings);

    QTableWidget* modelTable = dialog.findChild<QTableWidget*>("modelTable");
    QLineEdit* outputDir = dialog.findChild<QLineEdit*>("outputDirEdit");
    QSpinBox* totalRuns = dialog.findChild<QSpinBox*>("totalRunsSpin");
    QLineEdit* seed = dialog.findChild<QLineEdit*>("seedEdit");
    QSpinBox* threads = dialog.findChild<QSpinBox*>("threadsSpin");

    dvatest::check(modelTable != nullptr,
                   "batch processor dialog exposes model table");
    dvatest::check(outputDir != nullptr,
                   "batch processor dialog exposes output directory edit");
    dvatest::check(totalRuns != nullptr,
                   "batch processor dialog exposes total runs spin");
    dvatest::check(seed != nullptr, "batch processor dialog exposes seed edit");
    dvatest::check(threads != nullptr,
                   "batch processor dialog exposes thread spin");

    const SimulationSettings normalized = normalizeSimulationSettings(settings, 64);
    dvatest::check(modelTable != nullptr && modelTable->rowCount() == 0,
                   "batch processor dialog starts with empty model table");
    dvatest::check(outputDir != nullptr && outputDir->text().isEmpty(),
                   "batch processor dialog starts with empty output directory");
    dvatest::check(totalRuns != nullptr &&
                       totalRuns->value() == normalized.totalRuns,
                   "batch processor dialog normalizes total runs");
    dvatest::check(seed != nullptr &&
                       seed->text().toULongLong() == normalized.initialSeed,
                   "batch processor dialog normalizes seed");
    dvatest::check(threads != nullptr && threads->value() == normalized.threads,
                   "batch processor dialog clamps thread default");
}

TEST("batch processor dialog exposes stable dialog object name") {
    ui::BatchProcessorDialog dialog(SimulationSettings{});

    dvatest::check(dialog.objectName() == "batchProcessorDialog",
                   "batch processor dialog exposes stable object name");
}

TEST("batch processor dialog exposes stable model table header") {
    ui::BatchProcessorDialog dialog(SimulationSettings{});

    QHeaderView* header = dialog.findChild<QHeaderView*>("modelTableHeader");
    dvatest::check(header != nullptr,
                   "batch processor dialog exposes model table header");
    dvatest::check(header != nullptr && header->count() == 4,
                   "batch processor model table header exposes job columns");
}

TEST("batch processor dialog exposes stable button box") {
    ui::BatchProcessorDialog dialog(SimulationSettings{});

    QDialogButtonBox* buttons =
        dialog.findChild<QDialogButtonBox*>("batchProcessorButtonBox");
    dvatest::check(buttons != nullptr,
                   "batch processor dialog exposes button box");
    dvatest::check(buttons != nullptr &&
                       buttons->button(QDialogButtonBox::Ok) != nullptr,
                   "batch processor dialog exposes Run button");
    dvatest::check(buttons != nullptr &&
                       buttons->button(QDialogButtonBox::Ok)->text() == "Run",
                   "batch processor dialog labels OK button as Run");
    dvatest::check(buttons != nullptr &&
                       buttons->button(QDialogButtonBox::Cancel) != nullptr,
                   "batch processor dialog exposes Cancel button");
}

TEST("batch processor dialog exposes stable run controls") {
    ui::BatchProcessorDialog dialog(SimulationSettings{});

    const std::vector<std::pair<const char*, QString>> expected{
        {"batchProcessorRunButton", "Run"},
        {"batchProcessorCancelButton", "Cancel"}};

    for (const auto& entry : expected) {
        QAbstractButton* button =
            dialog.findChild<QAbstractButton*>(entry.first);
        dvatest::check(button != nullptr,
                       std::string("batch processor dialog exposes ") +
                           entry.first);
        dvatest::check(button != nullptr && button->text() == entry.second,
                       std::string("batch processor dialog labels ") +
                           entry.first);
    }
}

TEST("batch processor dialog exposes stable command buttons") {
    ui::BatchProcessorDialog dialog(SimulationSettings{});

    const std::vector<std::pair<const char*, QString>> expected{
        {"addModelsButton", "Add Models..."},
        {"removeModelsButton", "Remove"},
        {"browseOutputDirectoryButton", "Browse..."}};

    for (const auto& entry : expected) {
        QPushButton* button = dialog.findChild<QPushButton*>(entry.first);
        dvatest::check(button != nullptr,
                       std::string("batch processor dialog exposes ") +
                           entry.first);
        dvatest::check(button != nullptr && button->text() == entry.second,
                       std::string("batch processor dialog labels ") +
                           entry.first);
    }
}

TEST("simulation results dialog exposes populated tables and actions") {
    MeasureStats stats;
    stats.nominal = 10.0;
    stats.mean = 10.25;
    stats.sigma = 0.5;
    stats.sixSigma = 3.0;
    stats.minVal = 9.5;
    stats.maxVal = 11.0;
    stats.cp = 1.2;
    stats.cpk = 1.1;
    stats.totOutPct = 2.5;
    stats.dpmo = 25000.0;
    stats.histogram = {2, 3};

    SimulationSampleRow sample;
    sample.buildIndex = 7;
    sample.measureValues[401] = 10.125;

    ContributorRow contributor;
    contributor.measure = 401;
    contributor.contributor = 501;
    contributor.geoFactor = 0.75;
    contributor.sixSigma = 1.5;
    contributor.contributionPct = 62.5;

    ui::SimulationResultsDialog dialog(
        "Result Model", {{401, stats}}, {{401, "Gap"}},
        {sample}, {contributor}, {{501, "Slot Tol"}}, "reports/result.html");

    QTabWidget* tabs = dialog.findChild<QTabWidget*>("resultsTabs");
    QTableWidget* summary = dialog.findChild<QTableWidget*>("summaryTable");
    QTableWidget* histogram = dialog.findChild<QTableWidget*>("histogramTable");
    QTableWidget* samples = dialog.findChild<QTableWidget*>("samplesTable");
    QTableWidget* contributors =
        dialog.findChild<QTableWidget*>("contributorTable");

    dvatest::check(tabs != nullptr, "results dialog exposes tabs");
    dvatest::check(summary != nullptr, "results dialog exposes summary table");
    dvatest::check(histogram != nullptr,
                   "results dialog exposes histogram table");
    dvatest::check(samples != nullptr, "results dialog exposes samples table");
    dvatest::check(contributors != nullptr,
                   "results dialog exposes contributor table");

    dvatest::check(summary != nullptr && summary->rowCount() == 1 &&
                       summary->item(0, 0)->text() == "Gap",
                   "results dialog populates summary measure name");
    dvatest::check(histogram != nullptr && histogram->rowCount() == 2,
                   "results dialog populates histogram rows");
    dvatest::check(samples != nullptr && samples->rowCount() == 1 &&
                       samples->item(0, 0)->text() == "7",
                   "results dialog populates sample build");
    dvatest::check(contributors != nullptr && contributors->rowCount() == 1 &&
                       contributors->item(0, 1)->text() == "Slot Tol",
                   "results dialog populates contributor name");

    const char* buttons[] = {"exportHtmlButton", "exportCsvButton",
                             "exportExcelXmlButton", "exportHstButton",
                             "exportHlmButton", "openHstButton",
                             "openHlmButton"};
    for (const char* name : buttons) {
        dvatest::check(dialog.findChild<QAbstractButton*>(name) != nullptr,
                       std::string("results dialog exposes ") + name);
    }
}

TEST("simulation results dialog exposes stable dialog object name") {
    ui::SimulationResultsDialog dialog(
        "Result Model", {}, {}, {}, {}, {}, "reports/result.html");

    dvatest::check(dialog.objectName() == "simulationResultsDialog",
                   "results dialog exposes stable object name");
}

TEST("simulation results dialog exposes stable tab bar") {
    ui::SimulationResultsDialog dialog(
        "Result Model", {}, {}, {}, {}, {}, "reports/result.html");

    QTabBar* tabBar = dialog.findChild<QTabBar*>("resultsTabBar");
    dvatest::check(tabBar != nullptr, "results dialog exposes tab bar");
    dvatest::check(tabBar != nullptr && tabBar->count() == 4,
                   "results dialog tab bar exposes result pages");
    dvatest::check(tabBar != nullptr && tabBar->tabText(0) == "Summary",
                   "results dialog labels summary tab");
    dvatest::check(tabBar != nullptr && tabBar->tabText(1) == "Histogram",
                   "results dialog labels histogram tab");
}

TEST("simulation results dialog exposes stable summary table header") {
    ui::SimulationResultsDialog dialog(
        "Result Model", {}, {}, {}, {}, {}, "reports/result.html");

    QHeaderView* header =
        dialog.findChild<QHeaderView*>("summaryTableHeader");
    dvatest::check(header != nullptr,
                   "results dialog exposes summary table header");
    dvatest::check(header != nullptr && header->count() == 11,
                   "results dialog summary header exposes statistic columns");
}

TEST("simulation results dialog exposes stable histogram table header") {
    ui::SimulationResultsDialog dialog(
        "Result Model", {}, {}, {}, {}, {}, "reports/result.html");

    QHeaderView* header =
        dialog.findChild<QHeaderView*>("histogramTableHeader");
    dvatest::check(header != nullptr,
                   "results dialog exposes histogram table header");
    dvatest::check(header != nullptr && header->count() == 5,
                   "results dialog histogram header exposes bin columns");
}

TEST("simulation results dialog exposes stable samples table header") {
    SimulationSampleRow sample;
    sample.buildIndex = 7;
    sample.measureValues[401] = 10.125;
    MeasureStats stats;

    ui::SimulationResultsDialog dialog(
        "Result Model", {{401, stats}}, {{401, "Gap"}}, {sample}, {}, {},
        "reports/result.html");

    QHeaderView* header = dialog.findChild<QHeaderView*>("samplesTableHeader");
    dvatest::check(header != nullptr,
                   "results dialog exposes samples table header");
    dvatest::check(header != nullptr && header->count() == 2,
                   "results dialog samples header exposes build and measure");
}

TEST("simulation results dialog exposes stable contributor table header") {
    ui::SimulationResultsDialog dialog(
        "Result Model", {}, {}, {}, {}, {}, "reports/result.html");

    QHeaderView* header =
        dialog.findChild<QHeaderView*>("contributorTableHeader");
    dvatest::check(header != nullptr,
                   "results dialog exposes contributor table header");
    dvatest::check(header != nullptr && header->count() == 5,
                   "results dialog contributor header exposes contributor columns");
}

TEST("simulation results dialog exposes stable button box") {
    ui::SimulationResultsDialog dialog(
        "Result Model", {}, {}, {}, {}, {}, "reports/result.html");

    QDialogButtonBox* buttons =
        dialog.findChild<QDialogButtonBox*>("simulationResultsButtonBox");
    dvatest::check(buttons != nullptr,
                   "results dialog exposes button box");
    dvatest::check(buttons != nullptr &&
                       buttons->button(QDialogButtonBox::Close) != nullptr,
                   "results dialog exposes Close button");
}

TEST("simulation results dialog exposes stable histogram bars") {
    MeasureStats stats;
    stats.histogram = {2, 3};

    ui::SimulationResultsDialog dialog(
        "Result Model", {{401, stats}}, {{401, "Gap"}}, {}, {}, {},
        "reports/result.html");

    QProgressBar* firstBar =
        dialog.findChild<QProgressBar*>("histogramBar0");
    QProgressBar* secondBar =
        dialog.findChild<QProgressBar*>("histogramBar1");
    dvatest::check(firstBar != nullptr,
                   "results dialog exposes first histogram bar");
    dvatest::check(secondBar != nullptr,
                   "results dialog exposes second histogram bar");
    dvatest::check(firstBar != nullptr && firstBar->value() == 2,
                   "first histogram bar shows first count");
    dvatest::check(secondBar != nullptr && secondBar->value() == 3,
                   "second histogram bar shows second count");
}

TEST("simulation results dialog exposes stable close button") {
    ui::SimulationResultsDialog dialog(
        "Result Model", {}, {}, {}, {}, {}, "reports/result.html");

    QAbstractButton* close =
        dialog.findChild<QAbstractButton*>("simulationResultsCloseButton");
    dvatest::check(close != nullptr,
                   "results dialog exposes stable close button");
    dvatest::check(close != nullptr && close->text() == "Close",
                   "results dialog labels stable close button");
}

TEST("main window apply preferences refreshes recent files menu") {
    AppPreferences preferences;
    preferences.recentModelPaths = {"applied.xml"};

    ui::MainWindow window;
    window.applyPreferencesForTesting(preferences);

    const QStringList actions = window.recentFileActionTextsForTesting();
    dvatest::check(actions.size() == 1,
                   "applied preferences refresh recent file count");
    dvatest::check(actions[0] == "applied.xml",
                   "applied preferences refresh recent file label");
    dvatest::check(window.statusBar()->currentMessage() == "Preferences updated",
                   "applied preferences reports status");
}

TEST("main window apply preferences persists to configured path") {
    const std::string path = "mainwindow_preferences_test.ini";
    std::remove(path.c_str());

    AppPreferences preferences;
    preferences.lengthUnit = LengthUnit::Inch;
    preferences.analysisDefaults.totalRuns = 6789;
    preferences.analysisDefaults.initialSeed = 24680;
    preferences.analysisDefaults.threads = 2;
    preferences.defaultReportPath = "reports/persisted.html";
    preferences.recentModelPaths = {"persisted.xml"};

    ui::MainWindow window;
    window.setPreferencesPathForTesting(QString::fromStdString(path));
    window.applyPreferencesForTesting(preferences);

    const AppPreferences loaded = loadAppPreferences(path);
    dvatest::check(loaded.lengthUnit == LengthUnit::Inch,
                   "applied preferences persist unit");
    dvatest::check(loaded.analysisDefaults.totalRuns == 6789,
                   "applied preferences persist run count");
    dvatest::check(loaded.analysisDefaults.initialSeed == 24680,
                   "applied preferences persist seed");
    dvatest::check(loaded.analysisDefaults.threads == 2,
                   "applied preferences persist threads");
    dvatest::check(loaded.defaultReportPath == "reports/persisted.html",
                   "applied preferences persist report path");
    dvatest::check(!loaded.recentModelPaths.empty() &&
                       loaded.recentModelPaths[0] == "persisted.xml",
                   "applied preferences persist recent path");

    std::remove(path.c_str());
}

TEST("main window recent files menu reflects preferences") {
    AppPreferences preferences;
    preferences.recentModelPaths = {"first.xml", "second.xml"};

    ui::MainWindow window;
    window.setPreferencesForTesting(preferences);

    const QStringList actions = window.recentFileActionTextsForTesting();
    dvatest::check(actions.size() == 2,
                   "recent files menu shows persisted paths");
    dvatest::check(actions[0] == "first.xml",
                   "newest recent file appears first");
    dvatest::check(actions[1] == "second.xml",
                   "older recent file appears second");
}

TEST("main window empty recent files menu disables placeholder") {
    AppPreferences preferences;

    ui::MainWindow window;
    window.setPreferencesForTesting(preferences);

    const QStringList actions = window.recentFileActionTextsForTesting();
    dvatest::check(actions.size() == 1,
                   "empty recent files menu shows one placeholder");
    dvatest::check(actions[0] == "(No recent files)",
                   "empty recent files menu labels placeholder");
    dvatest::check(!window.recentFileActionEnabledForTesting(0),
                   "empty recent files placeholder is disabled");
}

TEST("main window open recent actions expose stable names") {
    AppPreferences preferences;
    preferences.recentModelPaths = {"first_recent_model.xml",
                                    "second_recent_model.xml"};

    ui::MainWindow window;
    QMenu* recentMenu = window.findChild<QMenu*>("recentFilesMenu");
    dvatest::check(recentMenu != nullptr,
                   "main window exposes recent files menu for action names");

    window.setPreferencesForTesting(AppPreferences{});
    const QList<QAction*> emptyActions =
        recentMenu != nullptr ? recentMenu->actions() : QList<QAction*>{};
    QAction* emptyAction = emptyActions.empty() ? nullptr : emptyActions[0];
    dvatest::check(emptyAction != nullptr &&
                       emptyAction->objectName() == "emptyRecentFilesAction",
                   "empty recent placeholder exposes stable action name");

    window.setPreferencesForTesting(preferences);
    QAction* first = recentMenu != nullptr
                         ? recentMenu->findChild<QAction*>("recentFileAction0")
                         : nullptr;
    QAction* second = recentMenu != nullptr
                          ? recentMenu->findChild<QAction*>("recentFileAction1")
                          : nullptr;

    dvatest::check(first != nullptr,
                   "first recent file action exposes stable name");
    dvatest::check(second != nullptr,
                   "second recent file action exposes stable name");
    dvatest::check(first != nullptr &&
                       first->data().toString() == "first_recent_model.xml",
                   "first recent file action stores first path");
    dvatest::check(second != nullptr &&
                       second->data().toString() == "second_recent_model.xml",
                   "second recent file action stores second path");
}

TEST("main window open recent action loads selected model") {
    Model recentModel = createStarterModel();
    recentModel.assemblyName = "Recent Assembly";
    const std::string path = "mainwindow_recent_model_test.xml";
    dvatest::check(saveModel(recentModel, path), "recent test model saved");

    AppPreferences preferences;
    preferences.recentModelPaths = {path};

    ui::MainWindow window;
    window.setPreferencesForTesting(preferences);
    dvatest::check(window.triggerRecentFileForTesting(0),
                   "main window triggers recent file action");

    const Model& loaded = window.modelForTesting();
    dvatest::check(loaded.assemblyName == "Recent Assembly",
                   "recent file action loads selected model");
    dvatest::check(window.windowTitle().contains("mainwindow_recent_model_test.xml"),
                   "recent file path appears in window title");
    dvatest::check(!window.windowTitle().contains("*"),
                   "recent file load is not marked dirty");

    std::remove(path.c_str());
}

TEST("main window open model path loads file and records recent path") {
    Model saved = createStarterModel();
    saved.assemblyName = "Opened Assembly";
    const std::string path = "mainwindow_open_model_test.xml";
    dvatest::check(saveModel(saved, path), "open test model saved");

    ui::MainWindow window;
    dvatest::check(window.openModelFromPathForTesting(QString::fromStdString(path)),
                   "main window opens model from path");

    const Model& loaded = window.modelForTesting();
    dvatest::check(loaded.assemblyName == "Opened Assembly",
                   "opened model replaces current model");
    dvatest::check(window.windowTitle().contains("mainwindow_open_model_test.xml"),
                   "opened model path appears in window title");
    dvatest::check(!window.windowTitle().contains("*"),
                   "opened model is not marked dirty");
    const QStringList recent = window.recentFileActionTextsForTesting();
    dvatest::check(!recent.empty() && recent[0] == "mainwindow_open_model_test.xml",
                   "opened model is recorded as most recent path");

    std::remove(path.c_str());
}

TEST("main window save model path writes file and records recent path") {
    Model model = createStarterModel();
    model.assemblyName = "Saved Assembly";
    const std::string path = "mainwindow_save_model_test.xml";
    std::remove(path.c_str());

    ui::MainWindow window;
    window.setModelForTesting(model);
    dvatest::check(window.saveModelToPathForTesting(QString::fromStdString(path)),
                   "main window saves model to path");

    Model loaded;
    dvatest::check(loadModel(loaded, path), "saved model loads from disk");
    dvatest::check(loaded.assemblyName == "Saved Assembly",
                   "saved model preserves assembly name");
    dvatest::check(window.windowTitle().contains("mainwindow_save_model_test.xml"),
                   "saved model path appears in window title");
    dvatest::check(!window.windowTitle().contains("*"),
                   "saved model is not marked dirty");
    const QStringList recent = window.recentFileActionTextsForTesting();
    dvatest::check(!recent.empty() && recent[0] == "mainwindow_save_model_test.xml",
                   "saved model is recorded as most recent path");

    std::remove(path.c_str());
}

TEST("main window new model resets model path and dirty state") {
    Model opened = createStarterModel();
    opened.assemblyName = "Opened Before New";
    const std::string path = "mainwindow_new_model_source.xml";
    dvatest::check(saveModel(opened, path), "new model source saved");

    ui::MainWindow window;
    dvatest::check(window.openModelFromPathForTesting(QString::fromStdString(path)),
                   "main window opens source model");
    dvatest::check(window.windowTitle().contains("mainwindow_new_model_source.xml"),
                   "source path appears before new model");

    window.newModelForTesting();

    const Model& model = window.modelForTesting();
    dvatest::check(model.assemblyName == "UntitledModel",
                   "new model resets assembly name");
    dvatest::check(!window.windowTitle().contains("mainwindow_new_model_source.xml"),
                   "new model clears current file path");
    dvatest::check(window.windowTitle().contains("UntitledModel"),
                   "new model title uses starter assembly name");
    dvatest::check(!window.windowTitle().contains("*"),
                   "new model is not marked dirty");
    dvatest::check(window.statusBar()->currentMessage() == "New model created",
                   "new model reports status");

    std::remove(path.c_str());
}

TEST("main window delete action requires navigator selection") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    navigator->setCurrentItem(nullptr);

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Select an item to delete",
                   "delete action reports missing navigator selection");

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "delete selection guard leaves parts unchanged");
    dvatest::check(model.moves.size() == 2,
                   "delete selection guard leaves moves unchanged");
    dvatest::check(model.variants.size() == 1,
                   "delete selection guard leaves variants unchanged");
}

TEST("main window delete action rejects category selection") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* moves = childByText(root, "Moves");
    dvatest::check(moves != nullptr, "moves category exists");
    navigator->setCurrentItem(moves);

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Select a model object to delete",
                   "delete action reports category selection guard");

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "delete category guard leaves parts unchanged");
    dvatest::check(model.moves.size() == 2,
                   "delete category guard leaves moves unchanged");
    dvatest::check(model.variants.size() == 1,
                   "delete category guard leaves variants unchanged");
}

TEST("main window delete action rejects root selection") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Select a model object to delete",
                   "delete action reports root selection guard");

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "delete root guard leaves parts unchanged");
    dvatest::check(model.moves.size() == 2,
                   "delete root guard leaves moves unchanged");
    dvatest::check(model.variants.size() == 1,
                   "delete root guard leaves variants unchanged");
}

TEST("main window delete action removes confirmed move") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.moves.size() == 2,
                   "starter UI model has two moves");
    dvatest::check(!initial.variants.empty(),
                   "starter UI model has a variant");
    dvatest::check(initial.variants.front().moves.size() == 2,
                   "starter variant references both moves");
    const MoveId deletedMove = initial.moves[0].id;
    const MoveId remainingMove = initial.moves[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) yesButton->click();
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    const QString status = window.statusBar()->currentMessage();
    dvatest::check(status == "Deleted move",
                   QString("delete action reports confirmed move deletion "
                           "(was '%1')")
                       .arg(status)
                       .toStdString());

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == 1,
                   "confirmed move deletion removes one move");
    dvatest::check(model.moves.front().id == remainingMove,
                   "confirmed move deletion leaves the second move");
    dvatest::check(model.variants.size() == 1,
                   "confirmed move deletion leaves variant count unchanged");
    dvatest::check(model.variants.front().moves.size() == 1,
                   "confirmed move deletion removes variant move reference");
    dvatest::check(model.variants.front().moves.front() == remainingMove,
                   "confirmed move deletion keeps remaining variant move");
    dvatest::check(model.variants.front().moves.front() != deletedMove,
                   "confirmed move deletion drops deleted variant move");
}

TEST("main window confirmed move deletion refreshes navigator") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.moves.size() == 2,
                   "starter UI model has two moves");
    const QString deletedMoveName =
        QString::fromStdString(initial.moves[0].name);
    const QString remainingMoveName =
        QString::fromStdString(initial.moves[1].name);

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) yesButton->click();
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* moves = childByText(root, "Moves");
    dvatest::check(moves != nullptr, "moves category exists");
    dvatest::check(moves->childCount() == 1,
                   "confirmed move deletion refreshes move node count");
    dvatest::check(childByText(moves, deletedMoveName) == nullptr,
                   "confirmed move deletion removes deleted navigator node");
    QTreeWidgetItem* remaining = childByText(moves, remainingMoveName);
    dvatest::check(remaining != nullptr,
                   "confirmed move deletion keeps remaining navigator node");
}

TEST("main window delete action cancellation keeps move") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.moves.size() == 2,
                   "starter UI model has two moves");
    dvatest::check(!initial.variants.empty(),
                   "starter UI model has a variant");
    dvatest::check(initial.variants.front().moves.size() == 2,
                   "starter variant references both moves");
    const MoveId firstMove = initial.moves[0].id;
    const MoveId secondMove = initial.moves[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* noButton =
            box != nullptr ? box->button(QMessageBox::No) : nullptr;
        dvatest::check(noButton != nullptr,
                       "delete confirmation dialog has a no button");
        if (noButton != nullptr) noButton->click();
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    const Model& model = window.modelForTesting();
    dvatest::check(model.moves.size() == 2,
                   "cancelled move deletion leaves move count unchanged");
    dvatest::check(model.moves[0].id == firstMove,
                   "cancelled move deletion leaves first move in place");
    dvatest::check(model.moves[1].id == secondMove,
                   "cancelled move deletion leaves second move in place");
    dvatest::check(model.variants.size() == 1,
                   "cancelled move deletion leaves variant count unchanged");
    dvatest::check(model.variants.front().moves.size() == 2,
                   "cancelled move deletion leaves variant moves unchanged");
    dvatest::check(model.variants.front().moves[0] == firstMove,
                   "cancelled move deletion keeps first variant move");
    dvatest::check(model.variants.front().moves[1] == secondMove,
                   "cancelled move deletion keeps second variant move");
}

TEST("main window cancelled move deletion keeps navigator") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.moves.size() == 2,
                   "starter UI model has two moves");
    const QString firstMoveName = QString::fromStdString(initial.moves[0].name);
    const QString secondMoveName =
        QString::fromStdString(initial.moves[1].name);

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* noButton =
            box != nullptr ? box->button(QMessageBox::No) : nullptr;
        dvatest::check(noButton != nullptr,
                       "delete confirmation dialog has a no button");
        if (noButton != nullptr) noButton->click();
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* moves = childByText(root, "Moves");
    dvatest::check(moves != nullptr, "moves category exists");
    dvatest::check(moves->childCount() == 2,
                   "cancelled move deletion leaves move node count unchanged");
    dvatest::check(childByText(moves, firstMoveName) != nullptr,
                   "cancelled move deletion keeps first navigator node");
    dvatest::check(childByText(moves, secondMoveName) != nullptr,
                   "cancelled move deletion keeps second navigator node");
}

TEST("main window delete action removes confirmed measure") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMeasureVariant());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.measures.size() == 2,
                   "starter UI model has two measures");
    dvatest::check(!initial.variants.empty(),
                   "starter UI model has a variant");
    dvatest::check(initial.variants.front().measures.size() == 2,
                   "starter variant references both measures");
    const MeasureId deletedMeasure = initial.measures[0].id;
    const MeasureId remainingMeasure = initial.measures[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) yesButton->click();
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    const QString status = window.statusBar()->currentMessage();
    dvatest::check(status == "Deleted measure",
                   QString("delete action reports confirmed measure deletion "
                           "(was '%1')")
                       .arg(status)
                       .toStdString());

    const Model& model = window.modelForTesting();
    dvatest::check(model.measures.size() == 1,
                   "confirmed measure deletion removes one measure");
    dvatest::check(model.measures.front().id == remainingMeasure,
                   "confirmed measure deletion leaves the second measure");
    dvatest::check(model.variants.size() == 1,
                   "confirmed measure deletion leaves variant count unchanged");
    dvatest::check(
        model.variants.front().measures.size() == 1,
        "confirmed measure deletion removes variant measure reference");
    dvatest::check(model.variants.front().measures.front() == remainingMeasure,
                   "confirmed measure deletion keeps remaining variant measure");
    dvatest::check(model.variants.front().measures.front() != deletedMeasure,
                   "confirmed measure deletion drops deleted variant measure");
}

TEST("main window delete action cancellation keeps measure") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMeasureVariant());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.measures.size() == 2,
                   "starter UI model has two measures");
    dvatest::check(!initial.variants.empty(),
                   "starter UI model has a variant");
    dvatest::check(initial.variants.front().measures.size() == 2,
                   "starter variant references both measures");
    const MeasureId firstMeasure = initial.measures[0].id;
    const MeasureId secondMeasure = initial.measures[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* noButton =
            box != nullptr ? box->button(QMessageBox::No) : nullptr;
        dvatest::check(noButton != nullptr,
                       "delete confirmation dialog has a no button");
        if (noButton != nullptr) noButton->click();
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    const Model& model = window.modelForTesting();
    dvatest::check(model.measures.size() == 2,
                   "cancelled measure deletion leaves measure count unchanged");
    dvatest::check(model.measures[0].id == firstMeasure,
                   "cancelled measure deletion leaves first measure in place");
    dvatest::check(model.measures[1].id == secondMeasure,
                   "cancelled measure deletion leaves second measure in place");
    dvatest::check(model.variants.size() == 1,
                   "cancelled measure deletion leaves variant count unchanged");
    dvatest::check(
        model.variants.front().measures.size() == 2,
        "cancelled measure deletion leaves variant measures unchanged");
    dvatest::check(model.variants.front().measures[0] == firstMeasure,
                   "cancelled measure deletion keeps first variant measure");
    dvatest::check(model.variants.front().measures[1] == secondMeasure,
                   "cancelled measure deletion keeps second variant measure");
}

TEST("main window confirmed measure deletion refreshes navigator") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMeasureVariant());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.measures.size() == 2,
                   "starter UI model has two measures");
    const QString deletedMeasureName =
        QString::fromStdString(initial.measures[0].name);
    const QString remainingMeasureName =
        QString::fromStdString(initial.measures[1].name);

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) yesButton->click();
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* measures = childByText(root, "Measures");
    dvatest::check(measures != nullptr, "measures category exists");
    dvatest::check(measures->childCount() == 1,
                   "confirmed measure deletion refreshes measure node count");
    dvatest::check(
        childByText(measures, deletedMeasureName) == nullptr,
        "confirmed measure deletion removes deleted navigator node");
    QTreeWidgetItem* remaining =
        childByText(measures, remainingMeasureName);
    dvatest::check(remaining != nullptr,
                   "confirmed measure deletion keeps remaining navigator node");
}

TEST("main window cancelled measure deletion keeps navigator") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMeasureVariant());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.measures.size() == 2,
                   "starter UI model has two measures");
    const QString firstMeasureName =
        QString::fromStdString(initial.measures[0].name);
    const QString secondMeasureName =
        QString::fromStdString(initial.measures[1].name);

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* noButton =
            box != nullptr ? box->button(QMessageBox::No) : nullptr;
        dvatest::check(noButton != nullptr,
                       "delete confirmation dialog has a no button");
        if (noButton != nullptr) noButton->click();
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* measures = childByText(root, "Measures");
    dvatest::check(measures != nullptr, "measures category exists");
    dvatest::check(
        measures->childCount() == 2,
        "cancelled measure deletion leaves measure node count unchanged");
    dvatest::check(childByText(measures, firstMeasureName) != nullptr,
                   "cancelled measure deletion keeps first navigator node");
    dvatest::check(childByText(measures, secondMeasureName) != nullptr,
                   "cancelled measure deletion keeps second navigator node");
}

TEST("main window delete action removes confirmed tolerance") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoToleranceVariant());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().tolerances.size() == 2,
                   "starter UI model has two tolerances");
    dvatest::check(!initial.variants.empty(),
                   "starter UI model has a variant");
    dvatest::check(initial.variants.front().tolerances.size() == 2,
                   "starter variant references both tolerances");
    const ToleranceId deletedTolerance =
        initial.parts.front().tolerances[0].id;
    const ToleranceId remainingTolerance =
        initial.parts.front().tolerances[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) {
            yesButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    const QString status = window.statusBar()->currentMessage();
    dvatest::check(status == "Deleted tolerance",
                   QString("delete action reports confirmed tolerance deletion "
                           "(was '%1')")
                       .arg(status)
                       .toStdString());

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(model.parts.front().tolerances.size() == 1,
                   "confirmed tolerance deletion removes one tolerance");
    dvatest::check(model.parts.front().tolerances.front().id ==
                       remainingTolerance,
                   "confirmed tolerance deletion leaves the second tolerance");
    dvatest::check(model.variants.size() == 1,
                   "confirmed tolerance deletion leaves variant count unchanged");
    dvatest::check(
        model.variants.front().tolerances.size() == 1,
        "confirmed tolerance deletion removes variant tolerance reference");
    dvatest::check(
        model.variants.front().tolerances.front() == remainingTolerance,
        "confirmed tolerance deletion keeps remaining variant tolerance");
    dvatest::check(
        model.variants.front().tolerances.front() != deletedTolerance,
        "confirmed tolerance deletion drops deleted variant tolerance");
}

TEST("main window delete action cancellation keeps tolerance") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoToleranceVariant());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().tolerances.size() == 2,
                   "starter UI model has two tolerances");
    dvatest::check(!initial.variants.empty(),
                   "starter UI model has a variant");
    dvatest::check(initial.variants.front().tolerances.size() == 2,
                   "starter variant references both tolerances");
    const ToleranceId firstTolerance =
        initial.parts.front().tolerances[0].id;
    const ToleranceId secondTolerance =
        initial.parts.front().tolerances[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* noButton =
            box != nullptr ? box->button(QMessageBox::No) : nullptr;
        dvatest::check(noButton != nullptr,
                       "delete confirmation dialog has a no button");
        if (noButton != nullptr) {
            noButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(model.parts.front().tolerances.size() == 2,
                   "cancelled tolerance deletion leaves tolerance count unchanged");
    dvatest::check(model.parts.front().tolerances[0].id == firstTolerance,
                   "cancelled tolerance deletion leaves first tolerance in place");
    dvatest::check(model.parts.front().tolerances[1].id == secondTolerance,
                   "cancelled tolerance deletion leaves second tolerance in place");
    dvatest::check(model.variants.size() == 1,
                   "cancelled tolerance deletion leaves variant count unchanged");
    dvatest::check(
        model.variants.front().tolerances.size() == 2,
        "cancelled tolerance deletion leaves variant tolerances unchanged");
    dvatest::check(
        model.variants.front().tolerances[0] == firstTolerance,
        "cancelled tolerance deletion keeps first variant tolerance");
    dvatest::check(
        model.variants.front().tolerances[1] == secondTolerance,
        "cancelled tolerance deletion keeps second variant tolerance");
}

TEST("main window confirmed tolerance deletion refreshes navigator") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoToleranceVariant());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().tolerances.size() == 2,
                   "starter UI model has two tolerances");
    const QString deletedToleranceName =
        QString::fromStdString(initial.parts.front().tolerances[0].name);
    const QString remainingToleranceName =
        QString::fromStdString(initial.parts.front().tolerances[1].name);

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) {
            yesButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() > 0,
                   "parts category has a part child");
    QTreeWidgetItem* part =
        parts != nullptr && parts->childCount() > 0 ? parts->child(0)
                                                    : nullptr;
    QTreeWidgetItem* tolerances = childByText(part, "Tolerances");
    dvatest::check(tolerances != nullptr, "tolerances category exists");
    dvatest::check(
        tolerances->childCount() == 1,
        "confirmed tolerance deletion refreshes tolerance node count");
    dvatest::check(
        childByText(tolerances, deletedToleranceName) == nullptr,
        "confirmed tolerance deletion removes deleted navigator node");
    QTreeWidgetItem* remaining =
        childByText(tolerances, remainingToleranceName);
    dvatest::check(
        remaining != nullptr,
        "confirmed tolerance deletion keeps remaining navigator node");
}

TEST("main window delete action removes confirmed gdt") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().gdts.size() == 2,
                   "starter UI model has two GD&T callouts");
    const GdtId deletedGdt = initial.parts.front().gdts[0].id;
    const GdtId remainingGdt = initial.parts.front().gdts[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) {
            yesButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    const QString status = window.statusBar()->currentMessage();
    dvatest::check(status == "Deleted GD&T",
                   QString("delete action reports confirmed GD&T deletion "
                           "(was '%1')")
                       .arg(status)
                       .toStdString());

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(model.parts.front().gdts.size() == 1,
                   "confirmed GD&T deletion removes one callout");
    dvatest::check(model.parts.front().gdts.front().id == remainingGdt,
                   "confirmed GD&T deletion leaves the second callout");
    dvatest::check(model.parts.front().gdts.front().id != deletedGdt,
                   "confirmed GD&T deletion drops deleted callout");
}

TEST("main window delete action cancellation keeps gdt") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().gdts.size() == 2,
                   "starter UI model has two GD&T callouts");
    const GdtId firstGdt = initial.parts.front().gdts[0].id;
    const GdtId secondGdt = initial.parts.front().gdts[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* noButton =
            box != nullptr ? box->button(QMessageBox::No) : nullptr;
        dvatest::check(noButton != nullptr,
                       "delete confirmation dialog has a no button");
        if (noButton != nullptr) {
            noButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(model.parts.front().gdts.size() == 2,
                   "cancelled GD&T deletion leaves callout count unchanged");
    dvatest::check(model.parts.front().gdts[0].id == firstGdt,
                   "cancelled GD&T deletion leaves first callout in place");
    dvatest::check(model.parts.front().gdts[1].id == secondGdt,
                   "cancelled GD&T deletion leaves second callout in place");
}

TEST("main window confirmed gdt deletion refreshes navigator") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().gdts.size() == 2,
                   "starter UI model has two GD&T callouts");
    const QString deletedGdtName =
        QString::fromStdString(initial.parts.front().gdts[0].name);
    const QString remainingGdtName =
        QString::fromStdString(initial.parts.front().gdts[1].name);

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) {
            yesButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() > 0,
                   "parts category has a part child");
    QTreeWidgetItem* part =
        parts != nullptr && parts->childCount() > 0 ? parts->child(0)
                                                    : nullptr;
    QTreeWidgetItem* gdts = childByText(part, "GD&T");
    dvatest::check(gdts != nullptr, "GD&T category exists");
    dvatest::check(gdts->childCount() == 1,
                   "confirmed GD&T deletion refreshes GD&T node count");
    dvatest::check(childByText(gdts, deletedGdtName) == nullptr,
                   "confirmed GD&T deletion removes deleted navigator node");
    QTreeWidgetItem* remaining = childByText(gdts, remainingGdtName);
    dvatest::check(remaining != nullptr,
                   "confirmed GD&T deletion keeps remaining navigator node");
}

TEST("main window delete action removes confirmed feature") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoFeatures());
    dvatest::check(window.selectFirstFeatureForTesting(),
                   "starter UI model has a feature item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().features.size() == 2,
                   "starter UI model has two features");
    const FeatureId deletedFeature = initial.parts.front().features[0].id;
    const FeatureId remainingFeature = initial.parts.front().features[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) {
            yesButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    const QString status = window.statusBar()->currentMessage();
    dvatest::check(status == "Deleted feature",
                   QString("delete action reports confirmed feature deletion "
                           "(was '%1')")
                       .arg(status)
                       .toStdString());

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(model.parts.front().features.size() == 1,
                   "confirmed feature deletion removes one feature");
    dvatest::check(model.parts.front().features.front().id ==
                       remainingFeature,
                   "confirmed feature deletion leaves the second feature");
    dvatest::check(model.parts.front().features.front().id != deletedFeature,
                   "confirmed feature deletion drops deleted feature");
}

TEST("main window delete action cancellation keeps feature") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoFeatures());
    dvatest::check(window.selectFirstFeatureForTesting(),
                   "starter UI model has a feature item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().features.size() == 2,
                   "starter UI model has two features");
    const FeatureId firstFeature = initial.parts.front().features[0].id;
    const FeatureId secondFeature = initial.parts.front().features[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* noButton =
            box != nullptr ? box->button(QMessageBox::No) : nullptr;
        dvatest::check(noButton != nullptr,
                       "delete confirmation dialog has a no button");
        if (noButton != nullptr) {
            noButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(model.parts.front().features.size() == 2,
                   "cancelled feature deletion leaves feature count unchanged");
    dvatest::check(model.parts.front().features[0].id == firstFeature,
                   "cancelled feature deletion leaves first feature in place");
    dvatest::check(model.parts.front().features[1].id == secondFeature,
                   "cancelled feature deletion leaves second feature in place");
}

TEST("main window confirmed feature deletion refreshes navigator") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoFeatures());
    dvatest::check(window.selectFirstFeatureForTesting(),
                   "starter UI model has a feature item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().features.size() == 2,
                   "starter UI model has two features");
    const QString deletedFeatureName =
        QString("Feature %1").arg(initial.parts.front().features[0].id);
    const QString remainingFeatureName =
        QString("Feature %1").arg(initial.parts.front().features[1].id);

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) {
            yesButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() > 0,
                   "parts category has a part child");
    QTreeWidgetItem* part =
        parts != nullptr && parts->childCount() > 0 ? parts->child(0)
                                                    : nullptr;
    QTreeWidgetItem* features = childByText(part, "Features");
    dvatest::check(features != nullptr, "features category exists");
    dvatest::check(features->childCount() == 1,
                   "confirmed feature deletion refreshes feature node count");
    dvatest::check(childByText(features, deletedFeatureName) == nullptr,
                   "confirmed feature deletion removes deleted navigator node");
    QTreeWidgetItem* remaining = childByText(features, remainingFeatureName);
    dvatest::check(remaining != nullptr,
                   "confirmed feature deletion keeps remaining navigator node");
}

TEST("main window delete action removes confirmed point") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoFeatures());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().points.size() == 2,
                   "starter UI model has two points");
    dvatest::check(initial.parts.front().features.size() == 2,
                   "starter UI model has two point-based features");
    const PointId deletedPoint = initial.parts.front().points[0].id;
    const PointId remainingPoint = initial.parts.front().points[1].id;
    const FeatureId deletedFeature = initial.parts.front().features[0].id;
    const FeatureId remainingFeature = initial.parts.front().features[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) {
            yesButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    const QString status = window.statusBar()->currentMessage();
    dvatest::check(status == "Deleted point",
                   QString("delete action reports confirmed point deletion "
                           "(was '%1')")
                       .arg(status)
                       .toStdString());

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(model.parts.front().points.size() == 1,
                   "confirmed point deletion removes one point");
    dvatest::check(model.parts.front().points.front().id == remainingPoint,
                   "confirmed point deletion leaves the second point");
    dvatest::check(model.parts.front().points.front().id != deletedPoint,
                   "confirmed point deletion drops deleted point");
    dvatest::check(model.parts.front().features.size() == 1,
                   "confirmed point deletion removes dependent feature");
    dvatest::check(model.parts.front().features.front().id ==
                       remainingFeature,
                   "confirmed point deletion leaves independent feature");
    dvatest::check(model.parts.front().features.front().id != deletedFeature,
                   "confirmed point deletion drops deleted point feature");
}

TEST("main window delete action cancellation keeps point") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoFeatures());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().points.size() == 2,
                   "starter UI model has two points");
    dvatest::check(initial.parts.front().features.size() == 2,
                   "starter UI model has two point-based features");
    const PointId firstPoint = initial.parts.front().points[0].id;
    const PointId secondPoint = initial.parts.front().points[1].id;
    const FeatureId firstFeature = initial.parts.front().features[0].id;
    const FeatureId secondFeature = initial.parts.front().features[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* noButton =
            box != nullptr ? box->button(QMessageBox::No) : nullptr;
        dvatest::check(noButton != nullptr,
                       "delete confirmation dialog has a no button");
        if (noButton != nullptr) {
            noButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(model.parts.front().points.size() == 2,
                   "cancelled point deletion leaves point count unchanged");
    dvatest::check(model.parts.front().points[0].id == firstPoint,
                   "cancelled point deletion leaves first point in place");
    dvatest::check(model.parts.front().points[1].id == secondPoint,
                   "cancelled point deletion leaves second point in place");
    dvatest::check(model.parts.front().features.size() == 2,
                   "cancelled point deletion leaves feature count unchanged");
    dvatest::check(model.parts.front().features[0].id == firstFeature,
                   "cancelled point deletion leaves first feature in place");
    dvatest::check(model.parts.front().features[1].id == secondFeature,
                   "cancelled point deletion leaves second feature in place");
}

TEST("main window confirmed point deletion refreshes navigator") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoFeatures());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().points.size() == 2,
                   "starter UI model has two points");
    dvatest::check(initial.parts.front().features.size() == 2,
                   "starter UI model has two point-based features");
    const QString deletedPointName =
        QString("Point %1").arg(initial.parts.front().points[0].id);
    const QString remainingPointName =
        QString("Point %1").arg(initial.parts.front().points[1].id);
    const QString deletedFeatureName =
        QString("Feature %1").arg(initial.parts.front().features[0].id);
    const QString remainingFeatureName =
        QString("Feature %1").arg(initial.parts.front().features[1].id);

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) {
            yesButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts != nullptr && parts->childCount() > 0,
                   "parts category has a part child");
    QTreeWidgetItem* part =
        parts != nullptr && parts->childCount() > 0 ? parts->child(0)
                                                    : nullptr;

    QTreeWidgetItem* points = childByText(part, "Points");
    dvatest::check(points != nullptr, "points category exists");
    dvatest::check(points->childCount() == 1,
                   "confirmed point deletion refreshes point node count");
    dvatest::check(childByText(points, deletedPointName) == nullptr,
                   "confirmed point deletion removes deleted point node");
    dvatest::check(childByText(points, remainingPointName) != nullptr,
                   "confirmed point deletion keeps remaining point node");

    QTreeWidgetItem* features = childByText(part, "Features");
    dvatest::check(features != nullptr, "features category exists");
    dvatest::check(
        features->childCount() == 1,
        "confirmed point deletion refreshes dependent feature node count");
    dvatest::check(
        childByText(features, deletedFeatureName) == nullptr,
        "confirmed point deletion removes deleted dependent feature node");
    dvatest::check(
        childByText(features, remainingFeatureName) != nullptr,
        "confirmed point deletion keeps remaining feature node");
}

TEST("main window delete action removes confirmed part") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts());
    dvatest::check(window.selectFirstPartForTesting(),
                   "starter UI model has a part item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "starter UI model has two parts");
    const PartId deletedPart = initial.parts[0].id;
    const PartId remainingPart = initial.parts[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) {
            yesButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    const QString status = window.statusBar()->currentMessage();
    dvatest::check(status == "Deleted part",
                   QString("delete action reports confirmed part deletion "
                           "(was '%1')")
                       .arg(status)
                       .toStdString());

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 1,
                   "confirmed part deletion removes one part");
    dvatest::check(model.parts.front().id == remainingPart,
                   "confirmed part deletion leaves the second part");
    dvatest::check(model.parts.front().id != deletedPart,
                   "confirmed part deletion drops deleted part");
}

TEST("main window delete action cancellation keeps part") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts());
    dvatest::check(window.selectFirstPartForTesting(),
                   "starter UI model has a part item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "starter UI model has two parts");
    const PartId firstPart = initial.parts[0].id;
    const PartId secondPart = initial.parts[1].id;

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* noButton =
            box != nullptr ? box->button(QMessageBox::No) : nullptr;
        dvatest::check(noButton != nullptr,
                       "delete confirmation dialog has a no button");
        if (noButton != nullptr) {
            noButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    const Model& model = window.modelForTesting();
    dvatest::check(model.parts.size() == 2,
                   "cancelled part deletion leaves part count unchanged");
    dvatest::check(model.parts[0].id == firstPart,
                   "cancelled part deletion leaves first part in place");
    dvatest::check(model.parts[1].id == secondPart,
                   "cancelled part deletion leaves second part in place");
}

TEST("main window confirmed part deletion refreshes navigator") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoParts());
    dvatest::check(window.selectFirstPartForTesting(),
                   "starter UI model has a part item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.parts.size() == 2,
                   "starter UI model has two parts");
    const QString deletedPartName =
        QString::fromStdString(initial.parts[0].dcsName);
    const QString remainingPartName =
        QString::fromStdString(initial.parts[1].dcsName);

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "delete confirmation dialog appears");
        QAbstractButton* yesButton =
            box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
        dvatest::check(yesButton != nullptr,
                       "delete confirmation dialog has a yes button");
        if (yesButton != nullptr) {
            yesButton->click();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* parts = childByText(root, "Parts");
    dvatest::check(parts != nullptr, "parts category exists");
    dvatest::check(parts->childCount() == 1,
                   "confirmed part deletion refreshes part node count");
    dvatest::check(childByText(parts, deletedPartName) == nullptr,
                   "confirmed part deletion removes deleted navigator node");
    QTreeWidgetItem* remaining = childByText(parts, remainingPartName);
    dvatest::check(remaining != nullptr,
                   "confirmed part deletion keeps remaining navigator node");
}

TEST("main window delete action removes confirmed variant") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.variants.size() == 1,
                   "starter UI model has one variant");
    const std::string variantName = initial.variants.front().name;

    QTimer::singleShot(0, []() {
        QWidget* modal = QApplication::activeModalWidget();
        dvatest::check(modal != nullptr, "variant selection dialog appears");
        QTimer::singleShot(0, []() {
            QMessageBox* box =
                qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            dvatest::check(box != nullptr,
                           "variant delete confirmation dialog appears");
            QAbstractButton* yesButton =
                box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
            dvatest::check(yesButton != nullptr,
                           "variant delete confirmation dialog has a yes button");
            if (yesButton != nullptr) {
                yesButton->click();
            }
        });
        QDialog* dialog = qobject_cast<QDialog*>(modal);
        dvatest::check(dialog != nullptr,
                       "variant selection dialog is acceptable");
        if (dialog != nullptr) {
            dialog->accept();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");
    const QString status = window.statusBar()->currentMessage();
    dvatest::check(status == "Model variant deleted: Baseline",
                   QString("delete action reports confirmed variant deletion "
                           "(was '%1')")
                       .arg(status)
                       .toStdString());

    const Model& model = window.modelForTesting();
    dvatest::check(model.variants.empty(),
                   "confirmed variant deletion removes the variant");
    dvatest::check(variantName == "Baseline",
                   "confirmed variant deletion targeted baseline variant");
}

TEST("main window delete action cancellation keeps variant") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.variants.size() == 1,
                   "starter UI model has one variant");
    const std::string variantName = initial.variants.front().name;

    QTimer::singleShot(0, []() {
        QWidget* modal = QApplication::activeModalWidget();
        dvatest::check(modal != nullptr, "variant selection dialog appears");
        QTimer::singleShot(0, []() {
            QMessageBox* box =
                qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            dvatest::check(box != nullptr,
                           "variant delete confirmation dialog appears");
            QAbstractButton* noButton =
                box != nullptr ? box->button(QMessageBox::No) : nullptr;
            dvatest::check(noButton != nullptr,
                           "variant delete confirmation dialog has a no button");
            if (noButton != nullptr) {
                noButton->click();
            }
        });
        QDialog* dialog = qobject_cast<QDialog*>(modal);
        dvatest::check(dialog != nullptr,
                       "variant selection dialog is acceptable");
        if (dialog != nullptr) {
            dialog->accept();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    const Model& model = window.modelForTesting();
    dvatest::check(model.variants.size() == 1,
                   "cancelled variant deletion leaves variant count unchanged");
    dvatest::check(model.variants.front().name == variantName,
                   "cancelled variant deletion keeps the selected variant");
}

TEST("main window confirmed variant deletion refreshes navigator") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.variants.size() == 1,
                   "starter UI model has one variant");
    const QString variantName =
        QString::fromStdString(initial.variants.front().name);

    QTimer::singleShot(0, []() {
        QWidget* modal = QApplication::activeModalWidget();
        dvatest::check(modal != nullptr, "variant selection dialog appears");
        QTimer::singleShot(0, []() {
            QMessageBox* box =
                qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            dvatest::check(box != nullptr,
                           "variant delete confirmation dialog appears");
            QAbstractButton* yesButton =
                box != nullptr ? box->button(QMessageBox::Yes) : nullptr;
            dvatest::check(yesButton != nullptr,
                           "variant delete confirmation dialog has a yes button");
            if (yesButton != nullptr) {
                yesButton->click();
            }
        });
        QDialog* dialog = qobject_cast<QDialog*>(modal);
        dvatest::check(dialog != nullptr,
                       "variant selection dialog is acceptable");
        if (dialog != nullptr) {
            dialog->accept();
        }
    });

    const bool invoked =
        QMetaObject::invokeMethod(&window, "deleteSelectedNavigatorItem");
    dvatest::check(invoked, "delete-selected slot is invokable");

    QTreeWidget* navigator = window.findChild<QTreeWidget*>();
    dvatest::check(navigator != nullptr, "navigator exists");
    QTreeWidgetItem* root = navigator->topLevelItem(0);
    dvatest::check(root != nullptr, "navigator has root");
    QTreeWidgetItem* variants = childByText(root, "Variants");
    dvatest::check(variants != nullptr, "variants category exists");
    dvatest::check(variants->childCount() == 0,
                   "confirmed variant deletion refreshes variant node count");
    dvatest::check(childByText(variants, variantName) == nullptr,
                   "confirmed variant deletion removes deleted navigator node");
}

TEST("main window apply variant action handles empty variant list") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.variants.empty(),
                   "starter UI model has no variants");
    dvatest::check(!initial.parts.empty(),
                   "starter UI model has parts");
    const std::size_t partCount = initial.parts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "empty variant message appears");
        QAbstractButton* okButton =
            box != nullptr ? box->button(QMessageBox::Ok) : nullptr;
        dvatest::check(okButton != nullptr,
                       "empty variant message has an ok button");
        if (okButton != nullptr) {
            okButton->click();
        }
    });

    QAction* applyVariant = actionByText(window, "Apply Model Variant...");
    dvatest::check(applyVariant != nullptr,
                   "apply model variant action exists");
    applyVariant->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.variants.empty(),
                   "empty variant apply guard leaves variants empty");
    dvatest::check(model.parts.size() == partCount,
                   "empty variant apply guard leaves parts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "empty variant apply guard leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "empty variant apply guard leaves measures unchanged");
}

TEST("main window delete variant action handles empty variant list") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectRootForTesting(),
                   "starter UI model has an assembly root item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.variants.empty(),
                   "starter UI model has no variants");
    dvatest::check(!initial.parts.empty(),
                   "starter UI model has parts");
    const std::size_t partCount = initial.parts.size();
    const std::size_t moveCount = initial.moves.size();
    const std::size_t measureCount = initial.measures.size();

    QTimer::singleShot(0, []() {
        QMessageBox* box =
            qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        dvatest::check(box != nullptr, "empty variant message appears");
        QAbstractButton* okButton =
            box != nullptr ? box->button(QMessageBox::Ok) : nullptr;
        dvatest::check(okButton != nullptr,
                       "empty variant message has an ok button");
        if (okButton != nullptr) {
            okButton->click();
        }
    });

    QAction* deleteVariant = actionByText(window, "Delete Model Variant...");
    dvatest::check(deleteVariant != nullptr,
                   "delete model variant action exists");
    deleteVariant->trigger();

    const Model& model = window.modelForTesting();
    dvatest::check(model.variants.empty(),
                   "empty variant delete guard leaves variants empty");
    dvatest::check(model.parts.size() == partCount,
                   "empty variant delete guard leaves parts unchanged");
    dvatest::check(model.moves.size() == moveCount,
                   "empty variant delete guard leaves moves unchanged");
    dvatest::check(model.measures.size() == measureCount,
                   "empty variant delete guard leaves measures unchanged");
}

TEST("main window point coordinate edit keeps point properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "X");
    dvatest::check(row >= 0, "point properties include X coordinate");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point X value item exists");

    value->setText("12.5");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::checkNear(model.parts.front().points.front().position.x, 12.5,
                       1e-12, "edited point X is saved to the model");

    const int updatedRow = propertyRow(window, "X");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after coordinate edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated point X value item exists");
    dvatest::check(updated->text().toStdString() == "12.5",
                   "updated point X remains visible");
}

TEST("main window invalid numeric edit restores point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const Vec3 position = initial.parts.front().points.front().position;

    const int row = propertyRow(window, "X");
    dvatest::check(row >= 0, "point properties include X coordinate");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point X value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid point numeric edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    const Vec3& updatedPosition = model.parts.front().points.front().position;
    dvatest::checkNear(updatedPosition.x, position.x, 1e-12,
                       "invalid numeric edit leaves point X unchanged");
    dvatest::checkNear(updatedPosition.y, position.y, 1e-12,
                       "invalid numeric edit leaves point Y unchanged");
    dvatest::checkNear(updatedPosition.z, position.z, 1e-12,
                       "invalid numeric edit leaves point Z unchanged");

    const int updatedRow = propertyRow(window, "X");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after invalid numeric edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored point X value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid numeric edit restores visible value");
}

TEST("main window unchanged numeric edit refreshes point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const Vec3 position = initial.parts.front().points.front().position;

    const int row = propertyRow(window, "X");
    dvatest::check(row >= 0, "point properties include X coordinate");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point X value item exists");
    const QString initialText = value->text();

    value->setText(initialText + ".0");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    const Vec3& updatedPosition = model.parts.front().points.front().position;
    dvatest::checkNear(updatedPosition.x, position.x, 1e-12,
                       "unchanged numeric edit leaves point X unchanged");
    dvatest::checkNear(updatedPosition.y, position.y, 1e-12,
                       "unchanged numeric edit leaves point Y unchanged");
    dvatest::checkNear(updatedPosition.z, position.z, 1e-12,
                       "unchanged numeric edit leaves point Z unchanged");

    const int updatedRow = propertyRow(window, "X");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after unchanged numeric edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed point X value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged numeric edit restores canonical visible value");
}

TEST("main window point Y coordinate edit keeps point properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "Y");
    dvatest::check(row >= 0, "point properties include Y coordinate");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point Y value item exists");

    value->setText("-7.25");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::checkNear(model.parts.front().points.front().position.y, -7.25,
                       1e-12, "edited point Y is saved to the model");

    const int updatedRow = propertyRow(window, "Y");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after Y coordinate edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated point Y value item exists");
    dvatest::check(updated->text().toStdString() == "-7.25",
                   "updated point Y remains visible");
}

TEST("main window invalid point Y coordinate edit restores point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const Vec3 position = initial.parts.front().points.front().position;

    const int row = propertyRow(window, "Y");
    dvatest::check(row >= 0, "point properties include Y coordinate");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point Y value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid point Y edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    const Vec3& updatedPosition = model.parts.front().points.front().position;
    dvatest::checkNear(updatedPosition.x, position.x, 1e-12,
                       "invalid point Y edit leaves point X unchanged");
    dvatest::checkNear(updatedPosition.y, position.y, 1e-12,
                       "invalid point Y edit leaves point Y unchanged");
    dvatest::checkNear(updatedPosition.z, position.z, 1e-12,
                       "invalid point Y edit leaves point Z unchanged");

    const int updatedRow = propertyRow(window, "Y");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after invalid Y edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored point Y value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid point Y edit restores visible value");
}

TEST("main window point Z coordinate edit keeps point properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "Z");
    dvatest::check(row >= 0, "point properties include Z coordinate");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point Z value item exists");

    value->setText("18.75");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::checkNear(model.parts.front().points.front().position.z, 18.75,
                       1e-12, "edited point Z is saved to the model");

    const int updatedRow = propertyRow(window, "Z");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after Z coordinate edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated point Z value item exists");
    dvatest::check(updated->text().toStdString() == "18.75",
                   "updated point Z remains visible");
}

TEST("main window invalid point Z coordinate edit restores point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const Vec3 position = initial.parts.front().points.front().position;

    const int row = propertyRow(window, "Z");
    dvatest::check(row >= 0, "point properties include Z coordinate");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point Z value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid point Z edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    const Vec3& updatedPosition = model.parts.front().points.front().position;
    dvatest::checkNear(updatedPosition.x, position.x, 1e-12,
                       "invalid point Z edit leaves point X unchanged");
    dvatest::checkNear(updatedPosition.y, position.y, 1e-12,
                       "invalid point Z edit leaves point Y unchanged");
    dvatest::checkNear(updatedPosition.z, position.z, 1e-12,
                       "invalid point Z edit leaves point Z unchanged");

    const int updatedRow = propertyRow(window, "Z");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after invalid Z edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored point Z value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid point Z edit restores visible value");
}

TEST("main window point active edit keeps point properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithDeactivatablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "Active");
    dvatest::check(row >= 0, "point properties include active");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point active value item exists");

    value->setCheckState(Qt::Unchecked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::check(!model.parts.front().points.front().active,
                   "edited point active flag is saved to the model");

    const int updatedRow = propertyRow(window, "Active");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after active edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated point active value item exists");
    dvatest::check(updated->checkState() == Qt::Unchecked,
                   "updated point active remains unchecked");
}

TEST("main window point diameter edit keeps point properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "Diameter");
    dvatest::check(row >= 0, "point properties include diameter");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point diameter value item exists");

    value->setText("4.25");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::checkNear(model.parts.front().points.front().diameter, 4.25,
                       1e-12, "edited point diameter is saved to the model");

    const int updatedRow = propertyRow(window, "Diameter");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after diameter edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated point diameter value item exists");
    dvatest::check(updated->text().toStdString() == "4.25",
                   "updated point diameter remains visible");
}

TEST("main window invalid point diameter edit restores point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const double diameter = initial.parts.front().points.front().diameter;

    const int row = propertyRow(window, "Diameter");
    dvatest::check(row >= 0, "point properties include diameter");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point diameter value item exists");
    const QString initialText = value->text();

    value->setText("not-a-diameter");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid point diameter edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::checkNear(model.parts.front().points.front().diameter,
                       diameter, 1e-12,
                       "invalid point diameter edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Diameter");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after invalid diameter edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored point diameter value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid point diameter edit restores visible value");
}

TEST("main window point type edit keeps point properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "Point Type");
    dvatest::check(row >= 0, "point properties include point type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point type value item exists");

    value->setText("Feature");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::check(model.parts.front().points.front().kind ==
                       PointKind::Feature,
                   "edited point type is saved to the model");

    const int updatedRow = propertyRow(window, "Point Type");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated point type value item exists");
    dvatest::check(updated->text().toStdString() == "Feature",
                   "updated point type remains visible");
}

TEST("main window invalid point type edit restores point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "Point Type");
    dvatest::check(row >= 0, "point properties include point type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point type value item exists");
    dvatest::check(value->text().toStdString() == "Coordinate",
                   "starter point type is visible");

    value->setText("Not A Point Type");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid point type",
                   "invalid point type edit reports type error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::check(model.parts.front().points.front().kind ==
                       PointKind::Coordinate,
                   "invalid point type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Point Type");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after invalid type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored point type value item exists");
    dvatest::check(updated->text().toStdString() == "Coordinate",
                   "invalid point type edit restores visible value");
}

TEST("main window unchanged point type edit refreshes point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "Point Type");
    dvatest::check(row >= 0, "point properties include point type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point type value item exists");
    dvatest::check(value->text().toStdString() == "Coordinate",
                   "starter point type is visible");

    value->setText("coordinate");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::check(model.parts.front().points.front().kind ==
                       PointKind::Coordinate,
                   "unchanged point type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Point Type");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after unchanged type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed point type value item exists");
    dvatest::check(updated->text().toStdString() == "Coordinate",
                   "unchanged point type edit restores canonical value");
}

TEST("main window point hole type edit keeps point properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "Hole Type");
    dvatest::check(row >= 0, "point properties include hole type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point hole type value item exists");

    value->setText("Hole");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::check(model.parts.front().points.front().holeType ==
                       HoleType::Hole,
                   "edited point hole type is saved to the model");

    const int updatedRow = propertyRow(window, "Hole Type");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after hole type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated point hole type value item exists");
    dvatest::check(updated->text().toStdString() == "Hole",
                   "updated point hole type remains visible");
}

TEST("main window invalid point hole type edit restores point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "Hole Type");
    dvatest::check(row >= 0, "point properties include hole type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point hole type value item exists");
    dvatest::check(value->text().toStdString() == "None",
                   "starter point hole type is visible");

    value->setText("Not A Hole Type");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid point hole type",
                   "invalid point hole type edit reports hole-type error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::check(model.parts.front().points.front().holeType ==
                       HoleType::None,
                   "invalid point hole type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Hole Type");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after invalid hole type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored point hole type value item exists");
    dvatest::check(updated->text().toStdString() == "None",
                   "invalid point hole type edit restores visible value");
}

TEST("main window unchanged point hole type edit refreshes point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "Hole Type");
    dvatest::check(row >= 0, "point properties include hole type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point hole type value item exists");
    dvatest::check(value->text().toStdString() == "None",
                   "starter point hole type is visible");

    value->setText(" none ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::check(model.parts.front().points.front().holeType ==
                       HoleType::None,
                   "unchanged point hole type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Hole Type");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after unchanged hole type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed point hole type value item exists");
    dvatest::check(updated->text().toStdString() == "None",
                   "unchanged point hole type edit restores canonical value");
}

TEST("main window point direction edit keeps point properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "I");
    dvatest::check(row >= 0, "point properties include I direction");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point I value item exists");

    value->setText("3.0");

    const double expectedI = 3.0 / std::sqrt(10.0);
    const double expectedK = 1.0 / std::sqrt(10.0);
    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::checkNear(model.parts.front().points.front().ijk.x, expectedI,
                       1e-12, "edited point I is normalized into the model");
    dvatest::checkNear(model.parts.front().points.front().ijk.z, expectedK,
                       1e-12, "edited point K is normalized into the model");

    const int updatedRow = propertyRow(window, "I");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after direction edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated point I value item exists");
    dvatest::checkNear(updated->text().toDouble(), expectedI, 1e-12,
                       "updated point I remains visible");
}

TEST("main window invalid point direction edit restores point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const Vec3 direction = initial.parts.front().points.front().ijk;

    const int row = propertyRow(window, "I");
    dvatest::check(row >= 0, "point properties include I direction");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point I value item exists");
    const QString initialText = value->text();

    value->setText("not-a-direction");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid point direction I edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    const Vec3& updatedDirection = model.parts.front().points.front().ijk;
    dvatest::checkNear(updatedDirection.x, direction.x, 1e-12,
                       "invalid point direction I edit leaves I unchanged");
    dvatest::checkNear(updatedDirection.z, direction.z, 1e-12,
                       "invalid point direction I edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "I");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after invalid direction I edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored point I value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid point direction I edit restores visible value");
}

TEST("main window point direction J edit keeps point properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "J");
    dvatest::check(row >= 0, "point properties include J direction");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point J value item exists");

    value->setText("4.0");

    const double expectedJ = 4.0 / std::sqrt(17.0);
    const double expectedK = 1.0 / std::sqrt(17.0);
    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::checkNear(model.parts.front().points.front().ijk.y, expectedJ,
                       1e-12, "edited point J is normalized into the model");
    dvatest::checkNear(model.parts.front().points.front().ijk.z, expectedK,
                       1e-12, "edited point K is normalized into the model");

    const int updatedRow = propertyRow(window, "J");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after direction J edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated point J value item exists");
    dvatest::checkNear(updated->text().toDouble(), expectedJ, 1e-12,
                       "updated point J remains visible");
}

TEST("main window invalid point direction J edit restores point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const Vec3 direction = initial.parts.front().points.front().ijk;

    const int row = propertyRow(window, "J");
    dvatest::check(row >= 0, "point properties include J direction");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point J value item exists");
    const QString initialText = value->text();

    value->setText("not-a-direction");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid point direction J edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    const Vec3& updatedDirection = model.parts.front().points.front().ijk;
    dvatest::checkNear(updatedDirection.y, direction.y, 1e-12,
                       "invalid point direction J edit leaves J unchanged");
    dvatest::checkNear(updatedDirection.z, direction.z, 1e-12,
                       "invalid point direction J edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "J");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after invalid direction J edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored point J value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid point direction J edit restores visible value");
}

TEST("main window point direction K edit keeps point properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const int row = propertyRow(window, "K");
    dvatest::check(row >= 0, "point properties include K direction");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point K value item exists");

    value->setText("-2.0");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    dvatest::checkNear(model.parts.front().points.front().ijk.z, -1.0, 1e-12,
                       "edited point K is normalized into the model");

    const int updatedRow = propertyRow(window, "K");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after direction K edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated point K value item exists");
    dvatest::checkNear(updated->text().toDouble(), -1.0, 1e-12,
                       "updated point K remains visible");
}

TEST("main window invalid point direction K edit restores point properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstPointForTesting(),
                   "starter UI model has a point item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().points.empty(),
                   "starter UI model has points");
    const Vec3 direction = initial.parts.front().points.front().ijk;

    const int row = propertyRow(window, "K");
    dvatest::check(row >= 0, "point properties include K direction");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "point K value item exists");
    const QString initialText = value->text();

    value->setText("not-a-direction");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid point direction K edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().points.empty(),
                   "edited UI model still has points");
    const Vec3& updatedDirection = model.parts.front().points.front().ijk;
    dvatest::checkNear(updatedDirection.x, direction.x, 1e-12,
                       "invalid point direction K edit leaves I unchanged");
    dvatest::checkNear(updatedDirection.z, direction.z, 1e-12,
                       "invalid point direction K edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "K");
    dvatest::check(updatedRow >= 0,
                   "point properties stay selected after invalid direction K edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored point K value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid point direction K edit restores visible value");
}

TEST("main window part rename keeps part properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstPartForTesting(),
                   "starter UI model has a part item");

    const int row = propertyRow(window, "DCS Name");
    dvatest::check(row >= 0, "part properties include DCS name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "part DCS name value item exists");

    value->setText("Renamed Part");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(model.parts.front().dcsName == "Renamed Part",
                   "edited part DCS name is saved to the model");

    const int updatedRow = propertyRow(window, "DCS Name");
    dvatest::check(updatedRow >= 0,
                   "part properties stay selected after rename");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated part name value item exists");
    dvatest::check(updated->text().toStdString() == "Renamed Part",
                   "updated part name remains visible");
}

TEST("main window unchanged part dcs name edit refreshes part properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstPartForTesting(),
                   "starter UI model has a part item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");

    const int row = propertyRow(window, "DCS Name");
    dvatest::check(row >= 0, "part properties include DCS name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "part DCS name value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(QString::fromStdString(model.parts.front().dcsName) ==
                       initialText,
                   "unchanged part DCS name edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "DCS Name");
    dvatest::check(
        updatedRow >= 0,
        "part properties stay selected after unchanged DCS name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed part DCS name value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged part DCS name edit restores canonical value");
}

TEST("main window feature type edit keeps feature properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstFeatureForTesting(),
                   "starter UI model has a feature item");

    const int row = propertyRow(window, "Feature Type");
    dvatest::check(row >= 0, "feature properties include feature type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "feature type value item exists");

    value->setText("Sphere");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().features.empty(),
                   "edited UI model still has features");
    dvatest::check(model.parts.front().features.front().kind ==
                       FeatureKind::Sphere,
                   "edited feature type is saved to the model");

    const int updatedRow = propertyRow(window, "Feature Type");
    dvatest::check(updatedRow >= 0,
                   "feature properties stay selected after type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated feature type value item exists");
    dvatest::check(updated->text().toStdString() == "Sphere",
                   "updated feature type remains visible");
}

TEST("main window invalid feature type edit restores feature properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstFeatureForTesting(),
                   "starter UI model has a feature item");

    const int row = propertyRow(window, "Feature Type");
    dvatest::check(row >= 0, "feature properties include feature type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "feature type value item exists");
    dvatest::check(value->text().toStdString() == "Point Based",
                   "starter feature type is visible");

    value->setText("Not A Feature Type");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid feature type",
                   "invalid feature type edit reports type error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().features.empty(),
                   "edited UI model still has features");
    dvatest::check(model.parts.front().features.front().kind ==
                       FeatureKind::PointBased,
                   "invalid feature type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Feature Type");
    dvatest::check(updatedRow >= 0,
                   "feature properties stay selected after invalid type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored feature type value item exists");
    dvatest::check(updated->text().toStdString() == "Point Based",
                   "invalid feature type edit restores visible value");
}

TEST("main window unchanged feature type edit refreshes feature properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstFeatureForTesting(),
                   "starter UI model has a feature item");

    const int row = propertyRow(window, "Feature Type");
    dvatest::check(row >= 0, "feature properties include feature type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "feature type value item exists");
    dvatest::check(value->text().toStdString() == "Point Based",
                   "starter feature type is visible");

    value->setText(" pointbased ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().features.empty(),
                   "edited UI model still has features");
    dvatest::check(model.parts.front().features.front().kind ==
                       FeatureKind::PointBased,
                   "unchanged feature type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Feature Type");
    dvatest::check(updatedRow >= 0,
                   "feature properties stay selected after unchanged type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed feature type value item exists");
    dvatest::check(updated->text().toStdString() == "Point Based",
                   "unchanged feature type edit restores canonical value");
}

TEST("main window feature defining points edit keeps feature properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().points.size() >= 2,
                   "starter UI model has a second point");
    const PointId replacementPoint = initial.parts.front().points[1].id;
    dvatest::check(window.selectFirstFeatureForTesting(),
                   "starter UI model has a feature item");

    const int row = propertyRow(window, "Defining Points");
    dvatest::check(row >= 0, "feature properties include defining points");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "feature defining points value item exists");

    value->setText(QString::number(replacementPoint));

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().features.empty(),
                   "edited UI model still has features");
    const std::vector<PointId>& definingPoints =
        model.parts.front().features.front().definingPoints;
    dvatest::check(definingPoints.size() == 1 &&
                       definingPoints.front() == replacementPoint,
                   "edited feature defining points are saved to the model");

    const int updatedRow = propertyRow(window, "Defining Points");
    dvatest::check(updatedRow >= 0,
                   "feature properties stay selected after defining-points edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated feature defining points value item exists");
    dvatest::check(updated->text() == QString::number(replacementPoint),
                   "updated feature defining points remain visible");
}

TEST("main window invalid feature defining points edit restores feature properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstFeatureForTesting(),
                   "starter UI model has a feature item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().features.empty(),
                   "starter UI model has features");
    const std::vector<PointId> definingPoints =
        initial.parts.front().features.front().definingPoints;
    QStringList initialParts;
    for (const PointId id : definingPoints) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Defining Points");
    dvatest::check(row >= 0, "feature properties include defining points");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "feature defining points value item exists");
    dvatest::check(value->text() == initialText,
                   "starter feature defining points are visible");

    value->setText("not-a-point-list");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid feature defining point list",
                   "invalid feature defining points edit reports point-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().features.empty(),
                   "edited UI model still has features");
    dvatest::check(model.parts.front().features.front().definingPoints ==
                       definingPoints,
                   "invalid feature defining points edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Defining Points");
    dvatest::check(
        updatedRow >= 0,
        "feature properties stay selected after invalid defining-points edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored feature defining points value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid feature defining points edit restores visible value");
}

TEST("main window unchanged feature defining points edit refreshes feature properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditablePoint());
    dvatest::check(window.selectFirstFeatureForTesting(),
                   "starter UI model has a feature item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().features.empty(),
                   "starter UI model has features");
    const std::vector<PointId> definingPoints =
        initial.parts.front().features.front().definingPoints;
    QStringList initialParts;
    for (const PointId id : definingPoints) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Defining Points");
    dvatest::check(row >= 0, "feature properties include defining points");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "feature defining points value item exists");
    dvatest::check(value->text() == initialText,
                   "starter feature defining points are visible");

    value->setText(initialText + ", ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().features.empty(),
                   "edited UI model still has features");
    dvatest::check(model.parts.front().features.front().definingPoints ==
                       definingPoints,
                   "unchanged feature defining points edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Defining Points");
    dvatest::check(
        updatedRow >= 0,
        "feature properties stay selected after unchanged defining-points edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed feature defining points value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged feature defining points edit restores canonical value");
}

TEST("main window gdt type edit keeps gdt properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const int row = propertyRow(window, "GD&T Type");
    dvatest::check(row >= 0, "GD&T properties include type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T type value item exists");

    value->setText("Surface Profile");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().type ==
                       GdtType::SurfaceProfile,
                   "edited GD&T type is saved to the model");

    const int updatedRow = propertyRow(window, "GD&T Type");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated GD&T type value item exists");
    dvatest::check(updated->text().toStdString() == "Surface Profile",
                   "updated GD&T type remains visible");
}

TEST("main window invalid gdt type edit restores gdt properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const int row = propertyRow(window, "GD&T Type");
    dvatest::check(row >= 0, "GD&T properties include type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T type value item exists");
    dvatest::check(value->text().toStdString() == "Flatness",
                   "starter GD&T type is visible");

    value->setText("Not A GD&T Type");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid GD&T type",
                   "invalid GD&T type edit reports type error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().type == GdtType::Flatness,
                   "invalid GD&T type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "GD&T Type");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after invalid type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored GD&T type value item exists");
    dvatest::check(updated->text().toStdString() == "Flatness",
                   "invalid GD&T type edit restores visible value");
}

TEST("main window unchanged gdt type edit refreshes gdt properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const int row = propertyRow(window, "GD&T Type");
    dvatest::check(row >= 0, "GD&T properties include type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T type value item exists");
    dvatest::check(value->text().toStdString() == "Flatness",
                   "starter GD&T type is visible");

    value->setText(" flat ness ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().type == GdtType::Flatness,
                   "unchanged GD&T type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "GD&T Type");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after unchanged type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T type value item exists");
    dvatest::check(updated->text().toStdString() == "Flatness",
                   "unchanged GD&T type edit restores canonical value");
}

TEST("main window gdt name edit keeps gdt properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "GD&T properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T name value item exists");

    value->setText("RenamedGdt");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().name == "RenamedGdt",
                   "edited GD&T name is saved to the model");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated GD&T name value item exists");
    dvatest::check(updated->text().toStdString() == "RenamedGdt",
                   "updated GD&T name remains visible");
}

TEST("main window unchanged gdt name edit refreshes gdt properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().gdts.empty(),
                   "starter UI model has GD&T");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "GD&T properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T name value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(QString::fromStdString(model.parts.front().gdts.front().name) ==
                       initialText,
                   "unchanged GD&T name edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after unchanged name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed GD&T name value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged GD&T name edit restores canonical value");
}

TEST("main window gdt active edit keeps gdt properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const int row = propertyRow(window, "Active");
    dvatest::check(row >= 0, "GD&T properties include active");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T active value item exists");

    value->setCheckState(Qt::Unchecked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(!model.parts.front().gdts.front().active,
                   "edited GD&T active flag is saved to the model");

    const int updatedRow = propertyRow(window, "Active");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after active edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T active value item exists");
    dvatest::check(updated->checkState() == Qt::Unchecked,
                   "updated GD&T active remains unchecked");
}

TEST("main window gdt range edit keeps gdt properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const int row = propertyRow(window, "Range");
    dvatest::check(row >= 0, "GD&T properties include range");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T range value item exists");

    value->setText("0.42");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::checkNear(model.parts.front().gdts.front().range, 0.42, 1e-12,
                       "edited GD&T range is saved to the model");

    const int updatedRow = propertyRow(window, "Range");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after range edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated GD&T range value item exists");
    dvatest::check(updated->text().toStdString() == "0.42",
                   "updated GD&T range remains visible");
}

TEST("main window invalid gdt range edit restores gdt properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().gdts.empty(),
                   "starter UI model has GD&T");
    const double range = initial.parts.front().gdts.front().range;

    const int row = propertyRow(window, "Range");
    dvatest::check(row >= 0, "GD&T properties include range");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T range value item exists");
    const QString initialText = value->text();

    value->setText("not-a-range");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid GD&T range edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::checkNear(model.parts.front().gdts.front().range, range, 1e-12,
                       "invalid GD&T range edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Range");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after invalid range edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "restored GD&T range value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid GD&T range edit restores previous value");
}

TEST("main window gdt diametrical edit keeps gdt properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const int row = propertyRow(window, "Diametrical");
    dvatest::check(row >= 0, "GD&T properties include diametrical");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T diametrical value item exists");

    value->setCheckState(Qt::Checked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().diametrical,
                   "edited GD&T diametrical flag is saved to the model");

    const int updatedRow = propertyRow(window, "Diametrical");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after diametrical edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T diametrical value item exists");
    dvatest::check(updated->checkState() == Qt::Checked,
                   "updated GD&T diametrical flag remains checked");
}

TEST("main window gdt drf primary edit keeps gdt properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdtDrf());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& before = window.modelForTesting();
    dvatest::check(!before.parts.empty(), "starter UI model has parts");
    dvatest::check(before.parts.front().features.size() >= 2,
                   "starter UI model has a datum feature");
    const FeatureId datum = before.parts.front().features.front().id;

    const int row = propertyRow(window, "DRF Primary");
    dvatest::check(row >= 0, "GD&T properties include DRF primary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF primary value item exists");

    value->setText(QString::number(datum));

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().drf.primary == datum,
                   "edited GD&T DRF primary is saved to the model");

    const int updatedRow = propertyRow(window, "DRF Primary");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after DRF primary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T DRF primary value item exists");
    dvatest::check(updated->text().toStdString() == std::to_string(datum),
                   "updated GD&T DRF primary remains visible");
}

TEST("main window gdt drf primary edit keeps selected datum visible") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdtDrf());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& before = window.modelForTesting();
    dvatest::check(!before.parts.empty(), "starter UI model has parts");
    dvatest::check(before.parts.front().features.size() >= 2,
                   "starter UI model has two datum features");
    const FeatureId datum = before.parts.front().features[1].id;

    const int row = propertyRow(window, "DRF Primary");
    dvatest::check(row >= 0, "GD&T properties include DRF primary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF primary value item exists");

    const QString datumText = QString::number(datum);
    value->setText(datumText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().drf.primary == datum,
                   "edited GD&T selected DRF primary is saved");

    const int updatedRow = propertyRow(window, "DRF Primary");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after DRF primary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T DRF primary value item exists");
    dvatest::check(updated->text() == datumText,
                   "updated GD&T selected DRF primary remains visible");
}

TEST("main window invalid gdt drf primary edit restores gdt properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdtDrfSecondary());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().gdts.empty(),
                   "starter UI model has GD&T");
    const DatumReferenceFrame drf = initial.parts.front().gdts.front().drf;

    const int row = propertyRow(window, "DRF Primary");
    dvatest::check(row >= 0, "GD&T properties include DRF primary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF primary value item exists");
    const QString initialText = value->text();

    value->setText("not-a-feature-id");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid DRF feature id",
                   "invalid GD&T DRF primary edit reports feature-id error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    const DatumReferenceFrame& updatedDrf =
        model.parts.front().gdts.front().drf;
    dvatest::check(updatedDrf.primary == drf.primary,
                   "invalid GD&T DRF primary edit leaves primary unchanged");
    dvatest::check(updatedDrf.secondary == drf.secondary,
                   "invalid GD&T DRF primary edit leaves secondary unchanged");
    dvatest::check(updatedDrf.tertiary == drf.tertiary,
                   "invalid GD&T DRF primary edit leaves tertiary unchanged");

    const int updatedRow = propertyRow(window, "DRF Primary");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after invalid DRF primary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored GD&T DRF primary value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid GD&T DRF primary edit restores visible value");
}

TEST("main window unchanged gdt drf primary edit refreshes gdt properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdtDrfSecondary());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().gdts.empty(),
                   "starter UI model has GD&T");
    const DatumReferenceFrame drf = initial.parts.front().gdts.front().drf;

    const int row = propertyRow(window, "DRF Primary");
    dvatest::check(row >= 0, "GD&T properties include DRF primary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF primary value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    const DatumReferenceFrame& updatedDrf =
        model.parts.front().gdts.front().drf;
    dvatest::check(updatedDrf.primary == drf.primary,
                   "unchanged GD&T DRF primary edit leaves primary unchanged");
    dvatest::check(updatedDrf.secondary == drf.secondary,
                   "unchanged GD&T DRF primary edit leaves secondary unchanged");
    dvatest::check(updatedDrf.tertiary == drf.tertiary,
                   "unchanged GD&T DRF primary edit leaves tertiary unchanged");

    const int updatedRow = propertyRow(window, "DRF Primary");
    dvatest::check(
        updatedRow >= 0,
        "GD&T properties stay selected after unchanged DRF primary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed GD&T DRF primary value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged GD&T DRF primary edit restores canonical value");
}

TEST("main window gdt drf secondary edit keeps gdt properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdtDrfSecondary());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& before = window.modelForTesting();
    dvatest::check(!before.parts.empty(), "starter UI model has parts");
    dvatest::check(before.parts.front().features.size() >= 2,
                   "starter UI model has a secondary datum feature");
    const FeatureId datum = before.parts.front().features[1].id;

    const int row = propertyRow(window, "DRF Secondary");
    dvatest::check(row >= 0, "GD&T properties include DRF secondary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF secondary value item exists");

    value->setText(QString::number(datum));

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().drf.secondary == datum,
                   "edited GD&T DRF secondary is saved to the model");

    const int updatedRow = propertyRow(window, "DRF Secondary");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after DRF secondary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T DRF secondary value item exists");
    dvatest::check(updated->text().toStdString() == std::to_string(datum),
                   "updated GD&T DRF secondary remains visible");
}

TEST("main window gdt drf secondary edit keeps selected datum visible") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdtDrfTertiary());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& before = window.modelForTesting();
    dvatest::check(!before.parts.empty(), "starter UI model has parts");
    dvatest::check(before.parts.front().features.size() >= 3,
                   "starter UI model has three datum features");
    const FeatureId datum = before.parts.front().features[2].id;

    const int row = propertyRow(window, "DRF Secondary");
    dvatest::check(row >= 0, "GD&T properties include DRF secondary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF secondary value item exists");

    const QString datumText = QString::number(datum);
    value->setText(datumText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().drf.secondary == datum,
                   "edited GD&T selected DRF secondary is saved");

    const int updatedRow = propertyRow(window, "DRF Secondary");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after DRF secondary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T DRF secondary value item exists");
    dvatest::check(updated->text() == datumText,
                   "updated GD&T selected DRF secondary remains visible");
}

TEST("main window unchanged gdt drf secondary edit refreshes gdt properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdtDrfTertiary());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().gdts.empty(),
                   "starter UI model has GD&T");
    const DatumReferenceFrame drf = initial.parts.front().gdts.front().drf;

    const int row = propertyRow(window, "DRF Secondary");
    dvatest::check(row >= 0, "GD&T properties include DRF secondary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF secondary value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    const DatumReferenceFrame& updatedDrf =
        model.parts.front().gdts.front().drf;
    dvatest::check(updatedDrf.primary == drf.primary,
                   "unchanged GD&T DRF secondary edit leaves primary unchanged");
    dvatest::check(updatedDrf.secondary == drf.secondary,
                   "unchanged GD&T DRF secondary edit leaves secondary unchanged");
    dvatest::check(updatedDrf.tertiary == drf.tertiary,
                   "unchanged GD&T DRF secondary edit leaves tertiary unchanged");

    const int updatedRow = propertyRow(window, "DRF Secondary");
    dvatest::check(
        updatedRow >= 0,
        "GD&T properties stay selected after unchanged DRF secondary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed GD&T DRF secondary value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged GD&T DRF secondary edit restores canonical value");
}

TEST("main window invalid gdt drf secondary edit restores gdt properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdtDrfTertiary());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().gdts.empty(),
                   "starter UI model has GD&T");
    const DatumReferenceFrame drf = initial.parts.front().gdts.front().drf;

    const int row = propertyRow(window, "DRF Secondary");
    dvatest::check(row >= 0, "GD&T properties include DRF secondary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF secondary value item exists");
    const QString initialText = value->text();

    value->setText("not-a-feature-id");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid DRF feature id",
                   "invalid GD&T DRF secondary edit reports feature-id error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    const DatumReferenceFrame& updatedDrf =
        model.parts.front().gdts.front().drf;
    dvatest::check(updatedDrf.primary == drf.primary,
                   "invalid GD&T DRF secondary edit leaves primary unchanged");
    dvatest::check(updatedDrf.secondary == drf.secondary,
                   "invalid GD&T DRF secondary edit leaves secondary unchanged");
    dvatest::check(updatedDrf.tertiary == drf.tertiary,
                   "invalid GD&T DRF secondary edit leaves tertiary unchanged");

    const int updatedRow = propertyRow(window, "DRF Secondary");
    dvatest::check(
        updatedRow >= 0,
        "GD&T properties stay selected after invalid DRF secondary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored GD&T DRF secondary value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid GD&T DRF secondary edit restores visible value");
}

TEST("main window gdt drf tertiary edit keeps gdt properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdtDrfTertiary());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& before = window.modelForTesting();
    dvatest::check(!before.parts.empty(), "starter UI model has parts");
    dvatest::check(before.parts.front().features.size() >= 3,
                   "starter UI model has a tertiary datum feature");
    const FeatureId datum = before.parts.front().features[2].id;

    const int row = propertyRow(window, "DRF Tertiary");
    dvatest::check(row >= 0, "GD&T properties include DRF tertiary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF tertiary value item exists");

    value->setText(QString::number(datum));

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().drf.tertiary == datum,
                   "edited GD&T DRF tertiary is saved to the model");

    const int updatedRow = propertyRow(window, "DRF Tertiary");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after DRF tertiary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T DRF tertiary value item exists");
    dvatest::check(updated->text().toStdString() == std::to_string(datum),
                   "updated GD&T DRF tertiary remains visible");
}

TEST("main window gdt drf tertiary edit keeps selected datum visible") {
    ui::MainWindow window;
    Model starter = modelWithEditableGdtDrfTertiary();
    dvatest::check(!starter.parts.empty(), "starter model has parts");
    const PartId partId = starter.parts.front().id;
    addCoordinatePoint(starter, {40.0, 0.0, 0.0}, partId);
    window.setModelForTesting(starter);
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& before = window.modelForTesting();
    dvatest::check(!before.parts.empty(), "starter UI model has parts");
    dvatest::check(before.parts.front().features.size() >= 4,
                   "starter UI model has four datum features");
    const FeatureId datum = before.parts.front().features[3].id;

    const int row = propertyRow(window, "DRF Tertiary");
    dvatest::check(row >= 0, "GD&T properties include DRF tertiary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF tertiary value item exists");

    const QString datumText = QString::number(datum);
    value->setText(datumText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().drf.tertiary == datum,
                   "edited GD&T selected DRF tertiary is saved");

    const int updatedRow = propertyRow(window, "DRF Tertiary");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after DRF tertiary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T DRF tertiary value item exists");
    dvatest::check(updated->text() == datumText,
                   "updated GD&T selected DRF tertiary remains visible");
}

TEST("main window unchanged gdt drf tertiary edit refreshes gdt properties") {
    ui::MainWindow window;
    Model starter = modelWithEditableGdtDrfTertiary();
    dvatest::check(!starter.parts.empty(), "starter model has parts");
    const PartId partId = starter.parts.front().id;
    addCoordinatePoint(starter, {40.0, 0.0, 0.0}, partId);
    DatumReferenceFrame starterDrf =
        starter.parts.front().gdts.front().drf;
    starterDrf.tertiary = starter.parts.front().features[3].id;
    setGdtDrf(starter, starter.parts.front().gdts.front().id, starterDrf);
    window.setModelForTesting(starter);
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().gdts.empty(),
                   "starter UI model has GD&T");
    const DatumReferenceFrame drf = initial.parts.front().gdts.front().drf;

    const int row = propertyRow(window, "DRF Tertiary");
    dvatest::check(row >= 0, "GD&T properties include DRF tertiary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF tertiary value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    const DatumReferenceFrame& updatedDrf =
        model.parts.front().gdts.front().drf;
    dvatest::check(updatedDrf.primary == drf.primary,
                   "unchanged GD&T DRF tertiary edit leaves primary unchanged");
    dvatest::check(updatedDrf.secondary == drf.secondary,
                   "unchanged GD&T DRF tertiary edit leaves secondary unchanged");
    dvatest::check(updatedDrf.tertiary == drf.tertiary,
                   "unchanged GD&T DRF tertiary edit leaves tertiary unchanged");

    const int updatedRow = propertyRow(window, "DRF Tertiary");
    dvatest::check(
        updatedRow >= 0,
        "GD&T properties stay selected after unchanged DRF tertiary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed GD&T DRF tertiary value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged GD&T DRF tertiary edit restores canonical value");
}

TEST("main window invalid gdt drf tertiary edit restores gdt properties") {
    ui::MainWindow window;
    Model starter = modelWithEditableGdtDrfTertiary();
    dvatest::check(!starter.parts.empty(), "starter model has parts");
    const PartId partId = starter.parts.front().id;
    addCoordinatePoint(starter, {40.0, 0.0, 0.0}, partId);
    DatumReferenceFrame starterDrf =
        starter.parts.front().gdts.front().drf;
    starterDrf.tertiary = starter.parts.front().features[3].id;
    setGdtDrf(starter, starter.parts.front().gdts.front().id, starterDrf);
    window.setModelForTesting(starter);
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().gdts.empty(),
                   "starter UI model has GD&T");
    const DatumReferenceFrame drf = initial.parts.front().gdts.front().drf;

    const int row = propertyRow(window, "DRF Tertiary");
    dvatest::check(row >= 0, "GD&T properties include DRF tertiary");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "GD&T DRF tertiary value item exists");
    const QString initialText = value->text();

    value->setText("not-a-feature-id");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid DRF feature id",
                   "invalid GD&T DRF tertiary edit reports feature-id error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    const DatumReferenceFrame& updatedDrf =
        model.parts.front().gdts.front().drf;
    dvatest::check(updatedDrf.primary == drf.primary,
                   "invalid GD&T DRF tertiary edit leaves primary unchanged");
    dvatest::check(updatedDrf.secondary == drf.secondary,
                   "invalid GD&T DRF tertiary edit leaves secondary unchanged");
    dvatest::check(updatedDrf.tertiary == drf.tertiary,
                   "invalid GD&T DRF tertiary edit leaves tertiary unchanged");

    const int updatedRow = propertyRow(window, "DRF Tertiary");
    dvatest::check(
        updatedRow >= 0,
        "GD&T properties stay selected after invalid DRF tertiary edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored GD&T DRF tertiary value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid GD&T DRF tertiary edit restores visible value");
}

TEST("main window gdt controlled features edit keeps gdt properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& before = window.modelForTesting();
    dvatest::check(!before.parts.empty(), "starter UI model has parts");
    dvatest::check(before.parts.front().features.size() >= 2,
                   "starter UI model has editable features");
    const FeatureId controlled = before.parts.front().features.front().id;

    const int row = propertyRow(window, "Controlled Features");
    dvatest::check(row >= 0, "GD&T properties include controlled features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "GD&T controlled features value item exists");

    value->setText(QString::number(controlled));

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().features.size() == 1,
                   "edited GD&T has one controlled feature");
    dvatest::check(model.parts.front().gdts.front().features.front() ==
                       controlled,
                   "edited GD&T controlled feature is saved to the model");

    const int updatedRow = propertyRow(window, "Controlled Features");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after feature edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T controlled features value item exists");
    dvatest::check(updated->text().toStdString() ==
                       std::to_string(controlled),
                   "updated GD&T controlled feature remains visible");
}

TEST("main window gdt controlled features edit keeps selected feature visible") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& before = window.modelForTesting();
    dvatest::check(!before.parts.empty(), "starter UI model has parts");
    dvatest::check(before.parts.front().features.size() >= 2,
                   "starter UI model has two editable features");
    const FeatureId selected = before.parts.front().features[1].id;

    const int row = propertyRow(window, "Controlled Features");
    dvatest::check(row >= 0, "GD&T properties include controlled features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "GD&T controlled features value item exists");

    const QString selectedText = QString::number(selected);
    value->setText(selectedText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    dvatest::check(model.parts.front().gdts.front().features.size() == 1,
                   "edited GD&T keeps one controlled feature");
    dvatest::check(model.parts.front().gdts.front().features.front() ==
                       selected,
                   "edited GD&T selected controlled feature is saved");

    const int updatedRow = propertyRow(window, "Controlled Features");
    dvatest::check(updatedRow >= 0,
                   "GD&T properties stay selected after feature-list edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated GD&T controlled features value item exists");
    dvatest::check(updated->text() == selectedText,
                   "updated GD&T selected controlled feature remains visible");
}

TEST("main window invalid gdt controlled features edit restores gdt properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().gdts.empty(),
                   "starter UI model has GD&T");
    const std::vector<FeatureId> features =
        initial.parts.front().gdts.front().features;
    QStringList initialParts;
    for (const FeatureId id : features) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Controlled Features");
    dvatest::check(row >= 0, "GD&T properties include controlled features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "GD&T controlled features value item exists");
    dvatest::check(value->text() == initialText,
                   "starter GD&T controlled features are visible");

    value->setText("not-a-feature-list");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid controlled feature list",
                   "invalid GD&T controlled features edit reports feature-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    const std::vector<FeatureId>& updatedFeatures =
        model.parts.front().gdts.front().features;
    dvatest::check(updatedFeatures == features,
                   "invalid GD&T controlled features edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Controlled Features");
    dvatest::check(
        updatedRow >= 0,
        "GD&T properties stay selected after invalid controlled features edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored GD&T controlled features value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid GD&T controlled features edit restores visible value");
}

TEST("main window unchanged gdt controlled features edit refreshes gdt properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableGdt());
    dvatest::check(window.selectFirstGdtForTesting(),
                   "starter UI model has a GD&T item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().gdts.empty(),
                   "starter UI model has GD&T");
    const std::vector<FeatureId> features =
        initial.parts.front().gdts.front().features;
    QStringList initialParts;
    for (const FeatureId id : features) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Controlled Features");
    dvatest::check(row >= 0, "GD&T properties include controlled features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "GD&T controlled features value item exists");
    dvatest::check(value->text() == initialText,
                   "starter GD&T controlled features are visible");

    value->setText(initialText + ", ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().gdts.empty(),
                   "edited UI model still has GD&T");
    const std::vector<FeatureId>& updatedFeatures =
        model.parts.front().gdts.front().features;
    dvatest::check(updatedFeatures == features,
                   "unchanged GD&T controlled features edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Controlled Features");
    dvatest::check(
        updatedRow >= 0,
        "GD&T properties stay selected after unchanged controlled features edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed GD&T controlled features value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged GD&T controlled features edit restores canonical value");
}

TEST("main window tolerance range edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Range");
    dvatest::check(row >= 0, "tolerance properties include range");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance range value item exists");

    value->setText("0.75");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(!model.parts.front().tolerances.front().ir.rands.empty(),
                   "edited tolerance still has random variables");
    dvatest::checkNear(model.parts.front().tolerances.front().ir.rands.front().range,
                       0.75, 1e-12,
                       "edited tolerance range is saved to the model");

    const int updatedRow = propertyRow(window, "Range");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after range edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance range value item exists");
    dvatest::check(updated->text().toStdString() == "0.75",
                   "updated tolerance range remains visible");
}

TEST("main window invalid tolerance range edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    dvatest::check(!initial.parts.front().tolerances.front().ir.rands.empty(),
                   "starter tolerance has random variables");
    const double range =
        initial.parts.front().tolerances.front().ir.rands.front().range;

    const int row = propertyRow(window, "Range");
    dvatest::check(row >= 0, "tolerance properties include range");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance range value item exists");
    const QString initialText = value->text();

    value->setText("not-a-range");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid tolerance range edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(!model.parts.front().tolerances.front().ir.rands.empty(),
                   "edited tolerance still has random variables");
    dvatest::checkNear(
        model.parts.front().tolerances.front().ir.rands.front().range,
        range, 1e-12,
        "invalid tolerance range edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Range");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after invalid range edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance range value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance range edit restores visible value");
}

TEST("main window tolerance name edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "tolerance properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance name value item exists");

    value->setText("RenamedTol");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(model.parts.front().tolerances.front().name ==
                       "RenamedTol",
                   "edited tolerance name is saved to the model");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance name value item exists");
    dvatest::check(updated->text().toStdString() == "RenamedTol",
                   "updated tolerance name remains visible");
}

TEST("main window unchanged tolerance name edit refreshes tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "tolerance properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance name value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(
        QString::fromStdString(model.parts.front().tolerances.front().name) ==
            initialText,
        "unchanged tolerance name edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after unchanged name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed tolerance name value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged tolerance name edit restores canonical value");
}

TEST("main window tolerance active edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Active");
    dvatest::check(row >= 0, "tolerance properties include active");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance active value item exists");

    value->setCheckState(Qt::Unchecked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(!model.parts.front().tolerances.front().active,
                   "edited tolerance active flag is saved to the model");

    const int updatedRow = propertyRow(window, "Active");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after active edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance active value item exists");
    dvatest::check(updated->checkState() == Qt::Unchecked,
                   "updated tolerance active remains unchecked");
}

TEST("main window tolerance distribution edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Distribution");
    dvatest::check(row >= 0, "tolerance properties include distribution");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance distribution value item exists");

    value->setText("Uniform");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(!model.parts.front().tolerances.front().ir.rands.empty(),
                   "edited tolerance still has random variables");
    dvatest::check(model.parts.front()
                       .tolerances.front()
                       .ir.rands.front()
                       .distribution == DistributionType::Uniform,
                   "edited tolerance distribution is saved to the model");

    const int updatedRow = propertyRow(window, "Distribution");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after distribution edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance distribution value item exists");
    dvatest::check(updated->text().toStdString() == "Uniform",
                   "updated tolerance distribution remains visible");
}

TEST("main window invalid tolerance distribution edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Distribution");
    dvatest::check(row >= 0, "tolerance properties include distribution");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance distribution value item exists");
    dvatest::check(value->text().toStdString() == "Normal",
                   "starter tolerance distribution is visible");

    value->setText("Not A Distribution");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid distribution",
                   "invalid tolerance distribution edit reports distribution error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(!model.parts.front().tolerances.front().ir.rands.empty(),
                   "edited tolerance still has random variables");
    dvatest::check(model.parts.front()
                       .tolerances.front()
                       .ir.rands.front()
                       .distribution == DistributionType::Normal,
                   "invalid tolerance distribution edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Distribution");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after invalid distribution edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance distribution value item exists");
    dvatest::check(updated->text().toStdString() == "Normal",
                   "invalid tolerance distribution edit restores visible value");
}

TEST("main window unchanged tolerance distribution edit refreshes tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Distribution");
    dvatest::check(row >= 0, "tolerance properties include distribution");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance distribution value item exists");
    dvatest::check(value->text().toStdString() == "Normal",
                   "starter tolerance distribution is visible");

    value->setText(" normal ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(!model.parts.front().tolerances.front().ir.rands.empty(),
                   "edited tolerance still has random variables");
    dvatest::check(model.parts.front()
                       .tolerances.front()
                       .ir.rands.front()
                       .distribution == DistributionType::Normal,
                   "unchanged tolerance distribution edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Distribution");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after unchanged distribution edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance distribution value item exists");
    dvatest::check(updated->text().toStdString() == "Normal",
                   "unchanged tolerance distribution edit restores canonical value");
}

TEST("main window tolerance offset edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Offset");
    dvatest::check(row >= 0, "tolerance properties include offset");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance offset value item exists");

    value->setText("0.125");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(!model.parts.front().tolerances.front().ir.rands.empty(),
                   "edited tolerance still has random variables");
    dvatest::checkNear(
        model.parts.front().tolerances.front().ir.rands.front().offset,
        0.125, 1e-12, "edited tolerance offset is saved to the model");

    const int updatedRow = propertyRow(window, "Offset");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after offset edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance offset value item exists");
    dvatest::check(updated->text().toStdString() == "0.125",
                   "updated tolerance offset remains visible");
}

TEST("main window invalid tolerance offset edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    dvatest::check(!initial.parts.front().tolerances.front().ir.rands.empty(),
                   "starter tolerance has random variables");
    const double offset =
        initial.parts.front().tolerances.front().ir.rands.front().offset;

    const int row = propertyRow(window, "Offset");
    dvatest::check(row >= 0, "tolerance properties include offset");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance offset value item exists");
    const QString initialText = value->text();

    value->setText("not-an-offset");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid tolerance offset edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(!model.parts.front().tolerances.front().ir.rands.empty(),
                   "edited tolerance still has random variables");
    dvatest::checkNear(
        model.parts.front().tolerances.front().ir.rands.front().offset,
        offset, 1e-12,
        "invalid tolerance offset edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Offset");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after invalid offset edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance offset value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance offset edit restores visible value");
}

TEST("main window tolerance sigma number edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Sigma Number");
    dvatest::check(row >= 0, "tolerance properties include sigma number");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance sigma number value item exists");

    value->setText("4.5");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(!model.parts.front().tolerances.front().ir.rands.empty(),
                   "edited tolerance still has random variables");
    dvatest::checkNear(
        model.parts.front().tolerances.front().ir.rands.front().sigmaNum,
        4.5, 1e-12,
        "edited tolerance sigma number is saved to the model");

    const int updatedRow = propertyRow(window, "Sigma Number");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after sigma number edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance sigma number value item exists");
    dvatest::check(updated->text().toStdString() == "4.5",
                   "updated tolerance sigma number remains visible");
}

TEST("main window invalid tolerance sigma number edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    dvatest::check(!initial.parts.front().tolerances.front().ir.rands.empty(),
                   "starter tolerance has random variables");
    const double sigmaNum =
        initial.parts.front().tolerances.front().ir.rands.front().sigmaNum;

    const int row = propertyRow(window, "Sigma Number");
    dvatest::check(row >= 0, "tolerance properties include sigma number");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance sigma number value item exists");
    const QString initialText = value->text();

    value->setText("not-a-sigma");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid tolerance sigma number edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(!model.parts.front().tolerances.front().ir.rands.empty(),
                   "edited tolerance still has random variables");
    dvatest::checkNear(
        model.parts.front().tolerances.front().ir.rands.front().sigmaNum,
        sigmaNum, 1e-12,
        "invalid tolerance sigma number edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Sigma Number");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after invalid sigma number edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance sigma number value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance sigma number edit restores visible value");
}

TEST("main window tolerance geom rule edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Geom Rule");
    dvatest::check(row >= 0, "tolerance properties include geom rule");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance geom rule value item exists");

    value->setText("Diameter Scale");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(model.parts.front().tolerances.front().ir.geomRule ==
                       GeomRule::DiameterScale,
                   "edited tolerance geom rule is saved to the model");

    const int updatedRow = propertyRow(window, "Geom Rule");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after geom rule edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance geom rule value item exists");
    dvatest::check(updated->text().toStdString() == "Diameter Scale",
                   "updated tolerance geom rule remains visible");
}

TEST("main window invalid tolerance geom rule edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Geom Rule");
    dvatest::check(row >= 0, "tolerance properties include geom rule");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance geom rule value item exists");
    dvatest::check(value->text().toStdString() == "Translate Along Vector",
                   "starter tolerance geom rule is visible");

    value->setText("Not A Geometry Rule");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid geometry rule",
                   "invalid tolerance geom rule edit reports rule error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(model.parts.front().tolerances.front().ir.geomRule ==
                       GeomRule::TranslateAlongVector,
                   "invalid tolerance geom rule edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Geom Rule");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after invalid geom rule edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance geom rule value item exists");
    dvatest::check(updated->text().toStdString() == "Translate Along Vector",
                   "invalid tolerance geom rule edit restores visible value");
}

TEST("main window unchanged tolerance geom rule edit refreshes tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Geom Rule");
    dvatest::check(row >= 0, "tolerance properties include geom rule");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance geom rule value item exists");
    dvatest::check(value->text().toStdString() == "Translate Along Vector",
                   "starter tolerance geom rule is visible");

    value->setText(" translate-along-vector ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(model.parts.front().tolerances.front().ir.geomRule ==
                       GeomRule::TranslateAlongVector,
                   "unchanged tolerance geom rule edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Geom Rule");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after unchanged geom rule edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance geom rule value item exists");
    dvatest::check(updated->text().toStdString() == "Translate Along Vector",
                   "unchanged tolerance geom rule edit restores canonical value");
}

TEST("main window tolerance range scale edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Range Scale");
    dvatest::check(row >= 0, "tolerance properties include range scale");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance range scale value item exists");

    value->setText("1.25");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::checkNear(model.parts.front().tolerances.front().ir.rangeScale,
                       1.25, 1e-12,
                       "edited tolerance range scale is saved to the model");

    const int updatedRow = propertyRow(window, "Range Scale");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after range scale edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance range scale value item exists");
    dvatest::check(updated->text().toStdString() == "1.25",
                   "updated tolerance range scale remains visible");
}

TEST("main window invalid tolerance range scale edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    const double rangeScale =
        initial.parts.front().tolerances.front().ir.rangeScale;

    const int row = propertyRow(window, "Range Scale");
    dvatest::check(row >= 0, "tolerance properties include range scale");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance range scale value item exists");
    const QString initialText = value->text();

    value->setText("not-a-range-scale");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid tolerance range scale edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::checkNear(model.parts.front().tolerances.front().ir.rangeScale,
                       rangeScale, 1e-12,
                       "invalid tolerance range scale edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Range Scale");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after invalid range scale edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance range scale value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance range scale edit restores visible value");
}

TEST("main window tolerance direction edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Direction I");
    dvatest::check(row >= 0, "tolerance properties include direction I");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance direction I value item exists");

    value->setText("3.0");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const Vec3& direction =
        model.parts.front().tolerances.front().ir.direction.ijk;
    dvatest::checkNear(direction.x, 0.9486832980505138, 1e-12,
                       "edited tolerance direction I is normalized");
    dvatest::checkNear(direction.z, 0.31622776601683794, 1e-12,
                       "edited tolerance direction keeps previous K component");

    const int updatedRow = propertyRow(window, "Direction I");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after direction edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance direction I value item exists");
    dvatest::check(updated->text().toStdString() == "0.948683298051",
                   "updated tolerance direction I shows normalized value");
}

TEST("main window invalid tolerance direction edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    const Vec3 direction =
        initial.parts.front().tolerances.front().ir.direction.ijk;

    const int row = propertyRow(window, "Direction I");
    dvatest::check(row >= 0, "tolerance properties include direction I");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance direction I value item exists");
    const QString initialText = value->text();

    value->setText("not-a-direction");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid tolerance direction I edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const Vec3& updatedDirection =
        model.parts.front().tolerances.front().ir.direction.ijk;
    dvatest::checkNear(updatedDirection.x, direction.x, 1e-12,
                       "invalid tolerance direction I edit leaves I unchanged");
    dvatest::checkNear(updatedDirection.z, direction.z, 1e-12,
                       "invalid tolerance direction I edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction I");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after invalid direction I edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance direction I value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance direction I edit restores visible value");
}

TEST("main window tolerance direction J edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Direction J");
    dvatest::check(row >= 0, "tolerance properties include direction J");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance direction J value item exists");

    value->setText("4.0");

    const double expectedJ = 4.0 / std::sqrt(17.0);
    const double expectedK = 1.0 / std::sqrt(17.0);
    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const Vec3& direction =
        model.parts.front().tolerances.front().ir.direction.ijk;
    dvatest::checkNear(direction.y, expectedJ, 1e-12,
                       "edited tolerance direction J is normalized");
    dvatest::checkNear(direction.z, expectedK, 1e-12,
                       "edited tolerance direction keeps previous K component");

    const int updatedRow = propertyRow(window, "Direction J");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after direction J edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance direction J value item exists");
    dvatest::checkNear(updated->text().toDouble(), expectedJ, 1e-12,
                       "updated tolerance direction J shows normalized value");
}

TEST("main window invalid tolerance direction J edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    const Vec3 direction =
        initial.parts.front().tolerances.front().ir.direction.ijk;

    const int row = propertyRow(window, "Direction J");
    dvatest::check(row >= 0, "tolerance properties include direction J");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance direction J value item exists");
    const QString initialText = value->text();

    value->setText("not-a-direction");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid tolerance direction J edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const Vec3& updatedDirection =
        model.parts.front().tolerances.front().ir.direction.ijk;
    dvatest::checkNear(updatedDirection.y, direction.y, 1e-12,
                       "invalid tolerance direction J edit leaves J unchanged");
    dvatest::checkNear(updatedDirection.z, direction.z, 1e-12,
                       "invalid tolerance direction J edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction J");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after invalid direction J edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance direction J value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance direction J edit restores visible value");
}

TEST("main window tolerance direction K edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Direction K");
    dvatest::check(row >= 0, "tolerance properties include direction K");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance direction K value item exists");

    value->setText("-2.0");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const Vec3& direction =
        model.parts.front().tolerances.front().ir.direction.ijk;
    dvatest::checkNear(direction.z, -1.0, 1e-12,
                       "edited tolerance direction K is normalized");

    const int updatedRow = propertyRow(window, "Direction K");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after direction K edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance direction K value item exists");
    dvatest::checkNear(updated->text().toDouble(), -1.0, 1e-12,
                       "updated tolerance direction K shows normalized value");
}

TEST("main window invalid tolerance direction K edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    const Vec3 direction =
        initial.parts.front().tolerances.front().ir.direction.ijk;

    const int row = propertyRow(window, "Direction K");
    dvatest::check(row >= 0, "tolerance properties include direction K");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "tolerance direction K value item exists");
    const QString initialText = value->text();

    value->setText("not-a-direction");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid tolerance direction K edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const Vec3& updatedDirection =
        model.parts.front().tolerances.front().ir.direction.ijk;
    dvatest::checkNear(updatedDirection.x, direction.x, 1e-12,
                       "invalid tolerance direction K edit leaves I unchanged");
    dvatest::checkNear(updatedDirection.z, direction.z, 1e-12,
                       "invalid tolerance direction K edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction K");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after invalid direction K edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance direction K value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance direction K edit restores visible value");
}

TEST("main window tolerance truncation active edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Truncation Active");
    dvatest::check(row >= 0, "tolerance properties include truncation active");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance truncation active value item exists");

    value->setCheckState(Qt::Checked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::check(model.parts.front().tolerances.front().ir.truncation.active,
                   "edited tolerance truncation active flag is saved");

    const int updatedRow = propertyRow(window, "Truncation Active");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after truncation edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance truncation active value item exists");
    dvatest::check(updated->checkState() == Qt::Checked,
                   "updated tolerance truncation active remains checked");
}

TEST("main window tolerance min truncation edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Min Truncation");
    dvatest::check(row >= 0, "tolerance properties include min truncation");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance min truncation value item exists");

    value->setText("-0.25");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::checkNear(
        model.parts.front().tolerances.front().ir.truncation.minTrunc,
        -0.25, 1e-12,
        "edited tolerance min truncation is saved to the model");

    const int updatedRow = propertyRow(window, "Min Truncation");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after min truncation edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance min truncation value item exists");
    dvatest::check(updated->text().toStdString() == "-0.25",
                   "updated tolerance min truncation remains visible");
}

TEST("main window invalid tolerance min truncation edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    const Truncation truncation =
        initial.parts.front().tolerances.front().ir.truncation;

    const int row = propertyRow(window, "Min Truncation");
    dvatest::check(row >= 0, "tolerance properties include min truncation");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance min truncation value item exists");
    const QString initialText = value->text();

    value->setText("not-a-min-truncation");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid tolerance min truncation edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const Truncation& updatedTruncation =
        model.parts.front().tolerances.front().ir.truncation;
    dvatest::checkNear(updatedTruncation.minTrunc, truncation.minTrunc,
                       1e-12,
                       "invalid tolerance min truncation edit leaves min unchanged");
    dvatest::checkNear(updatedTruncation.maxTrunc, truncation.maxTrunc,
                       1e-12,
                       "invalid tolerance min truncation edit leaves max unchanged");
    dvatest::check(updatedTruncation.active == truncation.active,
                   "invalid tolerance min truncation edit leaves active unchanged");

    const int updatedRow = propertyRow(window, "Min Truncation");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after invalid min truncation edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance min truncation value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance min truncation edit restores visible value");
}

TEST("main window tolerance max truncation edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Max Truncation");
    dvatest::check(row >= 0, "tolerance properties include max truncation");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance max truncation value item exists");

    value->setText("0.35");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    dvatest::checkNear(
        model.parts.front().tolerances.front().ir.truncation.maxTrunc,
        0.35, 1e-12,
        "edited tolerance max truncation is saved to the model");

    const int updatedRow = propertyRow(window, "Max Truncation");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after max truncation edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance max truncation value item exists");
    dvatest::check(updated->text().toStdString() == "0.35",
                   "updated tolerance max truncation remains visible");
}

TEST("main window invalid tolerance max truncation edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    const Truncation truncation =
        initial.parts.front().tolerances.front().ir.truncation;

    const int row = propertyRow(window, "Max Truncation");
    dvatest::check(row >= 0, "tolerance properties include max truncation");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance max truncation value item exists");
    const QString initialText = value->text();

    value->setText("not-a-max-truncation");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid tolerance max truncation edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const Truncation& updatedTruncation =
        model.parts.front().tolerances.front().ir.truncation;
    dvatest::checkNear(updatedTruncation.minTrunc, truncation.minTrunc,
                       1e-12,
                       "invalid tolerance max truncation edit leaves min unchanged");
    dvatest::checkNear(updatedTruncation.maxTrunc, truncation.maxTrunc,
                       1e-12,
                       "invalid tolerance max truncation edit leaves max unchanged");
    dvatest::check(updatedTruncation.active == truncation.active,
                   "invalid tolerance max truncation edit leaves active unchanged");

    const int updatedRow = propertyRow(window, "Max Truncation");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after invalid max truncation edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance max truncation value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance max truncation edit restores visible value");
}

TEST("main window tolerance target features edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().features.empty(),
                   "starter UI model has features");
    const FeatureId targetFeature = initial.parts.front().features.front().id;
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Target Features");
    dvatest::check(row >= 0, "tolerance properties include target features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance target features value item exists");

    value->setText(QString::number(targetFeature));

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const std::vector<FeatureId>& features =
        model.parts.front().tolerances.front().features;
    dvatest::check(features.size() == 1 && features.front() == targetFeature,
                   "edited tolerance target feature is saved to the model");

    const int updatedRow = propertyRow(window, "Target Features");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after target feature edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance target features value item exists");
    dvatest::check(updated->text() == QString::number(targetFeature),
                   "updated tolerance target feature remains visible");
}

TEST("main window tolerance target features edit keeps selected feature visible") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(initial.parts.front().features.size() >= 2,
                   "starter UI model has two features");
    const FeatureId targetFeature = initial.parts.front().features[1].id;
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Target Features");
    dvatest::check(row >= 0, "tolerance properties include target features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance target features value item exists");

    const QString targetText = QString::number(targetFeature);
    value->setText(targetText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const std::vector<FeatureId>& features =
        model.parts.front().tolerances.front().features;
    dvatest::check(features.size() == 1 && features.front() == targetFeature,
                   "edited tolerance selected target feature is saved");

    const int updatedRow = propertyRow(window, "Target Features");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after target feature-list edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance target features value item exists");
    dvatest::check(updated->text() == targetText,
                   "updated tolerance selected target feature remains visible");
}

TEST("main window invalid tolerance target features edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    const std::vector<FeatureId> features =
        initial.parts.front().tolerances.front().features;
    QStringList initialParts;
    for (const FeatureId id : features) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Target Features");
    dvatest::check(row >= 0, "tolerance properties include target features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance target features value item exists");
    dvatest::check(value->text() == initialText,
                   "starter tolerance target features are visible");

    value->setText("not-a-feature-list");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid tolerance feature list",
                   "invalid tolerance target features edit reports feature-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const std::vector<FeatureId>& updatedFeatures =
        model.parts.front().tolerances.front().features;
    dvatest::check(updatedFeatures == features,
                   "invalid tolerance target features edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Target Features");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after invalid target features edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance target features value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance target features edit restores visible value");
}

TEST("main window unchanged tolerance target features edit refreshes tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    const std::vector<FeatureId> features =
        initial.parts.front().tolerances.front().features;
    QStringList initialParts;
    for (const FeatureId id : features) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Target Features");
    dvatest::check(row >= 0, "tolerance properties include target features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance target features value item exists");
    dvatest::check(value->text() == initialText,
                   "starter tolerance target features are visible");

    value->setText(initialText + ", ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const std::vector<FeatureId>& updatedFeatures =
        model.parts.front().tolerances.front().features;
    dvatest::check(updatedFeatures == features,
                   "unchanged tolerance target features edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Target Features");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after unchanged target features edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed tolerance target features value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged tolerance target features edit restores canonical value");
}

TEST("main window tolerance random variables edit keeps tolerance properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const int row = propertyRow(window, "Random Variables");
    dvatest::check(row >= 0, "tolerance properties include random variables");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance random variables value item exists");

    value->setText("Uniform, 0.8, 0.1, 4; Normal, 0.2, -0.05, 3");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const std::vector<RandSpec>& rands =
        model.parts.front().tolerances.front().ir.rands;
    dvatest::check(rands.size() == 2,
                   "edited tolerance random variables are saved to the model");
    dvatest::check(rands[0].distribution == DistributionType::Uniform,
                   "edited first tolerance random variable distribution is saved");
    dvatest::checkNear(rands[0].range, 0.8, 1e-12,
                       "edited first tolerance random variable range is saved");
    dvatest::checkNear(rands[0].offset, 0.1, 1e-12,
                       "edited first tolerance random variable offset is saved");
    dvatest::checkNear(rands[0].sigmaNum, 4.0, 1e-12,
                       "edited first tolerance random variable sigma is saved");
    dvatest::check(rands[1].distribution == DistributionType::Normal,
                   "edited second tolerance random variable distribution is saved");
    dvatest::checkNear(rands[1].range, 0.2, 1e-12,
                       "edited second tolerance random variable range is saved");
    dvatest::checkNear(rands[1].offset, -0.05, 1e-12,
                       "edited second tolerance random variable offset is saved");
    dvatest::checkNear(rands[1].sigmaNum, 3.0, 1e-12,
                       "edited second tolerance random variable sigma is saved");

    const int updatedRow = propertyRow(window, "Random Variables");
    dvatest::check(updatedRow >= 0,
                   "tolerance properties stay selected after random variables edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated tolerance random variables value item exists");
    dvatest::check(updated->text().toStdString() ==
                       "Uniform, 0.8, 0.1, 4; Normal, 0.2, -0.05, 3",
                   "updated tolerance random variables remain visible");
}

TEST("main window invalid tolerance random variables edit restores tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    const std::vector<RandSpec> rands =
        initial.parts.front().tolerances.front().ir.rands;

    const int row = propertyRow(window, "Random Variables");
    dvatest::check(row >= 0, "tolerance properties include random variables");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance random variables value item exists");
    const QString initialText = value->text();

    value->setText("Uniform, not-a-range, 0, 3");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid tolerance random variables",
                   "invalid tolerance random variables edit reports random-variable error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const std::vector<RandSpec>& updatedRands =
        model.parts.front().tolerances.front().ir.rands;
    dvatest::check(updatedRands.size() == rands.size(),
                   "invalid tolerance random variables edit keeps count");
    for (std::size_t i = 0; i < rands.size() && i < updatedRands.size(); ++i) {
        dvatest::check(updatedRands[i].distribution == rands[i].distribution,
                       "invalid tolerance random variables edit keeps distribution");
        dvatest::checkNear(updatedRands[i].range, rands[i].range, 1e-12,
                           "invalid tolerance random variables edit keeps range");
        dvatest::checkNear(updatedRands[i].offset, rands[i].offset, 1e-12,
                           "invalid tolerance random variables edit keeps offset");
        dvatest::checkNear(updatedRands[i].sigmaNum, rands[i].sigmaNum, 1e-12,
                           "invalid tolerance random variables edit keeps sigma");
    }

    const int updatedRow = propertyRow(window, "Random Variables");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after invalid random variables edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored tolerance random variables value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid tolerance random variables edit restores visible value");
}

TEST("main window unchanged tolerance random variables edit refreshes tolerance properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithEditableTolerance());
    dvatest::check(window.selectFirstToleranceForTesting(),
                   "starter UI model has a tolerance item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model has parts");
    dvatest::check(!initial.parts.front().tolerances.empty(),
                   "starter UI model has tolerances");
    const std::vector<RandSpec> rands =
        initial.parts.front().tolerances.front().ir.rands;

    const int row = propertyRow(window, "Random Variables");
    dvatest::check(row >= 0, "tolerance properties include random variables");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "tolerance random variables value item exists");
    const QString initialText = value->text();

    value->setText(initialText + "; ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.parts.empty(), "edited UI model still has parts");
    dvatest::check(!model.parts.front().tolerances.empty(),
                   "edited UI model still has tolerances");
    const std::vector<RandSpec>& updatedRands =
        model.parts.front().tolerances.front().ir.rands;
    dvatest::check(updatedRands.size() == rands.size(),
                   "unchanged tolerance random variables edit keeps count");
    for (std::size_t i = 0; i < rands.size() && i < updatedRands.size(); ++i) {
        dvatest::check(updatedRands[i].distribution == rands[i].distribution,
                       "unchanged tolerance random variables edit keeps distribution");
        dvatest::checkNear(updatedRands[i].range, rands[i].range, 1e-12,
                           "unchanged tolerance random variables edit keeps range");
        dvatest::checkNear(updatedRands[i].offset, rands[i].offset, 1e-12,
                           "unchanged tolerance random variables edit keeps offset");
        dvatest::checkNear(updatedRands[i].sigmaNum, rands[i].sigmaNum, 1e-12,
                           "unchanged tolerance random variables edit keeps sigma");
    }

    const int updatedRow = propertyRow(window, "Random Variables");
    dvatest::check(
        updatedRow >= 0,
        "tolerance properties stay selected after unchanged random variables edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed tolerance random variables value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged tolerance random variables edit restores canonical value");
}

TEST("main window move properties expose editable user-dll routine") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithUserDllMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "User DLL Routine");
    dvatest::check(row >= 0, "move properties include user-dll routine");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "user-dll routine value item exists");
    dvatest::check(value->text().toStdString() == "externalMove",
                   "user-dll routine value is shown");
    dvatest::check((value->flags() & Qt::ItemIsEditable) != 0,
                   "user-dll routine value is editable");
}

TEST("main window move name edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "move properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move name value item exists");

    value->setText("Panel Transform");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().name == "Panel Transform",
                   "edited move name is saved to the model");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated move name value item exists");
    dvatest::check(updated->text().toStdString() == "Panel Transform",
                   "updated move name remains visible");
}

TEST("main window unchanged move name edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model has moves");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "move properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move name value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(QString::fromStdString(model.moves.front().name) ==
                       initialText,
                   "unchanged move name edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move name value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move name edit restores canonical value");
}

TEST("main window move routine edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithUserDllMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "User DLL Routine");
    dvatest::check(row >= 0, "move properties include user-dll routine");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "user-dll routine value item exists");

    value->setText("editedMoveRoutine");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.userDllRoutine ==
                       "editedMoveRoutine",
                   "edited user-dll routine is saved to the model");

    const int updatedRow = propertyRow(window, "User DLL Routine");
    dvatest::check(updatedRow >= 0, "move properties stay selected after edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated routine value item exists");
    dvatest::check(updated->text().toStdString() == "editedMoveRoutine",
                   "updated routine value remains visible");
}

TEST("main window unchanged move routine edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithUserDllMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model has moves");

    const int row = propertyRow(window, "User DLL Routine");
    dvatest::check(row >= 0, "move properties include user-dll routine");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "user-dll routine value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(
        QString::fromStdString(model.moves.front().inputs.userDllRoutine) ==
            initialText,
        "unchanged move routine edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "User DLL Routine");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after unchanged routine edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed user-dll routine value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move routine edit restores canonical value");
}

TEST("main window move active edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Active");
    dvatest::check(row >= 0, "move properties include active");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move active value item exists");

    value->setCheckState(Qt::Checked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().active,
                   "edited move active flag is saved to the model");

    const int updatedRow = propertyRow(window, "Active");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after active edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move active value item exists");
    dvatest::check(updated->checkState() == Qt::Checked,
                   "updated move active remains checked");
}

TEST("main window move nominal build edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Nominal Build");
    dvatest::check(row >= 0, "move properties include nominal build");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move nominal build value item exists");

    value->setCheckState(Qt::Checked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.isNominalBuild,
                   "edited move nominal build flag is saved to the model");

    const int updatedRow = propertyRow(window, "Nominal Build");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after nominal build edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move nominal build value item exists");
    dvatest::check(updated->checkState() == Qt::Checked,
                   "updated move nominal build remains checked");
}

TEST("main window move search accuracy edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Search Accuracy");
    dvatest::check(row >= 0, "move properties include search accuracy");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move search accuracy value item exists");

    value->setText("0.0025");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.searchAccuracy, 0.0025,
                       1e-12,
                       "edited move search accuracy is saved to the model");

    const int updatedRow = propertyRow(window, "Search Accuracy");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after search accuracy edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move search accuracy value item exists");
    dvatest::check(updated->text().toStdString() == "0.0025",
                   "updated move search accuracy remains visible");
}

TEST("main window unchanged move search accuracy edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Search Accuracy");
    dvatest::check(row >= 0, "move properties include search accuracy");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move search accuracy value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.searchAccuracy, initialValue,
                       1e-12,
                       "unchanged move search accuracy edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Search Accuracy");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after unchanged search accuracy edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move search accuracy value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move search accuracy edit restores canonical value");
}

TEST("main window invalid move search accuracy edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Search Accuracy");
    dvatest::check(row >= 0, "move properties include search accuracy");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move search accuracy value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move search accuracy edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.searchAccuracy, initialValue,
                       1e-12,
                       "invalid move search accuracy edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Search Accuracy");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after invalid search accuracy edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move search accuracy value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move search accuracy edit restores canonical value");
}

TEST("main window move max iterations edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Max Iterations");
    dvatest::check(row >= 0, "move properties include max iterations");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move max iterations value item exists");

    value->setText("25");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.maxIterations == 25,
                   "edited move max iterations is saved to the model");

    const int updatedRow = propertyRow(window, "Max Iterations");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after max iterations edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move max iterations value item exists");
    dvatest::check(updated->text().toStdString() == "25",
                   "updated move max iterations remains visible");
}

TEST("main window unchanged move max iterations edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Max Iterations");
    dvatest::check(row >= 0, "move properties include max iterations");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move max iterations value item exists");
    const QString initialText = value->text();
    const int initialValue = initialText.toInt();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.maxIterations == initialValue,
                   "unchanged move max iterations edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Max Iterations");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after unchanged max iterations edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move max iterations value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move max iterations edit restores canonical value");
}

TEST("main window invalid move max iterations edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Max Iterations");
    dvatest::check(row >= 0, "move properties include max iterations");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move max iterations value item exists");
    const QString initialText = value->text();
    const int initialValue = initialText.toInt();

    value->setText("not-an-integer");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move max iterations edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.maxIterations == initialValue,
                   "invalid move max iterations edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Max Iterations");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after invalid max iterations edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move max iterations value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move max iterations edit restores canonical value");
}

TEST("main window move float active edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Active");
    dvatest::check(row >= 0, "move properties include float active");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move float active value item exists");

    value->setCheckState(Qt::Checked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.hole_pin_float.active,
                   "edited move float active flag is saved to the model");

    const int updatedRow = propertyRow(window, "Float Active");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after float active edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move float active value item exists");
    dvatest::check(updated->checkState() == Qt::Checked,
                   "updated move float active remains checked");
}

TEST("main window move float sigma number edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Sigma Number");
    dvatest::check(row >= 0, "move properties include float sigma number");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float sigma number value item exists");

    value->setText("4");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.hole_pin_float.sigmaNumber == 4,
                   "edited move float sigma number is saved to the model");

    const int updatedRow = propertyRow(window, "Float Sigma Number");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after float sigma edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move float sigma number value item exists");
    dvatest::check(updated->text().toStdString() == "4",
                   "updated move float sigma number remains visible");
}

TEST("main window unchanged move float sigma number edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Sigma Number");
    dvatest::check(row >= 0, "move properties include float sigma number");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float sigma number value item exists");
    const QString initialText = value->text();
    const int initialValue = initialText.toInt();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.hole_pin_float.sigmaNumber ==
                       initialValue,
                   "unchanged move float sigma number edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Float Sigma Number");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after unchanged float sigma edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move float sigma number value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move float sigma number edit restores canonical value");
}

TEST("main window invalid move float sigma number edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Sigma Number");
    dvatest::check(row >= 0, "move properties include float sigma number");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float sigma number value item exists");
    const QString initialText = value->text();
    const int initialValue = initialText.toInt();

    value->setText("not-an-integer");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move float sigma number edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.hole_pin_float.sigmaNumber ==
                       initialValue,
                   "invalid move float sigma number edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Float Sigma Number");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after invalid float sigma edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move float sigma number value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move float sigma number edit restores canonical value");
}

TEST("main window move float range scale edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Range Scale");
    dvatest::check(row >= 0, "move properties include float range scale");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float range scale value item exists");

    value->setText("1.25");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.hole_pin_float.rangeScale,
                       1.25, 1e-12,
                       "edited move float range scale is saved to the model");

    const int updatedRow = propertyRow(window, "Float Range Scale");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after float range edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move float range scale value item exists");
    dvatest::check(updated->text().toStdString() == "1.25",
                   "updated move float range scale remains visible");
}

TEST("main window unchanged move float range scale edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Range Scale");
    dvatest::check(row >= 0, "move properties include float range scale");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float range scale value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.hole_pin_float.rangeScale,
                       initialValue, 1e-12,
                       "unchanged move float range scale edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Float Range Scale");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after unchanged float range edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move float range scale value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move float range scale edit restores canonical value");
}

TEST("main window invalid move float range scale edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Range Scale");
    dvatest::check(row >= 0, "move properties include float range scale");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float range scale value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move float range scale edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.hole_pin_float.rangeScale,
                       initialValue, 1e-12,
                       "invalid move float range scale edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Float Range Scale");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after invalid float range edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move float range scale value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move float range scale edit restores canonical value");
}

TEST("main window move float angle range edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Angle Range");
    dvatest::check(row >= 0, "move properties include float angle range");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float angle range value item exists");

    value->setText("180");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.hole_pin_float.angleRangeDeg,
                       180.0, 1e-12,
                       "edited move float angle range is saved to the model");

    const int updatedRow = propertyRow(window, "Float Angle Range");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after float angle edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move float angle range value item exists");
    dvatest::check(updated->text().toStdString() == "180",
                   "updated move float angle range remains visible");
}

TEST("main window unchanged move float angle range edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Angle Range");
    dvatest::check(row >= 0, "move properties include float angle range");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float angle range value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.hole_pin_float.angleRangeDeg,
                       initialValue, 1e-12,
                       "unchanged move float angle range edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Float Angle Range");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after unchanged float angle edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move float angle range value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move float angle range edit restores canonical value");
}

TEST("main window invalid move float angle range edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Angle Range");
    dvatest::check(row >= 0, "move properties include float angle range");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float angle range value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move float angle range edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.hole_pin_float.angleRangeDeg,
                       initialValue, 1e-12,
                       "invalid move float angle range edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Float Angle Range");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after invalid float angle edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move float angle range value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move float angle range edit restores canonical value");
}

TEST("main window move float angle offset edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Angle Offset");
    dvatest::check(row >= 0, "move properties include float angle offset");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float angle offset value item exists");

    value->setText("-15");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.hole_pin_float.angleOffsetDeg,
                       -15.0, 1e-12,
                       "edited move float angle offset is saved to the model");

    const int updatedRow = propertyRow(window, "Float Angle Offset");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after float angle offset edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move float angle offset value item exists");
    dvatest::check(updated->text().toStdString() == "-15",
                   "updated move float angle offset remains visible");
}

TEST("main window unchanged move float angle offset edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Angle Offset");
    dvatest::check(row >= 0, "move properties include float angle offset");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float angle offset value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.hole_pin_float.angleOffsetDeg,
                       initialValue, 1e-12,
                       "unchanged move float angle offset edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Float Angle Offset");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after unchanged float angle offset edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move float angle offset value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move float angle offset edit restores canonical value");
}

TEST("main window invalid move float angle offset edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Float Angle Offset");
    dvatest::check(row >= 0, "move properties include float angle offset");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "move float angle offset value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move float angle offset edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::checkNear(model.moves.front().inputs.hole_pin_float.angleOffsetDeg,
                       initialValue, 1e-12,
                       "invalid move float angle offset edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Float Angle Offset");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after invalid float angle offset edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move float angle offset value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move float angle offset edit restores canonical value");
}

TEST("main window move pairs edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Pairs");
    dvatest::check(row >= 0, "move properties include pairs");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move pairs value item exists");

    value->setText("1, 2, 3 -> 4, 5, 6 -> 0, 3, 4");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.pairs.size() == 1,
                   "edited move keeps one pair");
    const MovePair& pair = model.moves.front().inputs.pairs.front();
    dvatest::checkNear(pair.objectPoint.x, 1.0, 1e-12,
                       "edited move pair object X is saved");
    dvatest::checkNear(pair.targetPoint.z, 6.0, 1e-12,
                       "edited move pair target Z is saved");
    dvatest::checkNear(pair.direction.ijk.y, 0.6, 1e-12,
                       "edited move pair direction J is normalized");
    dvatest::checkNear(pair.direction.ijk.z, 0.8, 1e-12,
                       "edited move pair direction K is normalized");

    const int updatedRow = propertyRow(window, "Pairs");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after pairs edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated move pairs value item exists");
    dvatest::check(updated->text().toStdString() ==
                       "1, 2, 3 -> 4, 5, 6 -> 0, 0.6, 0.8",
                   "updated move pairs show normalized direction");
}

TEST("main window invalid move pairs edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    const std::vector<MovePair> pairs = initial.moves.front().inputs.pairs;
    dvatest::check(!pairs.empty(), "starter move has pairs");
    const MovePair firstPair = pairs.front();

    const int row = propertyRow(window, "Pairs");
    dvatest::check(row >= 0, "move properties include pairs");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move pairs value item exists");
    const QString initialText = value->text();
    dvatest::check(!initialText.isEmpty(), "starter move pairs are visible");

    value->setText("not-a-move-pair");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid move pair list",
                   "invalid move pairs edit reports pair-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    const std::vector<MovePair>& updatedPairs =
        model.moves.front().inputs.pairs;
    dvatest::check(updatedPairs.size() == pairs.size(),
                   "invalid move pairs edit leaves pair count unchanged");
    dvatest::check(!updatedPairs.empty(), "edited move still has pairs");
    const MovePair& updatedPair = updatedPairs.front();
    dvatest::checkNear(updatedPair.objectPoint.x, firstPair.objectPoint.x,
                       1e-12,
                       "invalid move pairs edit keeps object X unchanged");
    dvatest::checkNear(updatedPair.targetPoint.z, firstPair.targetPoint.z,
                       1e-12,
                       "invalid move pairs edit keeps target Z unchanged");
    dvatest::checkNear(updatedPair.direction.ijk.y, firstPair.direction.ijk.y,
                       1e-12,
                       "invalid move pairs edit keeps direction J unchanged");

    const int updatedRow = propertyRow(window, "Pairs");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid pairs edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "restored move pairs value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move pairs edit restores visible value");
}

TEST("main window unchanged move pairs edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    const std::vector<MovePair> pairs = initial.moves.front().inputs.pairs;
    dvatest::check(!pairs.empty(), "starter move has pairs");
    const MovePair firstPair = pairs.front();

    const int row = propertyRow(window, "Pairs");
    dvatest::check(row >= 0, "move properties include pairs");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move pairs value item exists");
    const QString initialText = value->text();
    dvatest::check(!initialText.isEmpty(), "starter move pairs are visible");

    const QString equivalentText =
        QString(" %1 , %2 , %3 -> %4 , %5 , %6 -> %7 , %8 , %9 ")
            .arg(firstPair.objectPoint.x, 0, 'g', 17)
            .arg(firstPair.objectPoint.y, 0, 'g', 17)
            .arg(firstPair.objectPoint.z, 0, 'g', 17)
            .arg(firstPair.targetPoint.x, 0, 'g', 17)
            .arg(firstPair.targetPoint.y, 0, 'g', 17)
            .arg(firstPair.targetPoint.z, 0, 'g', 17)
            .arg(firstPair.direction.ijk.x * 2.0, 0, 'g', 17)
            .arg(firstPair.direction.ijk.y * 2.0, 0, 'g', 17)
            .arg(firstPair.direction.ijk.z * 2.0, 0, 'g', 17);
    dvatest::check(equivalentText != initialText,
                   "unchanged move pairs edit uses non-canonical text");

    value->setText(equivalentText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    const std::vector<MovePair>& updatedPairs =
        model.moves.front().inputs.pairs;
    dvatest::check(updatedPairs.size() == pairs.size(),
                   "unchanged move pairs edit keeps pair count");
    dvatest::check(!updatedPairs.empty(), "edited move still has pairs");
    const MovePair& updatedPair = updatedPairs.front();
    dvatest::checkNear(updatedPair.objectPoint.x, firstPair.objectPoint.x,
                       1e-12,
                       "unchanged move pairs edit keeps object X");
    dvatest::checkNear(updatedPair.targetPoint.z, firstPair.targetPoint.z,
                       1e-12,
                       "unchanged move pairs edit keeps target Z");
    dvatest::checkNear(updatedPair.direction.ijk.z, firstPair.direction.ijk.z,
                       1e-12,
                       "unchanged move pairs edit keeps direction K");

    const int updatedRow = propertyRow(window, "Pairs");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged pairs edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated move pairs value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move pairs edit restores canonical value");
}

TEST("main window move translation edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Translation X");
    dvatest::check(row >= 0, "move properties include translation X");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move translation X value item exists");

    value->setText("4.5");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.x,
                       4.5, 1e-12,
                       "edited transform move translation X is saved");

    const int updatedRow = propertyRow(window, "Translation X");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after translation edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move translation X value item exists");
    dvatest::check(updated->text().toStdString() == "4.5",
                   "updated move translation X remains visible");
}

TEST("main window invalid move translation edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTranslationX =
        initial.moves.front().inputs.pairs.front().targetPoint.x;

    const int row = propertyRow(window, "Translation X");
    dvatest::check(row >= 0, "move properties include translation X");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move translation X value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move translation X edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.x,
                       initialTranslationX, 1e-12,
                       "invalid move translation X edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Translation X");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid translation X edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move translation X value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move translation X edit restores visible value");
}

TEST("main window unchanged move translation edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTranslationX =
        initial.moves.front().inputs.pairs.front().targetPoint.x;

    const int row = propertyRow(window, "Translation X");
    dvatest::check(row >= 0, "move properties include translation X");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move translation X value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialTranslationX, 1e-12,
                       "visible translation X matches model");

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.x,
                       initialTranslationX, 1e-12,
                       "unchanged move translation X edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Translation X");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after unchanged translation X edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move translation X value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move translation X edit restores canonical value");
}

TEST("main window move translation Y edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Translation Y");
    dvatest::check(row >= 0, "move properties include translation Y");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move translation Y value item exists");

    value->setText("-3.5");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.y,
                       -3.5, 1e-12,
                       "edited transform move translation Y is saved");

    const int updatedRow = propertyRow(window, "Translation Y");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after translation Y edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move translation Y value item exists");
    dvatest::check(updated->text().toStdString() == "-3.5",
                   "updated move translation Y remains visible");
}

TEST("main window invalid move translation Y edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTranslationY =
        initial.moves.front().inputs.pairs.front().targetPoint.y;

    const int row = propertyRow(window, "Translation Y");
    dvatest::check(row >= 0, "move properties include translation Y");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move translation Y value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move translation Y edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.y,
                       initialTranslationY, 1e-12,
                       "invalid move translation Y edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Translation Y");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid translation Y edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move translation Y value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move translation Y edit restores visible value");
}

TEST("main window unchanged move translation Y edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTranslationY =
        initial.moves.front().inputs.pairs.front().targetPoint.y;

    const int row = propertyRow(window, "Translation Y");
    dvatest::check(row >= 0, "move properties include translation Y");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move translation Y value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialTranslationY, 1e-12,
                       "visible translation Y matches model");

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.y,
                       initialTranslationY, 1e-12,
                       "unchanged move translation Y edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Translation Y");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after unchanged translation Y edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move translation Y value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move translation Y edit restores canonical value");
}

TEST("main window move translation Z edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Translation Z");
    dvatest::check(row >= 0, "move properties include translation Z");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move translation Z value item exists");

    value->setText("2.75");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.z,
                       2.75, 1e-12,
                       "edited transform move translation Z is saved");

    const int updatedRow = propertyRow(window, "Translation Z");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after translation Z edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move translation Z value item exists");
    dvatest::check(updated->text().toStdString() == "2.75",
                   "updated move translation Z remains visible");
}

TEST("main window invalid move translation Z edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTranslationZ =
        initial.moves.front().inputs.pairs.front().targetPoint.z;

    const int row = propertyRow(window, "Translation Z");
    dvatest::check(row >= 0, "move properties include translation Z");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move translation Z value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move translation Z edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.z,
                       initialTranslationZ, 1e-12,
                       "invalid move translation Z edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Translation Z");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid translation Z edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move translation Z value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move translation Z edit restores visible value");
}

TEST("main window unchanged move translation Z edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTranslationZ =
        initial.moves.front().inputs.pairs.front().targetPoint.z;

    const int row = propertyRow(window, "Translation Z");
    dvatest::check(row >= 0, "move properties include translation Z");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move translation Z value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialTranslationZ, 1e-12,
                       "visible translation Z matches model");

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.z,
                       initialTranslationZ, 1e-12,
                       "unchanged move translation Z edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Translation Z");
    dvatest::check(
        updatedRow >= 0,
        "move properties stay selected after unchanged translation Z edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move translation Z value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move translation Z edit restores canonical value");
}

TEST("main window move object point edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Object X");
    dvatest::check(row >= 0, "move properties include object X");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move object X value item exists");

    value->setText("6.25");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().objectPoint.x,
                       6.25, 1e-12,
                       "edited move object point X is saved");

    const int updatedRow = propertyRow(window, "Object X");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after object point edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move object X value item exists");
    dvatest::check(updated->text().toStdString() == "6.25",
                   "updated move object X remains visible");
}

TEST("main window invalid move object point edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialObjectX =
        initial.moves.front().inputs.pairs.front().objectPoint.x;

    const int row = propertyRow(window, "Object X");
    dvatest::check(row >= 0, "move properties include object X");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move object X value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move object X edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().objectPoint.x,
                       initialObjectX, 1e-12,
                       "invalid move object X edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Object X");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid object X edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move object X value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move object X edit restores visible value");
}

TEST("main window unchanged move object point edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialObjectX =
        initial.moves.front().inputs.pairs.front().objectPoint.x;

    const int row = propertyRow(window, "Object X");
    dvatest::check(row >= 0, "move properties include object X");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move object X value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialObjectX, 1e-12,
                       "visible object X matches model");

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().objectPoint.x,
                       initialObjectX, 1e-12,
                       "unchanged move object X edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Object X");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged object X edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move object X value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move object X edit restores canonical value");
}

TEST("main window move object point Y edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Object Y");
    dvatest::check(row >= 0, "move properties include object Y");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move object Y value item exists");

    value->setText("7.5");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().objectPoint.y,
                       7.5, 1e-12,
                       "edited move object point Y is saved");

    const int updatedRow = propertyRow(window, "Object Y");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after object point Y edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move object Y value item exists");
    dvatest::check(updated->text().toStdString() == "7.5",
                   "updated move object Y remains visible");
}

TEST("main window invalid move object point Y edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialObjectY =
        initial.moves.front().inputs.pairs.front().objectPoint.y;

    const int row = propertyRow(window, "Object Y");
    dvatest::check(row >= 0, "move properties include object Y");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move object Y value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move object Y edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().objectPoint.y,
                       initialObjectY, 1e-12,
                       "invalid move object Y edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Object Y");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid object Y edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move object Y value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move object Y edit restores visible value");
}

TEST("main window unchanged move object point Y edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialObjectY =
        initial.moves.front().inputs.pairs.front().objectPoint.y;

    const int row = propertyRow(window, "Object Y");
    dvatest::check(row >= 0, "move properties include object Y");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move object Y value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialObjectY, 1e-12,
                       "visible object Y matches model");

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().objectPoint.y,
                       initialObjectY, 1e-12,
                       "unchanged move object Y edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Object Y");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged object Y edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move object Y value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move object Y edit restores canonical value");
}

TEST("main window move object point Z edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Object Z");
    dvatest::check(row >= 0, "move properties include object Z");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move object Z value item exists");

    value->setText("-4.25");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().objectPoint.z,
                       -4.25, 1e-12,
                       "edited move object point Z is saved");

    const int updatedRow = propertyRow(window, "Object Z");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after object point Z edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move object Z value item exists");
    dvatest::check(updated->text().toStdString() == "-4.25",
                   "updated move object Z remains visible");
}

TEST("main window invalid move object point Z edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialObjectZ =
        initial.moves.front().inputs.pairs.front().objectPoint.z;

    const int row = propertyRow(window, "Object Z");
    dvatest::check(row >= 0, "move properties include object Z");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move object Z value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move object Z edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().objectPoint.z,
                       initialObjectZ, 1e-12,
                       "invalid move object Z edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Object Z");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid object Z edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move object Z value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move object Z edit restores visible value");
}

TEST("main window unchanged move object point Z edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialObjectZ =
        initial.moves.front().inputs.pairs.front().objectPoint.z;

    const int row = propertyRow(window, "Object Z");
    dvatest::check(row >= 0, "move properties include object Z");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move object Z value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialObjectZ, 1e-12,
                       "visible object Z matches model");

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().objectPoint.z,
                       initialObjectZ, 1e-12,
                       "unchanged move object Z edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Object Z");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged object Z edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move object Z value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move object Z edit restores canonical value");
}

TEST("main window move target point edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Target X");
    dvatest::check(row >= 0, "move properties include target X");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move target X value item exists");

    value->setText("8.75");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.x,
                       8.75, 1e-12,
                       "edited move target point X is saved");

    const int updatedRow = propertyRow(window, "Target X");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after target point edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move target X value item exists");
    dvatest::check(updated->text().toStdString() == "8.75",
                   "updated move target X remains visible");
}

TEST("main window invalid move target point edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTargetX =
        initial.moves.front().inputs.pairs.front().targetPoint.x;

    const int row = propertyRow(window, "Target X");
    dvatest::check(row >= 0, "move properties include target X");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move target X value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move target X edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.x,
                       initialTargetX, 1e-12,
                       "invalid move target X edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Target X");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid target X edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move target X value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move target X edit restores visible value");
}

TEST("main window unchanged move target point edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTargetX =
        initial.moves.front().inputs.pairs.front().targetPoint.x;

    const int row = propertyRow(window, "Target X");
    dvatest::check(row >= 0, "move properties include target X");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move target X value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialTargetX, 1e-12,
                       "visible target X matches model");

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.x,
                       initialTargetX, 1e-12,
                       "unchanged move target X edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Target X");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged target X edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move target X value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move target X edit restores canonical value");
}

TEST("main window move target point Y edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Target Y");
    dvatest::check(row >= 0, "move properties include target Y");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move target Y value item exists");

    value->setText("9.125");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.y,
                       9.125, 1e-12,
                       "edited move target point Y is saved");

    const int updatedRow = propertyRow(window, "Target Y");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after target point Y edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move target Y value item exists");
    dvatest::check(updated->text().toStdString() == "9.125",
                   "updated move target Y remains visible");
}

TEST("main window invalid move target point Y edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTargetY =
        initial.moves.front().inputs.pairs.front().targetPoint.y;

    const int row = propertyRow(window, "Target Y");
    dvatest::check(row >= 0, "move properties include target Y");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move target Y value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move target Y edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.y,
                       initialTargetY, 1e-12,
                       "invalid move target Y edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Target Y");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid target Y edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move target Y value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move target Y edit restores visible value");
}

TEST("main window unchanged move target point Y edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTargetY =
        initial.moves.front().inputs.pairs.front().targetPoint.y;

    const int row = propertyRow(window, "Target Y");
    dvatest::check(row >= 0, "move properties include target Y");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move target Y value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialTargetY, 1e-12,
                       "visible target Y matches model");

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.y,
                       initialTargetY, 1e-12,
                       "unchanged move target Y edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Target Y");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged target Y edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move target Y value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move target Y edit restores canonical value");
}

TEST("main window move target point Z edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Target Z");
    dvatest::check(row >= 0, "move properties include target Z");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move target Z value item exists");

    value->setText("-6.5");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.z,
                       -6.5, 1e-12,
                       "edited move target point Z is saved");

    const int updatedRow = propertyRow(window, "Target Z");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after target point Z edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move target Z value item exists");
    dvatest::check(updated->text().toStdString() == "-6.5",
                   "updated move target Z remains visible");
}

TEST("main window invalid move target point Z edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTargetZ =
        initial.moves.front().inputs.pairs.front().targetPoint.z;

    const int row = propertyRow(window, "Target Z");
    dvatest::check(row >= 0, "move properties include target Z");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move target Z value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move target Z edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.z,
                       initialTargetZ, 1e-12,
                       "invalid move target Z edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Target Z");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid target Z edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move target Z value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move target Z edit restores visible value");
}

TEST("main window unchanged move target point Z edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move has pairs");
    const double initialTargetZ =
        initial.moves.front().inputs.pairs.front().targetPoint.z;

    const int row = propertyRow(window, "Target Z");
    dvatest::check(row >= 0, "move properties include target Z");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move target Z value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialTargetZ, 1e-12,
                       "visible target Z matches model");

    value->setText(" " + QString::number(initialValue, 'f', 6) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    dvatest::checkNear(model.moves.front().inputs.pairs.front().targetPoint.z,
                       initialTargetZ, 1e-12,
                       "unchanged move target Z edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Target Z");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged target Z edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move target Z value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move target Z edit restores canonical value");
}

TEST("main window move parts edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    const std::vector<PartId>& moveParts = initial.moves.front().moveParts;
    dvatest::check(moveParts.size() == 2,
                   "starter transform move has two move parts");
    const PartId firstPart = moveParts[0];
    const PartId secondPart = moveParts[1];

    const int row = propertyRow(window, "Move Parts");
    dvatest::check(row >= 0, "move properties include move parts");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move parts value item exists");

    const QString reversedText =
        QString("%1, %2").arg(secondPart).arg(firstPart);
    value->setText(reversedText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    const std::vector<PartId>& updatedMoveParts = model.moves.front().moveParts;
    dvatest::check(updatedMoveParts.size() == 2,
                   "edited move keeps two move parts");
    dvatest::check(updatedMoveParts[0] == secondPart,
                   "edited move first part is saved");
    dvatest::check(updatedMoveParts[1] == firstPart,
                   "edited move second part is saved");

    const int updatedRow = propertyRow(window, "Move Parts");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after move parts edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated move parts value item exists");
    dvatest::check(updated->text() == reversedText,
                   "updated move parts remain visible");
}

TEST("main window invalid move parts edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    const std::vector<PartId> moveParts = initial.moves.front().moveParts;
    dvatest::check(moveParts.size() == 2,
                   "starter transform move has two move parts");
    const QString initialText =
        QString("%1, %2").arg(moveParts[0]).arg(moveParts[1]);

    const int row = propertyRow(window, "Move Parts");
    dvatest::check(row >= 0, "move properties include move parts");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move parts value item exists");
    dvatest::check(value->text() == initialText,
                   "starter move parts are visible");

    value->setText("not-a-part-list");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid move part list",
                   "invalid move parts edit reports part-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    const std::vector<PartId>& updatedMoveParts =
        model.moves.front().moveParts;
    dvatest::check(updatedMoveParts == moveParts,
                   "invalid move parts edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Move Parts");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid move parts edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "restored move parts value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move parts edit restores visible value");
}

TEST("main window unchanged move parts edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    const std::vector<PartId> moveParts = initial.moves.front().moveParts;
    dvatest::check(moveParts.size() == 2,
                   "starter transform move has two move parts");
    const QString initialText =
        QString("%1, %2").arg(moveParts[0]).arg(moveParts[1]);

    const int row = propertyRow(window, "Move Parts");
    dvatest::check(row >= 0, "move properties include move parts");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move parts value item exists");
    dvatest::check(value->text() == initialText,
                   "starter move parts are visible");

    const QString equivalentText =
        QString(" %1 , %2 ").arg(moveParts[0]).arg(moveParts[1]);
    dvatest::check(equivalentText != initialText,
                   "unchanged move parts edit uses non-canonical text");

    value->setText(equivalentText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    const std::vector<PartId>& updatedMoveParts =
        model.moves.front().moveParts;
    dvatest::check(updatedMoveParts == moveParts,
                   "unchanged move parts edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Move Parts");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged move parts edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated move parts value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move parts edit restores canonical value");
}

TEST("main window move direction edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Direction I");
    dvatest::check(row >= 0, "move properties include direction I");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move direction I value item exists");

    value->setText("4.0");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    const Vec3& direction =
        model.moves.front().inputs.pairs.front().direction.ijk;
    dvatest::checkNear(direction.x, 0.97218705984234055, 1e-12,
                       "edited move direction I is normalized");
    dvatest::checkNear(direction.z, 0.19487094073848929, 1e-12,
                       "edited move direction keeps previous K component");

    const int updatedRow = propertyRow(window, "Direction I");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after direction edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move direction I value item exists");
    dvatest::check(updated->text().toStdString() == "0.972187059842",
                   "updated move direction I shows normalized value");
}

TEST("main window invalid move direction edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move still has pairs");
    const Vec3 initialDirection =
        initial.moves.front().inputs.pairs.front().direction.ijk;

    const int row = propertyRow(window, "Direction I");
    dvatest::check(row >= 0, "move properties include direction I");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move direction I value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move direction I edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    const Vec3& direction =
        model.moves.front().inputs.pairs.front().direction.ijk;
    dvatest::checkNear(direction.x, initialDirection.x, 1e-12,
                       "invalid move direction I edit leaves I unchanged");
    dvatest::checkNear(direction.z, initialDirection.z, 1e-12,
                       "invalid move direction I edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction I");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid direction I edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move direction I value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move direction I edit restores visible value");
}

TEST("main window unchanged move direction edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move still has pairs");
    const Vec3 initialDirection =
        initial.moves.front().inputs.pairs.front().direction.ijk;

    const int row = propertyRow(window, "Direction I");
    dvatest::check(row >= 0, "move properties include direction I");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move direction I value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialDirection.x, 1e-12,
                       "visible direction I matches model");

    value->setText(" " + QString::number(initialDirection.x, 'g', 17) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    const Vec3& direction =
        model.moves.front().inputs.pairs.front().direction.ijk;
    dvatest::checkNear(direction.x, initialDirection.x, 1e-9,
                       "unchanged move direction I edit leaves I unchanged");
    dvatest::checkNear(direction.z, initialDirection.z, 1e-9,
                       "unchanged move direction I edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction I");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged direction I edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move direction I value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move direction I edit restores canonical value");
}

TEST("main window move direction J edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Direction J");
    dvatest::check(row >= 0, "move properties include direction J");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move direction J value item exists");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move still has pairs");
    const Vec3 initialDirection =
        initial.moves.front().inputs.pairs.front().direction.ijk;

    value->setText("5.0");

    const double length =
        std::sqrt(initialDirection.x * initialDirection.x + 25.0 +
                  initialDirection.z * initialDirection.z);
    const double expectedJ = 5.0 / length;
    const double expectedK = initialDirection.z / length;
    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    const Vec3& direction =
        model.moves.front().inputs.pairs.front().direction.ijk;
    dvatest::checkNear(direction.y, expectedJ, 1e-12,
                       "edited move direction J is normalized");
    dvatest::checkNear(direction.z, expectedK, 1e-12,
                       "edited move direction keeps previous K component");

    const int updatedRow = propertyRow(window, "Direction J");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after direction J edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move direction J value item exists");
    dvatest::checkNear(updated->text().toDouble(), expectedJ, 1e-12,
                       "updated move direction J shows normalized value");
}

TEST("main window invalid move direction J edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move still has pairs");
    const Vec3 initialDirection =
        initial.moves.front().inputs.pairs.front().direction.ijk;

    const int row = propertyRow(window, "Direction J");
    dvatest::check(row >= 0, "move properties include direction J");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move direction J value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move direction J edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    const Vec3& direction =
        model.moves.front().inputs.pairs.front().direction.ijk;
    dvatest::checkNear(direction.y, initialDirection.y, 1e-12,
                       "invalid move direction J edit leaves J unchanged");
    dvatest::checkNear(direction.z, initialDirection.z, 1e-12,
                       "invalid move direction J edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction J");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid direction J edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move direction J value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move direction J edit restores visible value");
}

TEST("main window unchanged move direction J edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move still has pairs");
    const Vec3 initialDirection =
        initial.moves.front().inputs.pairs.front().direction.ijk;

    const int row = propertyRow(window, "Direction J");
    dvatest::check(row >= 0, "move properties include direction J");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move direction J value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialDirection.y, 1e-12,
                       "visible direction J matches model");

    value->setText(" " + QString::number(initialDirection.y, 'g', 17) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    const Vec3& direction =
        model.moves.front().inputs.pairs.front().direction.ijk;
    dvatest::checkNear(direction.y, initialDirection.y, 1e-9,
                       "unchanged move direction J edit leaves J unchanged");
    dvatest::checkNear(direction.z, initialDirection.z, 1e-9,
                       "unchanged move direction J edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction J");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged direction J edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move direction J value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move direction J edit restores canonical value");
}

TEST("main window move direction K edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Direction K");
    dvatest::check(row >= 0, "move properties include direction K");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move direction K value item exists");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move still has pairs");
    const Vec3 initialDirection =
        initial.moves.front().inputs.pairs.front().direction.ijk;

    value->setText("-2.0");

    const double length =
        std::sqrt(initialDirection.x * initialDirection.x +
                  initialDirection.y * initialDirection.y + 4.0);
    const double expectedK = -2.0 / length;
    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    const Vec3& direction =
        model.moves.front().inputs.pairs.front().direction.ijk;
    dvatest::checkNear(direction.z, expectedK, 1e-12,
                       "edited move direction K is normalized");

    const int updatedRow = propertyRow(window, "Direction K");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after direction K edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated move direction K value item exists");
    dvatest::checkNear(updated->text().toDouble(), expectedK, 1e-12,
                       "updated move direction K shows normalized value");
}

TEST("main window invalid move direction K edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move still has pairs");
    const Vec3 initialDirection =
        initial.moves.front().inputs.pairs.front().direction.ijk;

    const int row = propertyRow(window, "Direction K");
    dvatest::check(row >= 0, "move properties include direction K");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move direction K value item exists");
    const QString initialText = value->text();

    value->setText("not-a-number");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid move direction K edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    const Vec3& direction =
        model.moves.front().inputs.pairs.front().direction.ijk;
    dvatest::checkNear(direction.x, initialDirection.x, 1e-12,
                       "invalid move direction K edit leaves I unchanged");
    dvatest::checkNear(direction.z, initialDirection.z, 1e-12,
                       "invalid move direction K edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction K");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid direction K edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move direction K value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid move direction K edit restores visible value");
}

TEST("main window unchanged move direction K edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.moves.empty(), "starter UI model still has moves");
    dvatest::check(!initial.moves.front().inputs.pairs.empty(),
                   "starter move still has pairs");
    const Vec3 initialDirection =
        initial.moves.front().inputs.pairs.front().direction.ijk;

    const int row = propertyRow(window, "Direction K");
    dvatest::check(row >= 0, "move properties include direction K");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move direction K value item exists");
    const QString initialText = value->text();
    const double initialValue = initialText.toDouble();
    dvatest::checkNear(initialValue, initialDirection.z, 1e-12,
                       "visible direction K matches model");

    value->setText(" " + QString::number(initialDirection.z, 'g', 17) + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(!model.moves.front().inputs.pairs.empty(),
                   "edited move still has pairs");
    const Vec3& direction =
        model.moves.front().inputs.pairs.front().direction.ijk;
    dvatest::checkNear(direction.x, initialDirection.x, 1e-9,
                       "unchanged move direction K edit leaves I unchanged");
    dvatest::checkNear(direction.z, initialDirection.z, 1e-9,
                       "unchanged move direction K edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction K");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged direction K edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed move direction K value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged move direction K edit restores canonical value");
}

TEST("main window move type edit keeps move properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Move Type");
    dvatest::check(row >= 0, "move properties include move type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move type value item exists");

    value->setText("User DLL");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.type == MoveType::UserDll,
                   "edited move type is saved to the model");

    const int updatedRow = propertyRow(window, "Move Type");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated move type value item exists");
    dvatest::check(updated->text().toStdString() == "User DLL",
                   "updated move type remains visible");
    dvatest::check(propertyRow(window, "User DLL Routine") >= 0,
                   "user-dll routine remains available after type edit");
}

TEST("main window invalid move type edit restores move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Move Type");
    dvatest::check(row >= 0, "move properties include move type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move type value item exists");
    dvatest::check(value->text().toStdString() == "Transform",
                   "starter move type is visible");

    value->setText("Not A Move Type");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid move type",
                   "invalid move type edit reports type error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.type == MoveType::Transform,
                   "invalid move type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Move Type");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after invalid type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored move type value item exists");
    dvatest::check(updated->text().toStdString() == "Transform",
                   "invalid move type edit restores visible value");
}

TEST("main window unchanged move type edit refreshes move properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveTransformMove());
    dvatest::check(window.selectFirstMoveForTesting(),
                   "starter UI model has a move item");

    const int row = propertyRow(window, "Move Type");
    dvatest::check(row >= 0, "move properties include move type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "move type value item exists");
    dvatest::check(value->text().toStdString() == "Transform",
                   "starter move type is visible");

    value->setText(" transform ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.moves.empty(), "edited UI model still has moves");
    dvatest::check(model.moves.front().inputs.type == MoveType::Transform,
                   "unchanged move type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Move Type");
    dvatest::check(updatedRow >= 0,
                   "move properties stay selected after unchanged type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated move type value item exists");
    dvatest::check(updated->text().toStdString() == "Transform",
                   "unchanged move type edit restores canonical value");
}

TEST("main window measure properties label user-dll equation as routine") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithUserDllMeasure());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "User DLL Routine");
    dvatest::check(row >= 0, "measure properties include user-dll routine");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "user-dll routine value item exists");
    dvatest::check(value->text().toStdString() == "externalMeasure",
                   "user-dll measure routine value is shown");
    dvatest::check((value->flags() & Qt::ItemIsEditable) != 0,
                   "user-dll measure routine value is editable");
}

TEST("main window measure user-dll routine edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithUserDllMeasure());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "User DLL Routine");
    dvatest::check(row >= 0, "measure properties include user-dll routine");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "user-dll routine value item exists");

    value->setText("alternateMeasure");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.equation == "alternateMeasure",
                   "edited user-dll measure routine is saved to the model");

    const int updatedRow = propertyRow(window, "User DLL Routine");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after routine edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated user-dll routine value item exists");
    dvatest::check(updated->text().toStdString() == "alternateMeasure",
                   "updated user-dll routine remains visible");
}

TEST("main window unchanged measure user-dll routine edit refreshes measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithUserDllMeasure());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model has measures");

    const int row = propertyRow(window, "User DLL Routine");
    dvatest::check(row >= 0, "measure properties include user-dll routine");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "user-dll routine value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(QString::fromStdString(model.measures.front().def.equation) ==
                       initialText,
                   "unchanged measure routine edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "User DLL Routine");
    dvatest::check(
        updatedRow >= 0,
        "measure properties stay selected after unchanged routine edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed user-dll routine value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged measure routine edit restores canonical value");
}

TEST("main window measure rename keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "measure properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure name value item exists");

    value->setText("Gap Flush");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().name == "Gap Flush",
                   "edited measure name is saved to the model");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after rename");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated measure name value item exists");
    dvatest::check(updated->text().toStdString() == "Gap Flush",
                   "updated measure name remains visible");
}

TEST("main window unchanged measure name edit refreshes measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(), "starter UI model has measures");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "measure properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure name value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(QString::fromStdString(model.measures.front().name) ==
                       initialText,
                   "unchanged measure name edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after unchanged name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed measure name value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged measure name edit restores canonical value");
}

TEST("main window measure scale edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Scale");
    dvatest::check(row >= 0, "measure properties include scale");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure scale value item exists");

    value->setText("2.5");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::checkNear(model.measures.front().def.scale, 2.5, 1e-12,
                       "edited measure scale is saved to the model");

    const int updatedRow = propertyRow(window, "Scale");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after scale edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated measure scale value item exists");
    dvatest::check(updated->text().toStdString() == "2.5",
                   "updated measure scale remains visible");
}

TEST("main window invalid measure scale edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const double scale = initial.measures.front().def.scale;

    const int row = propertyRow(window, "Scale");
    dvatest::check(row >= 0, "measure properties include scale");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure scale value item exists");
    const QString initialText = value->text();

    value->setText("not-a-scale");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid measure scale edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::checkNear(model.measures.front().def.scale, scale, 1e-12,
                       "invalid measure scale edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Scale");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after invalid scale edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure scale value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid measure scale edit restores visible value");
}

TEST("main window measure equation edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Equation");
    dvatest::check(row >= 0, "measure properties include equation");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure equation value item exists");

    value->setText("1 + 2");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.equation == "1 + 2",
                   "edited measure equation is saved to the model");

    const int updatedRow = propertyRow(window, "Equation");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after equation edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated measure equation value item exists");
    dvatest::check(updated->text().toStdString() == "1 + 2",
                   "updated measure equation remains visible");
}

TEST("main window unchanged measure equation edit refreshes measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model has measures");

    const int row = propertyRow(window, "Equation");
    dvatest::check(row >= 0, "measure properties include equation");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure equation value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(QString::fromStdString(model.measures.front().def.equation) ==
                       initialText,
                   "unchanged measure equation edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Equation");
    dvatest::check(
        updatedRow >= 0,
        "measure properties stay selected after unchanged equation edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed measure equation value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged measure equation edit restores canonical value");
}

TEST("main window measure values edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Values");
    dvatest::check(row >= 0, "measure properties include values");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure values value item exists");

    value->setText("3, 4.5");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.values.size() == 2,
                   "edited measure values are saved to the model");
    dvatest::checkNear(model.measures.front().def.values[0], 3.0, 1e-12,
                       "edited first measure value is saved");
    dvatest::checkNear(model.measures.front().def.values[1], 4.5, 1e-12,
                       "edited second measure value is saved");

    const int updatedRow = propertyRow(window, "Values");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after values edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated measure values value item exists");
    dvatest::check(updated->text().toStdString() == "3, 4.5",
                   "updated measure values remain visible");
}

TEST("main window invalid measure values edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const std::vector<double> values = initial.measures.front().def.values;

    const int row = propertyRow(window, "Values");
    dvatest::check(row >= 0, "measure properties include values");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure values value item exists");
    const QString initialText = value->text();

    value->setText("not-a-value-list");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid values list",
                   "invalid measure values edit reports values-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.values == values,
                   "invalid measure values edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Values");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after invalid values edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure values value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid measure values edit restores visible value");
}

TEST("main window unchanged measure values edit refreshes measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    int row = propertyRow(window, "Values");
    dvatest::check(row >= 0, "measure properties include values");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure values value item exists");

    value->setText("3, 4.5");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "edited UI model still has measures");
    const std::vector<double> values = initial.measures.front().def.values;
    dvatest::check(values.size() == 2, "edited measure has two values");

    row = propertyRow(window, "Values");
    dvatest::check(row >= 0, "measure properties still include values");
    value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "updated measure values value item exists");
    const QString initialText = value->text();
    dvatest::check(initialText == "3, 4.5",
                   "edited measure values are visible canonically");

    const QString equivalentText = " 3.0 , 4.500 ";
    dvatest::check(equivalentText != initialText,
                   "unchanged measure values edit uses non-canonical text");

    value->setText(equivalentText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const std::vector<double>& updatedValues =
        model.measures.front().def.values;
    dvatest::check(updatedValues.size() == values.size(),
                   "unchanged measure values edit keeps value count");
    for (std::size_t i = 0; i < values.size() && i < updatedValues.size();
         ++i) {
        dvatest::checkNear(updatedValues[i], values[i], 1e-12,
                           "unchanged measure values edit keeps value");
    }

    const int updatedRow = propertyRow(window, "Values");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after unchanged values edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed measure values value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged measure values edit restores canonical value");
}

TEST("main window measure input points edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const std::vector<PointId>& inputPoints =
        initial.measures.front().def.inputPoints;
    dvatest::check(inputPoints.size() == 2,
                   "starter measure has two input points");
    const PointId firstPoint = inputPoints[0];
    const PointId secondPoint = inputPoints[1];

    const int row = propertyRow(window, "Input Points");
    dvatest::check(row >= 0, "measure properties include input points");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure input points value item exists");

    const QString reversedText =
        QString("%1, %2").arg(secondPoint).arg(firstPoint);
    value->setText(reversedText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const std::vector<PointId>& updatedInputPoints =
        model.measures.front().def.inputPoints;
    dvatest::check(updatedInputPoints.size() == 2,
                   "edited measure keeps two input points");
    dvatest::check(updatedInputPoints[0] == secondPoint,
                   "edited measure first input point is saved");
    dvatest::check(updatedInputPoints[1] == firstPoint,
                   "edited measure second input point is saved");

    const int updatedRow = propertyRow(window, "Input Points");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after input points edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure input points value item exists");
    dvatest::check(updated->text() == reversedText,
                   "updated measure input points remain visible");
}

TEST("main window invalid measure input points edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const std::vector<PointId> inputPoints =
        initial.measures.front().def.inputPoints;
    dvatest::check(inputPoints.size() == 2,
                   "starter measure has two input points");
    const QString initialText =
        QString("%1, %2").arg(inputPoints[0]).arg(inputPoints[1]);

    const int row = propertyRow(window, "Input Points");
    dvatest::check(row >= 0, "measure properties include input points");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure input points value item exists");
    dvatest::check(value->text() == initialText,
                   "starter measure input points are visible");

    value->setText("not-a-point-list");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid input point list",
                   "invalid measure input points edit reports point-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const std::vector<PointId>& updatedInputPoints =
        model.measures.front().def.inputPoints;
    dvatest::check(updatedInputPoints == inputPoints,
                   "invalid measure input points edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Input Points");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after invalid input points edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure input points value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid measure input points edit restores visible value");
}

TEST("main window unchanged measure input points edit refreshes measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const std::vector<PointId> inputPoints =
        initial.measures.front().def.inputPoints;
    dvatest::check(inputPoints.size() == 2,
                   "starter measure has two input points");
    const QString initialText =
        QString("%1, %2").arg(inputPoints[0]).arg(inputPoints[1]);

    const int row = propertyRow(window, "Input Points");
    dvatest::check(row >= 0, "measure properties include input points");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure input points value item exists");
    dvatest::check(value->text() == initialText,
                   "starter measure input points are visible");

    const QString equivalentText =
        QString(" %1 , %2 ").arg(inputPoints[0]).arg(inputPoints[1]);
    dvatest::check(equivalentText != initialText,
                   "unchanged measure input points edit uses non-canonical text");

    value->setText(equivalentText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const std::vector<PointId>& updatedInputPoints =
        model.measures.front().def.inputPoints;
    dvatest::check(updatedInputPoints == inputPoints,
                   "unchanged measure input points edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Input Points");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after unchanged input points edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure input points value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged measure input points edit restores canonical value");
}

TEST("main window measure input features edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(), "starter UI model still has parts");
    dvatest::check(initial.parts.front().features.size() >= 2,
                   "starter part has at least two features");
    const FeatureId firstFeature = initial.parts.front().features[0].id;
    const FeatureId secondFeature = initial.parts.front().features[1].id;

    const int row = propertyRow(window, "Input Features");
    dvatest::check(row >= 0, "measure properties include input features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "measure input features value item exists");

    const QString featuresText =
        QString("%1, %2").arg(secondFeature).arg(firstFeature);
    value->setText(featuresText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const std::vector<FeatureId>& inputFeatures =
        model.measures.front().def.inputFeatures;
    dvatest::check(inputFeatures.size() == 2,
                   "edited measure keeps two input features");
    dvatest::check(inputFeatures[0] == secondFeature,
                   "edited measure first input feature is saved");
    dvatest::check(inputFeatures[1] == firstFeature,
                   "edited measure second input feature is saved");

    const int updatedRow = propertyRow(window, "Input Features");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after input features edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure input features value item exists");
    dvatest::check(updated->text() == featuresText,
                   "updated measure input features remain visible");
}

TEST("main window invalid measure input features edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const std::vector<FeatureId> inputFeatures =
        initial.measures.front().def.inputFeatures;
    QStringList initialParts;
    for (const FeatureId id : inputFeatures) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Input Features");
    dvatest::check(row >= 0, "measure properties include input features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "measure input features value item exists");
    dvatest::check(value->text() == initialText,
                   "starter measure input features are visible");

    value->setText("not-a-feature-list");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid input feature list",
                   "invalid measure input features edit reports feature-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const std::vector<FeatureId>& updatedInputFeatures =
        model.measures.front().def.inputFeatures;
    dvatest::check(updatedInputFeatures == inputFeatures,
                   "invalid measure input features edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Input Features");
    dvatest::check(
        updatedRow >= 0,
        "measure properties stay selected after invalid input features edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure input features value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid measure input features edit restores visible value");
}

TEST("main window unchanged measure input features edit refreshes measure properties") {
    ui::MainWindow window;
    Model editable = createStarterModel();
    dvatest::check(!editable.measures.empty(),
                   "starter UI model has measures");
    dvatest::check(!editable.parts.empty(), "starter UI model has parts");
    dvatest::check(editable.parts.front().features.size() >= 2,
                   "starter part has at least two features");
    const std::vector<FeatureId> inputFeatures = {
        editable.parts.front().features[0].id,
        editable.parts.front().features[1].id};
    dvatest::check(setMeasureInputFeatures(editable,
                                           editable.measures.front().id,
                                           inputFeatures),
                   "starter measure input features can be staged");
    window.setModelForTesting(editable);
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    dvatest::check(initial.measures.front().def.inputFeatures == inputFeatures,
                   "starter measure has input features");
    QStringList initialParts;
    for (const FeatureId id : inputFeatures) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Input Features");
    dvatest::check(row >= 0, "measure properties include input features");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "measure input features value item exists");
    dvatest::check(value->text() == initialText,
                   "starter measure input features are visible");

    QStringList equivalentParts;
    for (const FeatureId id : inputFeatures) {
        equivalentParts << QString(" %1 ").arg(id);
    }
    const QString equivalentText = equivalentParts.join(",");
    dvatest::check(equivalentText != initialText,
                   "unchanged measure input features edit uses non-canonical text");

    value->setText(equivalentText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const std::vector<FeatureId>& updatedInputFeatures =
        model.measures.front().def.inputFeatures;
    dvatest::check(updatedInputFeatures == inputFeatures,
                   "unchanged measure input features edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Input Features");
    dvatest::check(
        updatedRow >= 0,
        "measure properties stay selected after unchanged input features edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure input features value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged measure input features edit restores canonical value");
}

TEST("main window measure active edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Active");
    dvatest::check(row >= 0, "measure properties include active");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure active value item exists");

    value->setCheckState(Qt::Unchecked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(!model.measures.front().def.active,
                   "edited measure active flag is saved to the model");

    const int updatedRow = propertyRow(window, "Active");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after active edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure active value item exists");
    dvatest::check(updated->checkState() == Qt::Unchecked,
                   "updated measure active remains unchecked");
}

TEST("main window measure name edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "measure properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure name value item exists");

    value->setText("RenamedMeasure");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().name == "RenamedMeasure",
                   "edited measure name is saved to the model");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated measure name value item exists");
    dvatest::check(updated->text().toStdString() == "RenamedMeasure",
                   "updated measure name remains visible");
}

TEST("main window measure output edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Output");
    dvatest::check(row >= 0, "measure properties include output");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure output value item exists");

    value->setCheckState(Qt::Unchecked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(!model.measures.front().def.asOutput,
                   "edited measure output flag is saved to the model");

    const int updatedRow = propertyRow(window, "Output");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after output edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure output value item exists");
    dvatest::check(updated->checkState() == Qt::Unchecked,
                   "updated measure output remains unchecked");
}

TEST("main window measure spec mode edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Spec Mode");
    dvatest::check(row >= 0, "measure properties include spec mode");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure spec mode value item exists");

    value->setText("Relative To Nominal");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.spec.mode ==
                       SpecMode::RelativeToNominal,
                   "edited measure spec mode is saved to the model");

    const int updatedRow = propertyRow(window, "Spec Mode");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after spec mode edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure spec mode value item exists");
    dvatest::check(updated->text().toStdString() == "Relative To Nominal",
                   "updated measure spec mode remains visible");
}

TEST("main window invalid measure spec mode edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Spec Mode");
    dvatest::check(row >= 0, "measure properties include spec mode");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure spec mode value item exists");
    dvatest::check(value->text().toStdString() == "Absolute",
                   "starter measure spec mode is visible");

    value->setText("Not A Spec Mode");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid spec mode",
                   "invalid measure spec mode edit reports spec-mode error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.spec.mode == SpecMode::Absolute,
                   "invalid measure spec mode edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Spec Mode");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after invalid spec mode edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure spec mode value item exists");
    dvatest::check(updated->text().toStdString() == "Absolute",
                   "invalid measure spec mode edit restores visible value");
}

TEST("main window unchanged measure spec mode edit refreshes measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Spec Mode");
    dvatest::check(row >= 0, "measure properties include spec mode");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure spec mode value item exists");
    dvatest::check(value->text().toStdString() == "Absolute",
                   "starter measure spec mode is visible");

    value->setText(" absolute ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.spec.mode == SpecMode::Absolute,
                   "unchanged measure spec mode edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Spec Mode");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after unchanged spec mode edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure spec mode value item exists");
    dvatest::check(updated->text().toStdString() == "Absolute",
                   "unchanged measure spec mode edit restores canonical value");
}

TEST("main window measure direction mode edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Direction Mode");
    dvatest::check(row >= 0, "measure properties include direction mode");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "measure direction mode value item exists");

    value->setText("Projected On Plane");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.dirMode ==
                       DirectionMode::ProjectedOnPlane,
                   "edited measure direction mode is saved to the model");

    const int updatedRow = propertyRow(window, "Direction Mode");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after direction mode edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure direction mode value item exists");
    dvatest::check(updated->text().toStdString() == "Projected On Plane",
                   "updated measure direction mode remains visible");
}

TEST("main window invalid measure direction mode edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Direction Mode");
    dvatest::check(row >= 0, "measure properties include direction mode");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "measure direction mode value item exists");
    dvatest::check(value->text().toStdString() == "Projected On Vector",
                   "starter measure direction mode is visible");

    value->setText("Not A Direction Mode");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid direction mode",
                   "invalid measure direction mode edit reports direction-mode error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.dirMode ==
                       DirectionMode::ProjectedOnVector,
                   "invalid measure direction mode edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Direction Mode");
    dvatest::check(
        updatedRow >= 0,
        "measure properties stay selected after invalid direction mode edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure direction mode value item exists");
    dvatest::check(updated->text().toStdString() == "Projected On Vector",
                   "invalid measure direction mode edit restores visible value");
}

TEST("main window unchanged measure direction mode edit refreshes measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Direction Mode");
    dvatest::check(row >= 0, "measure properties include direction mode");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr,
                   "measure direction mode value item exists");
    dvatest::check(value->text().toStdString() == "Projected On Vector",
                   "starter measure direction mode is visible");

    value->setText(" projected-on-vector ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.dirMode ==
                       DirectionMode::ProjectedOnVector,
                   "unchanged measure direction mode edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Direction Mode");
    dvatest::check(
        updatedRow >= 0,
        "measure properties stay selected after unchanged direction mode edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure direction mode value item exists");
    dvatest::check(updated->text().toStdString() == "Projected On Vector",
                   "unchanged measure direction mode edit restores canonical value");
}

TEST("main window measure direction edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Direction I");
    dvatest::check(row >= 0, "measure properties include direction I");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure direction I value item exists");

    value->setText("3.0");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const Vec3& direction = model.measures.front().def.direction.ijk;
    dvatest::checkNear(direction.x, 0.9486832980505138, 1e-12,
                       "edited measure direction I is normalized");
    dvatest::checkNear(direction.z, 0.31622776601683794, 1e-12,
                       "edited measure direction keeps previous K component");

    const int updatedRow = propertyRow(window, "Direction I");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after direction edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure direction I value item exists");
    dvatest::check(updated->text().toStdString() == "0.948683298051",
                   "updated measure direction I shows normalized value");
}

TEST("main window invalid measure direction edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const Vec3 direction = initial.measures.front().def.direction.ijk;

    const int row = propertyRow(window, "Direction I");
    dvatest::check(row >= 0, "measure properties include direction I");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure direction I value item exists");
    const QString initialText = value->text();

    value->setText("not-a-direction");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid measure direction I edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const Vec3& updatedDirection = model.measures.front().def.direction.ijk;
    dvatest::checkNear(updatedDirection.x, direction.x, 1e-12,
                       "invalid measure direction I edit leaves I unchanged");
    dvatest::checkNear(updatedDirection.z, direction.z, 1e-12,
                       "invalid measure direction I edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction I");
    dvatest::check(
        updatedRow >= 0,
        "measure properties stay selected after invalid direction I edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure direction I value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid measure direction I edit restores visible value");
}

TEST("main window measure direction J edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Direction J");
    dvatest::check(row >= 0, "measure properties include direction J");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure direction J value item exists");

    value->setText("4.0");

    const double expectedJ = 4.0 / std::sqrt(17.0);
    const double expectedK = 1.0 / std::sqrt(17.0);
    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const Vec3& direction = model.measures.front().def.direction.ijk;
    dvatest::checkNear(direction.y, expectedJ, 1e-12,
                       "edited measure direction J is normalized");
    dvatest::checkNear(direction.z, expectedK, 1e-12,
                       "edited measure direction keeps previous K component");

    const int updatedRow = propertyRow(window, "Direction J");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after direction J edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure direction J value item exists");
    dvatest::checkNear(updated->text().toDouble(), expectedJ, 1e-12,
                       "updated measure direction J shows normalized value");
}

TEST("main window invalid measure direction J edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const Vec3 direction = initial.measures.front().def.direction.ijk;

    const int row = propertyRow(window, "Direction J");
    dvatest::check(row >= 0, "measure properties include direction J");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure direction J value item exists");
    const QString initialText = value->text();

    value->setText("not-a-direction");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid measure direction J edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const Vec3& updatedDirection = model.measures.front().def.direction.ijk;
    dvatest::checkNear(updatedDirection.y, direction.y, 1e-12,
                       "invalid measure direction J edit leaves J unchanged");
    dvatest::checkNear(updatedDirection.z, direction.z, 1e-12,
                       "invalid measure direction J edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction J");
    dvatest::check(
        updatedRow >= 0,
        "measure properties stay selected after invalid direction J edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure direction J value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid measure direction J edit restores visible value");
}

TEST("main window measure direction K edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Direction K");
    dvatest::check(row >= 0, "measure properties include direction K");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure direction K value item exists");

    value->setText("-2.0");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const Vec3& direction = model.measures.front().def.direction.ijk;
    dvatest::checkNear(direction.z, -1.0, 1e-12,
                       "edited measure direction K is normalized");

    const int updatedRow = propertyRow(window, "Direction K");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after direction K edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure direction K value item exists");
    dvatest::checkNear(updated->text().toDouble(), -1.0, 1e-12,
                       "updated measure direction K shows normalized value");
}

TEST("main window invalid measure direction K edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const Vec3 direction = initial.measures.front().def.direction.ijk;

    const int row = propertyRow(window, "Direction K");
    dvatest::check(row >= 0, "measure properties include direction K");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure direction K value item exists");
    const QString initialText = value->text();

    value->setText("not-a-direction");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid measure direction K edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const Vec3& updatedDirection = model.measures.front().def.direction.ijk;
    dvatest::checkNear(updatedDirection.x, direction.x, 1e-12,
                       "invalid measure direction K edit leaves I unchanged");
    dvatest::checkNear(updatedDirection.z, direction.z, 1e-12,
                       "invalid measure direction K edit leaves K unchanged");

    const int updatedRow = propertyRow(window, "Direction K");
    dvatest::check(
        updatedRow >= 0,
        "measure properties stay selected after invalid direction K edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure direction K value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid measure direction K edit restores visible value");
}

TEST("main window measure lsl active edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "LSL Active");
    dvatest::check(row >= 0, "measure properties include LSL active");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure LSL active value item exists");

    value->setCheckState(Qt::Unchecked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(!model.measures.front().def.spec.lslActive,
                   "edited measure LSL active flag is saved to the model");

    const int updatedRow = propertyRow(window, "LSL Active");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after LSL active edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure LSL active value item exists");
    dvatest::check(updated->checkState() == Qt::Unchecked,
                   "updated measure LSL active remains unchecked");
}

TEST("main window measure usl active edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "USL Active");
    dvatest::check(row >= 0, "measure properties include USL active");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure USL active value item exists");

    value->setCheckState(Qt::Unchecked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(!model.measures.front().def.spec.uslActive,
                   "edited measure USL active flag is saved to the model");

    const int updatedRow = propertyRow(window, "USL Active");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after USL active edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure USL active value item exists");
    dvatest::check(updated->checkState() == Qt::Unchecked,
                   "updated measure USL active remains unchecked");
}

TEST("main window measure lsl edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "LSL");
    dvatest::check(row >= 0, "measure properties include LSL");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure LSL value item exists");

    value->setText("9.25");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::checkNear(model.measures.front().def.spec.lsl, 9.25, 1e-12,
                       "edited measure LSL is saved to the model");

    const int updatedRow = propertyRow(window, "LSL");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after LSL edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated measure LSL value item exists");
    dvatest::check(updated->text().toStdString() == "9.25",
                   "updated measure LSL remains visible");
}

TEST("main window invalid measure lsl edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const SpecLimits spec = initial.measures.front().def.spec;

    const int row = propertyRow(window, "LSL");
    dvatest::check(row >= 0, "measure properties include LSL");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure LSL value item exists");
    const QString initialText = value->text();

    value->setText("not-an-lsl");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid measure LSL edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const SpecLimits& updatedSpec = model.measures.front().def.spec;
    dvatest::checkNear(updatedSpec.lsl, spec.lsl, 1e-12,
                       "invalid measure LSL edit leaves LSL unchanged");
    dvatest::checkNear(updatedSpec.usl, spec.usl, 1e-12,
                       "invalid measure LSL edit leaves USL unchanged");

    const int updatedRow = propertyRow(window, "LSL");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after invalid LSL edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure LSL value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid measure LSL edit restores visible value");
}

TEST("main window measure usl edit keeps measure properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "USL");
    dvatest::check(row >= 0, "measure properties include USL");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure USL value item exists");

    value->setText("12.25");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::checkNear(model.measures.front().def.spec.usl, 12.25, 1e-12,
                       "edited measure USL is saved to the model");

    const int updatedRow = propertyRow(window, "USL");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after USL edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated measure USL value item exists");
    dvatest::check(updated->text().toStdString() == "12.25",
                   "updated measure USL remains visible");
}

TEST("main window invalid measure usl edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(createStarterModel());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.measures.empty(),
                   "starter UI model still has measures");
    const SpecLimits spec = initial.measures.front().def.spec;

    const int row = propertyRow(window, "USL");
    dvatest::check(row >= 0, "measure properties include USL");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure USL value item exists");
    const QString initialText = value->text();

    value->setText("not-an-usl");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid numeric value",
                   "invalid measure USL edit reports numeric error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    const SpecLimits& updatedSpec = model.measures.front().def.spec;
    dvatest::checkNear(updatedSpec.lsl, spec.lsl, 1e-12,
                       "invalid measure USL edit leaves LSL unchanged");
    dvatest::checkNear(updatedSpec.usl, spec.usl, 1e-12,
                       "invalid measure USL edit leaves USL unchanged");

    const int updatedRow = propertyRow(window, "USL");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after invalid USL edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure USL value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid measure USL edit restores visible value");
}

TEST("main window measure type edit relabels user-dll routine") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveMeasure());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Measure Type");
    dvatest::check(row >= 0, "measure properties include type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure type value item exists");

    value->setText("User DLL");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.type == MeasureType::UserDll,
                   "edited measure type is saved to the model");

    const int updatedTypeRow = propertyRow(window, "Measure Type");
    dvatest::check(updatedTypeRow >= 0,
                   "measure properties stay selected after type edit");
    QTableWidgetItem* updatedType =
        window.propertyTableForTesting()->item(updatedTypeRow, 1);
    dvatest::check(updatedType != nullptr,
                   "updated measure type value item exists");
    dvatest::check(updatedType->text().toStdString() == "User DLL",
                   "updated measure type remains visible");
    dvatest::check(propertyRow(window, "Equation") < 0,
                   "ordinary equation label is hidden for user-dll measure");
    const int routineRow = propertyRow(window, "User DLL Routine");
    dvatest::check(routineRow >= 0,
                   "user-dll routine label appears after type edit");
    QTableWidgetItem* routine =
        window.propertyTableForTesting()->item(routineRow, 1);
    dvatest::check(routine != nullptr, "user-dll routine value item exists");
    dvatest::check((routine->flags() & Qt::ItemIsEditable) != 0,
                   "user-dll routine value is editable after type edit");
}

TEST("main window invalid measure type edit restores measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveMeasure());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Measure Type");
    dvatest::check(row >= 0, "measure properties include type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure type value item exists");
    dvatest::check(value->text().toStdString() == "Point-Point",
                   "starter measure type is visible");

    value->setText("Not A Measure Type");

    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid measure type",
                   "invalid measure type edit reports type error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.type == MeasureType::PointPoint,
                   "invalid measure type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Measure Type");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after invalid type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored measure type value item exists");
    dvatest::check(updated->text().toStdString() == "Point-Point",
                   "invalid measure type edit restores visible value");
}

TEST("main window unchanged measure type edit refreshes measure properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithInactiveMeasure());
    dvatest::check(window.selectFirstMeasureForTesting(),
                   "starter UI model has a measure item");

    const int row = propertyRow(window, "Measure Type");
    dvatest::check(row >= 0, "measure properties include type");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "measure type value item exists");
    dvatest::check(value->text().toStdString() == "Point-Point",
                   "starter measure type is visible");

    value->setText(" point point ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.measures.empty(), "edited UI model still has measures");
    dvatest::check(model.measures.front().def.type == MeasureType::PointPoint,
                   "unchanged measure type edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Measure Type");
    dvatest::check(updatedRow >= 0,
                   "measure properties stay selected after unchanged type edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated measure type value item exists");
    dvatest::check(updated->text().toStdString() == "Point-Point",
                   "unchanged measure type edit restores canonical value");
    dvatest::check(propertyRow(window, "Equation") >= 0,
                   "ordinary equation label stays visible after unchanged type edit");
    dvatest::check(propertyRow(window, "User DLL Routine") < 0,
                   "user-dll routine stays hidden after unchanged measure type edit");
}

TEST("main window variant rename keeps variant properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "variant properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant name value item exists");

    value->setText("Renamed Baseline");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().name == "Renamed Baseline",
                   "edited variant name is saved to the model");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after rename");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr, "updated variant name value item exists");
    dvatest::check(updated->text().toStdString() == "Renamed Baseline",
                   "updated variant name remains visible");
}

TEST("main window unchanged variant name edit refreshes variant properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.variants.empty(),
                   "starter UI model has variants");

    const int row = propertyRow(window, "Name");
    dvatest::check(row >= 0, "variant properties include name");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant name value item exists");
    const QString initialText = value->text();

    value->setText(" " + initialText + " ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(),
                   "edited UI model still has variants");
    dvatest::check(QString::fromStdString(model.variants.front().name) ==
                       initialText,
                   "unchanged variant name edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Name");
    dvatest::check(
        updatedRow >= 0,
        "variant properties stay selected after unchanged name edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed variant name value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged variant name edit restores canonical value");
}

TEST("main window variant active edit keeps variant properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const int row = propertyRow(window, "Active");
    dvatest::check(row >= 0, "variant properties include active");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant active value item exists");

    value->setCheckState(Qt::Checked);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().active,
                   "edited variant active flag is saved to the model");

    const int updatedRow = propertyRow(window, "Active");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after active edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated variant active value item exists");
    dvatest::check(updated->checkState() == Qt::Checked,
                   "updated variant active remains checked");
}

TEST("main window variant tolerances edit keeps variant properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const int row = propertyRow(window, "Tolerances");
    dvatest::check(row >= 0, "variant properties include tolerances");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant tolerances value item exists");

    value->setText("");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().tolerances.empty(),
                   "edited variant tolerances are saved to the model");

    const int updatedRow = propertyRow(window, "Tolerances");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after tolerances edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated variant tolerances value item exists");
    dvatest::check(updated->text().isEmpty(),
                   "updated variant tolerances remain visible");
}

TEST("main window variant tolerances edit keeps selected tolerance list visible") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoToleranceVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.parts.empty(),
                   "starter UI model has parts");
    dvatest::check(initial.parts.front().tolerances.size() >= 2,
                   "starter UI model has two tolerances");
    const ToleranceId selectedTolerance =
        initial.parts.front().tolerances[1].id;

    const int row = propertyRow(window, "Tolerances");
    dvatest::check(row >= 0, "variant properties include tolerances");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant tolerances value item exists");

    const QString selectedText = QString::number(selectedTolerance);
    value->setText(selectedText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().tolerances.size() == 1,
                   "edited variant keeps one tolerance");
    dvatest::check(model.variants.front().tolerances.front() ==
                       selectedTolerance,
                   "edited variant selected tolerance is saved to the model");

    const int updatedRow = propertyRow(window, "Tolerances");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after tolerance-list edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated variant tolerances value item exists");
    dvatest::check(updated->text() == selectedText,
                   "updated variant selected tolerance remains visible");
}

TEST("main window invalid variant tolerances edit restores variant properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoToleranceVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.variants.empty(),
                   "starter UI model still has variants");
    const std::vector<ToleranceId> tolerances =
        initial.variants.front().tolerances;
    QStringList initialParts;
    for (const ToleranceId id : tolerances) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Tolerances");
    dvatest::check(row >= 0, "variant properties include tolerances");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant tolerances value item exists");
    dvatest::check(value->text() == initialText,
                   "starter variant tolerances are visible");

    value->setText("not-a-tolerance-list");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid variant tolerance list",
                   "invalid variant tolerances edit reports tolerance-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().tolerances == tolerances,
                   "invalid variant tolerances edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Tolerances");
    dvatest::check(
        updatedRow >= 0,
        "variant properties stay selected after invalid tolerances edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored variant tolerances value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid variant tolerances edit restores visible value");
}

TEST("main window unchanged variant tolerances edit refreshes variant properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoToleranceVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.variants.empty(),
                   "starter UI model still has variants");
    const std::vector<ToleranceId> tolerances =
        initial.variants.front().tolerances;
    QStringList initialParts;
    for (const ToleranceId id : tolerances) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Tolerances");
    dvatest::check(row >= 0, "variant properties include tolerances");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant tolerances value item exists");
    dvatest::check(value->text() == initialText,
                   "starter variant tolerances are visible");

    value->setText(initialText + ", ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().tolerances == tolerances,
                   "unchanged variant tolerances edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Tolerances");
    dvatest::check(
        updatedRow >= 0,
        "variant properties stay selected after unchanged tolerances edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed variant tolerances value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged variant tolerances edit restores canonical value");
}

TEST("main window variant moves edit keeps variant properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithMoveVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const int row = propertyRow(window, "Moves");
    dvatest::check(row >= 0, "variant properties include moves");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant moves value item exists");
    dvatest::check(!value->text().isEmpty(),
                   "variant moves value starts populated");

    value->setText("");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().moves.empty(),
                   "edited variant moves are saved to the model");

    const int updatedRow = propertyRow(window, "Moves");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after moves edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated variant moves value item exists");
    dvatest::check(updated->text().isEmpty(),
                   "updated variant moves remain visible");
}

TEST("main window variant moves edit keeps selected move list visible") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.moves.size() >= 2,
                   "starter UI model has two moves");
    const MoveId selectedMove = initial.moves[1].id;

    const int row = propertyRow(window, "Moves");
    dvatest::check(row >= 0, "variant properties include moves");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant moves value item exists");

    const QString selectedText = QString::number(selectedMove);
    value->setText(selectedText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().moves.size() == 1,
                   "edited variant keeps one move");
    dvatest::check(model.variants.front().moves.front() == selectedMove,
                   "edited variant selected move is saved to the model");

    const int updatedRow = propertyRow(window, "Moves");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after move-list edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated variant moves value item exists");
    dvatest::check(updated->text() == selectedText,
                   "updated variant selected move remains visible");
}

TEST("main window invalid variant moves edit restores variant properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.variants.empty(),
                   "starter UI model still has variants");
    const std::vector<MoveId> moves = initial.variants.front().moves;
    QStringList initialParts;
    for (const MoveId id : moves) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Moves");
    dvatest::check(row >= 0, "variant properties include moves");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant moves value item exists");
    dvatest::check(value->text() == initialText,
                   "starter variant moves are visible");

    value->setText("not-a-move-list");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid variant move list",
                   "invalid variant moves edit reports move-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().moves == moves,
                   "invalid variant moves edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Moves");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after invalid moves edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored variant moves value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid variant moves edit restores visible value");
}

TEST("main window unchanged variant moves edit refreshes variant properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMoveVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.variants.empty(),
                   "starter UI model still has variants");
    const std::vector<MoveId> moves = initial.variants.front().moves;
    QStringList initialParts;
    for (const MoveId id : moves) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Moves");
    dvatest::check(row >= 0, "variant properties include moves");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant moves value item exists");
    dvatest::check(value->text() == initialText,
                   "starter variant moves are visible");

    value->setText(initialText + ", ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().moves == moves,
                   "unchanged variant moves edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Moves");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after unchanged moves edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed variant moves value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged variant moves edit restores canonical value");
}

TEST("main window variant measures edit keeps variant properties selected") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const int row = propertyRow(window, "Measures");
    dvatest::check(row >= 0, "variant properties include measures");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant measures value item exists");

    value->setText("");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().measures.empty(),
                   "edited variant measures are saved to the model");

    const int updatedRow = propertyRow(window, "Measures");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after measures edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated variant measures value item exists");
    dvatest::check(updated->text().isEmpty(),
                   "updated variant measures remain visible");
}

TEST("main window variant measures edit keeps selected measure list visible") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMeasureVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(initial.measures.size() >= 2,
                   "starter UI model has two measures");
    const MeasureId selectedMeasure = initial.measures[1].id;

    const int row = propertyRow(window, "Measures");
    dvatest::check(row >= 0, "variant properties include measures");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant measures value item exists");

    const QString selectedText = QString::number(selectedMeasure);
    value->setText(selectedText);

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().measures.size() == 1,
                   "edited variant keeps one measure");
    dvatest::check(model.variants.front().measures.front() == selectedMeasure,
                   "edited variant selected measure is saved to the model");

    const int updatedRow = propertyRow(window, "Measures");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after measure-list edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "updated variant measures value item exists");
    dvatest::check(updated->text() == selectedText,
                   "updated variant selected measure remains visible");
}

TEST("main window invalid variant measures edit restores variant properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMeasureVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.variants.empty(),
                   "starter UI model still has variants");
    const std::vector<MeasureId> measures =
        initial.variants.front().measures;
    QStringList initialParts;
    for (const MeasureId id : measures) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Measures");
    dvatest::check(row >= 0, "variant properties include measures");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant measures value item exists");
    dvatest::check(value->text() == initialText,
                   "starter variant measures are visible");

    value->setText("not-a-measure-list");
    dvatest::check(window.statusBar()->currentMessage() ==
                       "Invalid variant measure list",
                   "invalid variant measures edit reports measure-list error");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().measures == measures,
                   "invalid variant measures edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Measures");
    dvatest::check(updatedRow >= 0,
                   "variant properties stay selected after invalid measures edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "restored variant measures value item exists");
    dvatest::check(updated->text() == initialText,
                   "invalid variant measures edit restores visible value");
}

TEST("main window unchanged variant measures edit refreshes variant properties") {
    ui::MainWindow window;
    window.setModelForTesting(modelWithTwoMeasureVariant());
    dvatest::check(window.selectFirstVariantForTesting(),
                   "starter UI model has a variant item");

    const Model& initial = window.modelForTesting();
    dvatest::check(!initial.variants.empty(),
                   "starter UI model still has variants");
    const std::vector<MeasureId> measures =
        initial.variants.front().measures;
    QStringList initialParts;
    for (const MeasureId id : measures) {
        initialParts << QString::number(id);
    }
    const QString initialText = initialParts.join(", ");

    const int row = propertyRow(window, "Measures");
    dvatest::check(row >= 0, "variant properties include measures");
    QTableWidgetItem* value = window.propertyTableForTesting()->item(row, 1);
    dvatest::check(value != nullptr, "variant measures value item exists");
    dvatest::check(value->text() == initialText,
                   "starter variant measures are visible");

    value->setText(initialText + ", ");

    const Model& model = window.modelForTesting();
    dvatest::check(!model.variants.empty(), "edited UI model still has variants");
    dvatest::check(model.variants.front().measures == measures,
                   "unchanged variant measures edit leaves model unchanged");

    const int updatedRow = propertyRow(window, "Measures");
    dvatest::check(
        updatedRow >= 0,
        "variant properties stay selected after unchanged measures edit");
    QTableWidgetItem* updated =
        window.propertyTableForTesting()->item(updatedRow, 1);
    dvatest::check(updated != nullptr,
                   "refreshed variant measures value item exists");
    dvatest::check(updated->text() == initialText,
                   "unchanged variant measures edit restores canonical value");
}
