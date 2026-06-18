// A8 Qt UI main window for the OpenDVA desktop MVP.
#pragma once
#include <QMainWindow>
#include <QStringList>

#include "opendva/domain/AppPreferences.h"
#include "opendva/domain/Model.h"

class QCloseEvent;
class QMenu;
class QTableWidget;
class QTableWidgetItem;
class QTreeWidget;
class QTreeWidgetItem;

namespace opendva::ui {

class ModelViewport;

enum class NavigatorNodeKind {
    Root = 0,
    Part,
    Point,
    Feature,
    Tolerance,
    Gdt,
    Move,
    Measure,
    Variant,
    Category
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

#ifdef OPENDVA_UI_TESTING
    void setModelForTesting(const Model& model);
    bool selectRootForTesting();
    bool selectFirstPartForTesting();
    bool selectFirstPointForTesting();
    bool selectFirstFeatureForTesting();
    bool selectFirstToleranceForTesting();
    bool selectFirstGdtForTesting();
    bool selectFirstMoveForTesting();
    bool selectFirstMeasureForTesting();
    bool selectFirstVariantForTesting();
    bool moveNavigatorMoveForTesting(int fromIndex, int toIndex);
    void setPreferencesForTesting(const AppPreferences& preferences);
    void setPreferencesPathForTesting(const QString& path);
    void applyPreferencesForTesting(const AppPreferences& preferences);
    QStringList recentFileActionTextsForTesting() const;
    bool recentFileActionEnabledForTesting(int index) const;
    bool triggerRecentFileForTesting(int index);
    bool openModelFromPathForTesting(const QString& path);
    bool saveModelToPathForTesting(const QString& path);
    void newModelForTesting();
    const Model& modelForTesting() const;
    QTableWidget* propertyTableForTesting() const;
    QStringList navigatorContextActionNamesForTesting();
    QStringList navigatorContextActionTextsForTesting();
    QStringList navigatorContextActionSequenceForTesting();
    QStringList navigatorContextEnabledActionNamesForTesting();
#endif

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void validateModel();
    void runMonteCarlo();
    void runBatchProcessor();
    void newModel();
    void openModel();
    void openRecentModel();
    void saveModel();
    void saveModelAs();
    void showPreferences();
    void showModuleWindow(const QString& windowId);
    void addPart();
    void addPoint();
    void addFeature();
    void addLinearTolerance();
    void addGdt();
    void addTransformMove();
    void addPointPointMeasure();
    void moveSelectedMoveUp();
    void moveSelectedMoveDown();
    void deleteSelectedNavigatorItem();
    void captureModelVariant();
    void applyModelVariant();
    void deleteModelVariant();
    void showNavigatorMenu(const QPoint& position);
    void updateSelectionDetails(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void handlePropertyEdited(QTableWidgetItem* item);

private:
    void buildMenus();
    void buildDocks();
    void populateNavigatorContextMenu(QMenu& menu,
                                      NavigatorNodeKind selectedKind);
    void populateNavigator();
    void refreshUi();
    bool syncMoveOrderFromNavigator(MoveId selectedMove);
    void updateWindowTitle();
    void setDirty(bool dirty);
    bool maybeSaveDirtyModel();
    bool openModelFromPath(const QString& path);
    void recordRecentModelPath(const QString& path);
    void applyPreferences(const AppPreferences& preferences);
    void savePreferences();
    void updateRecentFilesMenu();
    bool saveModelToPath(const QString& path);
    void showModelSummary();
    PartId selectedPartContext() const;
    PointId selectedPointContext() const;

    Model model_;
    AppPreferences preferences_{};
    QString preferencesPath_;
    QString currentPath_;
    bool dirty_{false};
    bool updatingProperties_{false};

    QTreeWidget* navigator_{};
    QTableWidget* propertyTable_{};
    ModelViewport* viewport_{};
    QMenu* recentFilesMenu_{};
};

}  // namespace opendva::ui
