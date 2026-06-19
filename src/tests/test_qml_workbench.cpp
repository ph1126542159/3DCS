#include "dva_test.h"

#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QVariantMap>

#include "CommandExecutionModel.h"
#include "FeatureCatalogModel.h"
#include "WorkbenchModel.h"

TEST("qml workbench loads with workbench and feature catalog models") {
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    opendva::ui::FeatureCatalogModel featureCatalog;
    opendva::ui::WorkbenchModel workbenchModel;
    opendva::ui::CommandExecutionModel commandExecutor;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("featureCatalog", &featureCatalog);
    engine.rootContext()->setContextProperty("workbenchModel", &workbenchModel);
    engine.rootContext()->setContextProperty("commandExecutor", &commandExecutor);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    dvatest::check(!engine.rootObjects().isEmpty(),
                   "QML workbench root object loads");
    QObject* root = engine.rootObjects().front();
    dvatest::check(root->property("title")
                       .toString()
                       .contains(QStringLiteral("3DCS Industrial Workbench")),
                   "QML workbench exposes the industrial workbench title");
    dvatest::check(root->property("prototypeDirection").toString() ==
                       QStringLiteral("Neon Assembly Process Twin"),
                   "QML workbench exposes the selected Product Design prototype direction");
    dvatest::check(root->property("selectedWorkspace").toInt() == 0,
                   "QML workbench starts on the first workspace");
    dvatest::check(workbenchModel.workspaceCount() == 14,
                   "QML workbench test uses all workbench model rows");
    QObject* viewport =
        root->findChild<QObject*>(QStringLiteral("industrialQt3DViewport"));
    dvatest::check(viewport != nullptr,
                   "QML workbench exposes the Qt3D industrial viewport");
    QObject* processTwinRail =
        root->findChild<QObject*>(QStringLiteral("processTwinRail"));
    dvatest::check(processTwinRail != nullptr,
                   "QML workbench exposes the animated process twin rail");
    QObject* mtmGlassInspector =
        root->findChild<QObject*>(QStringLiteral("mtmGlassInspector"));
    dvatest::check(mtmGlassInspector != nullptr,
                   "QML workbench exposes the glass MTM inspector");
    QObject* simulationTwinConsole =
        root->findChild<QObject*>(QStringLiteral("simulationTwinConsole"));
    dvatest::check(simulationTwinConsole != nullptr,
                   "QML workbench exposes the simulation twin console");
    QObject* officialHelpTree =
        root->findChild<QObject*>(QStringLiteral("officialHelpTree"));
    dvatest::check(officialHelpTree != nullptr,
                   "QML workbench exposes the official help tree");

    auto indexForCatalogTitle = [&featureCatalog](const QString& title) {
        for (int i = 0; i < featureCatalog.rowCount(); ++i) {
            if (featureCatalog.get(i).value("title").toString() == title) {
                return i;
            }
        }
        return -1;
    };

    int catalogIndex = indexForCatalogTitle(QStringLiteral("Tree Link Wizard"));
    QVariant routed;
    bool invoked = QMetaObject::invokeMethod(
        root, "selectCatalogNode", Q_RETURN_ARG(QVariant, routed),
        Q_ARG(QVariant, QVariant(catalogIndex)));
    dvatest::check(invoked && routed.toBool(),
                   "QML official help tree can route Tree Link Wizard");
    dvatest::check(root->property("selectedWorkspace").toInt() ==
                       workbenchModel.indexOf("modeling"),
                   "QML help-tree Tree Link route opens Modeling");
    dvatest::check(root->property("selectedCommand").toString() ==
                       QStringLiteral("Tree Link"),
                   "QML help-tree Tree Link route selects Tree Link command");
    QVariantMap catalogNode = root->property("catalogNode").toMap();
    dvatest::check(catalogNode.value("href").toString() ==
                       QStringLiteral("treelinkwizard.htm"),
                   "QML help-tree selection retains official help href");

    catalogIndex = indexForCatalogTitle(QStringLiteral("StiffGen"));
    invoked = QMetaObject::invokeMethod(
        root, "selectCatalogNode", Q_RETURN_ARG(QVariant, routed),
        Q_ARG(QVariant, QVariant(catalogIndex)));
    dvatest::check(invoked && routed.toBool(),
                   "QML official help tree can route StiffGen");
    dvatest::check(root->property("selectedWorkspace").toInt() ==
                       workbenchModel.indexOf("fea"),
                   "QML help-tree StiffGen route opens FEA workspace");
    dvatest::check(root->property("selectedCommand").toString() ==
                       QStringLiteral("StiffGen"),
                   "QML help-tree StiffGen route selects StiffGen command");
    QVariantMap routedCommand = root->property("commandView").toMap();
    dvatest::check(routedCommand.value("viewType").toString() ==
                       QStringLiteral("aset"),
                   "QML help-tree StiffGen route opens the ASET GUI panel");

    catalogIndex = indexForCatalogTitle(QStringLiteral("Tolerance Optimizer"));
    invoked = QMetaObject::invokeMethod(
        root, "selectCatalogNode", Q_RETURN_ARG(QVariant, routed),
        Q_ARG(QVariant, QVariant(catalogIndex)));
    dvatest::check(invoked && routed.toBool(),
                   "QML official help tree can route Tolerance Optimizer");
    dvatest::check(root->property("selectedWorkspace").toInt() ==
                       workbenchModel.indexOf("aao"),
                   "QML help-tree Tolerance Optimizer route opens AAO");
    routedCommand = root->property("commandView").toMap();
    dvatest::check(routedCommand.value("viewType").toString() ==
                       QStringLiteral("optimizer"),
                   "QML help-tree Tolerance Optimizer route opens optimizer panel");

    const int aaoIndex = workbenchModel.indexOf("aao");
    bool selected = QMetaObject::invokeMethod(
        root, "selectWorkspace", Q_ARG(QVariant, QVariant(aaoIndex)));
    dvatest::check(selected, "QML selectWorkspace function is callable");
    selected = QMetaObject::invokeMethod(
        root, "selectCommand", Q_ARG(QVariant, QVariant(QStringLiteral("CTI"))));
    dvatest::check(selected, "QML selectCommand function is callable");
    dvatest::check(root->property("selectedWorkspace").toInt() == aaoIndex,
                   "QML workbench switches to AAO workspace");
    dvatest::check(root->property("selectedCommand").toString() == QStringLiteral("CTI"),
                   "QML workbench tracks the selected command");

    const QVariantMap commandView = root->property("commandView").toMap();
    dvatest::check(commandView.value("panel").toString().contains("CTI"),
                   "QML workbench exposes CTI command panel data");
    dvatest::check(commandView.value("matrixRows").toString().contains("GeoFactor %"),
                   "QML workbench exposes command-specific matrix rows");
    dvatest::check(commandView.value("state").toString().contains("Matrix Ready"),
                   "QML workbench exposes command execution state");
    dvatest::check(commandView.value("viewType").toString() == QStringLiteral("matrix"),
                   "QML workbench exposes command specialized view type");
    dvatest::check(commandView.value("sourceChapter").toString().contains("Chapter 9"),
                   "QML workbench exposes command source chapter metadata");
    dvatest::check(commandView.value("officialGui").toString().contains("CTI"),
                   "QML workbench exposes official GUI source metadata");
    dvatest::check(viewport != nullptr &&
                       viewport->property("modeLabel").toString() == QStringLiteral("CTI"),
                   "QML Qt3D viewport follows the selected command label");
    dvatest::check(viewport != nullptr &&
                       viewport->property("viewType").toString() == QStringLiteral("matrix"),
                   "QML Qt3D viewport follows the selected command view type");

    QVariant prerequisites;
    invoked = QMetaObject::invokeMethod(
        root, "activePrerequisites", Q_RETURN_ARG(QVariant, prerequisites));
    dvatest::check(invoked, "QML activePrerequisites helper is callable");
    dvatest::check(prerequisites.toStringList().contains(QStringLiteral("HLM/GF2 imported")),
                   "QML workbench exposes command prerequisites");

    QVariant outputSignals;
    invoked = QMetaObject::invokeMethod(root, "activeSignals",
                                        Q_RETURN_ARG(QVariant, outputSignals));
    dvatest::check(invoked, "QML activeSignals helper is callable");
    dvatest::check(outputSignals.toStringList().contains(QStringLiteral("CTI rank")),
                   "QML workbench exposes command output signals");

    QVariant detailRows;
    invoked = QMetaObject::invokeMethod(root, "activeDetailRows",
                                        Q_RETURN_ARG(QVariant, detailRows));
    dvatest::check(invoked, "QML activeDetailRows helper is callable");
    dvatest::check(detailRows.toStringList().contains(QStringLiteral("Datum A2:33:1")),
                   "QML workbench exposes specialized detail rows");

    invoked = QMetaObject::invokeMethod(
        root, "goToWorkspace", Q_RETURN_ARG(QVariant, routed),
        Q_ARG(QVariant, QVariant(QStringLiteral("reports"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("Generate Report"))));
    dvatest::check(invoked && routed.toBool(),
                   "QML global workspace shortcut is callable");
    dvatest::check(root->property("selectedWorkspace").toInt() ==
                       workbenchModel.indexOf("reports"),
                   "QML global shortcut switches workspace");
    dvatest::check(root->property("selectedCommand").toString() ==
                       QStringLiteral("Generate Report"),
                   "QML global shortcut selects command");

    const int cadIndex = workbenchModel.indexOf("cad");
    selected = QMetaObject::invokeMethod(
        root, "selectWorkspace", Q_ARG(QVariant, QVariant(cadIndex)));
    dvatest::check(selected, "QML can select CAD integration workspace");
    dvatest::check(root->property("selectedCommand").toString() ==
                       QStringLiteral("NX"),
                   "QML CAD workspace starts with the first CAD command");
    selected = QMetaObject::invokeMethod(
        root, "selectCommand",
        Q_ARG(QVariant, QVariant(QStringLiteral("PMI Extract"))));
    dvatest::check(selected, "QML can select CAD PMI Extract command");
    routedCommand = root->property("commandView").toMap();
    dvatest::check(routedCommand.value("viewType").toString() ==
                       QStringLiteral("cad"),
                   "QML CAD command exposes the CAD specialized view");
    dvatest::check(routedCommand.value("sourceChapter").toString().contains("Chapter 12"),
                   "QML CAD command exposes the official CAD chapter source");
    detailRows.clear();
    invoked = QMetaObject::invokeMethod(root, "activeDetailRows",
                                        Q_RETURN_ARG(QVariant, detailRows));
    dvatest::check(invoked &&
                       detailRows.toStringList().contains(QStringLiteral("NX:PMI:Missing")),
                   "QML CAD view exposes host adapter detail rows");

    invoked = QMetaObject::invokeMethod(
        root, "goToWorkspace", Q_RETURN_ARG(QVariant, routed),
        Q_ARG(QVariant, QVariant(QStringLiteral("userdll"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("Measure Hook"))));
    dvatest::check(invoked && routed.toBool(),
                   "QML can route to User DLL Measure Hook");
    routedCommand = root->property("commandView").toMap();
    dvatest::check(routedCommand.value("viewType").toString() ==
                       QStringLiteral("sdk"),
                   "QML User DLL command exposes the SDK specialized view");
    dvatest::check(routedCommand.value("sourceChapter").toString().contains("Chapter 13"),
                   "QML User DLL command exposes the official SDK chapter source");

    invoked = QMetaObject::invokeMethod(
        root, "goToWorkspace", Q_RETURN_ARG(QVariant, routed),
        Q_ARG(QVariant, QVariant(QStringLiteral("help"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("Formula Reference"))));
    dvatest::check(invoked && routed.toBool(),
                   "QML can route to Help Formula Reference");
    routedCommand = root->property("commandView").toMap();
    dvatest::check(routedCommand.value("viewType").toString() ==
                       QStringLiteral("help"),
                   "QML Help command exposes the help specialized view");

    const int toleranceIndex = workbenchModel.indexOf("tolerances");
    invoked = QMetaObject::invokeMethod(
        root, "selectNavigatorCommand", Q_RETURN_ARG(QVariant, routed),
        Q_ARG(QVariant, QVariant(toleranceIndex)),
        Q_ARG(QVariant, QVariant(QStringLiteral("Add GD&T"))));
    dvatest::check(invoked && routed.toBool(),
                   "QML navigator tree can select a command child node");
    dvatest::check(root->property("selectedWorkspace").toInt() == toleranceIndex,
                   "QML navigator command selection switches workspace row");
    dvatest::check(root->property("selectedCommand").toString() ==
                       QStringLiteral("Add GD&T"),
                   "QML navigator command selection switches command");
    routedCommand = root->property("commandView").toMap();
    dvatest::check(routedCommand.value("viewType").toString() ==
                       QStringLiteral("gdt"),
                   "QML navigator command opens the command-specific GUI panel");

    const int movesIndex = workbenchModel.indexOf("moves");
    invoked = QMetaObject::invokeMethod(
        root, "selectNavigatorCommand", Q_RETURN_ARG(QVariant, routed),
        Q_ARG(QVariant, QVariant(movesIndex)),
        Q_ARG(QVariant, QVariant(QStringLiteral("Auto Bend"))));
    dvatest::check(invoked && routed.toBool(),
                   "QML navigator can select an expanded move-family child node");
    routedCommand = root->property("commandView").toMap();
    dvatest::check(routedCommand.value("viewType").toString() ==
                       QStringLiteral("move"),
                   "QML expanded move node opens the move specialized GUI");
    dvatest::check(routedCommand.value("detailRows")
                       .toString()
                       .contains(QStringLiteral("Auto Bend")),
                   "QML expanded move node exposes Auto Bend detail state");

    invoked = QMetaObject::invokeMethod(
        root, "goToWorkspace", Q_RETURN_ARG(QVariant, routed),
        Q_ARG(QVariant, QVariant(QStringLiteral("aao"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("Tolerance Optimizer"))));
    dvatest::check(invoked && routed.toBool(),
                   "QML can route to AAO Tolerance Optimizer");
    routedCommand = root->property("commandView").toMap();
    dvatest::check(routedCommand.value("viewType").toString() ==
                       QStringLiteral("optimizer"),
                   "QML AAO optimizer command exposes optimizer view type");

    invoked = QMetaObject::invokeMethod(
        root, "goToWorkspace", Q_RETURN_ARG(QVariant, routed),
        Q_ARG(QVariant, QVariant(QStringLiteral("simulation"))),
        Q_ARG(QVariant, QVariant(QStringLiteral("Run Analysis"))));
    dvatest::check(invoked && routed.toBool(),
                   "QML simulation shortcut is callable");
    const QVariantMap simulationView = root->property("commandView").toMap();
    dvatest::check(simulationView.value("viewType").toString() ==
                       QStringLiteral("simulation"),
                   "QML workbench exposes simulation specialized view");
    dvatest::check(viewport != nullptr &&
                       viewport->property("modeLabel").toString() ==
                           QStringLiteral("Run Analysis"),
                   "QML Qt3D viewport updates when global shortcuts route commands");
    dvatest::check(viewport != nullptr &&
                       viewport->property("viewType").toString() ==
                           QStringLiteral("simulation"),
                   "QML Qt3D viewport reflects the simulation command view");
    detailRows.clear();
    invoked = QMetaObject::invokeMethod(root, "activeDetailRows",
                                        Q_RETURN_ARG(QVariant, detailRows));
    dvatest::check(invoked &&
                       detailRows.toStringList().contains(QStringLiteral("Gap_014:142:Ready")),
                   "QML simulation view exposes run detail rows");

    QVariant outputs;
    invoked = QMetaObject::invokeMethod(root, "activeOutputs",
                                        Q_RETURN_ARG(QVariant, outputs));
    dvatest::check(invoked, "QML activeOutputs helper is callable");
    dvatest::check(outputs.toStringList().contains(QStringLiteral("HST file")),
                   "QML simulation view exposes output artifacts");
    dvatest::check(simulationView.value("executeLabel").toString() ==
                       QStringLiteral("Run Monte Carlo"),
                   "QML simulation view exposes execution label");

    QVariant executionEntry;
    invoked = QMetaObject::invokeMethod(root, "executeCommand",
                                        Q_RETURN_ARG(QVariant, executionEntry));
    dvatest::check(invoked, "QML executeCommand helper is callable");
    dvatest::check(root->property("executionCounter").toInt() == 1,
                   "QML executeCommand increments the execution counter");
    dvatest::check(commandExecutor.executionCount() == 1,
                   "QML executeCommand delegates execution tracking to C++");
    dvatest::check(root->property("lastExecution")
                       .toString()
                       .contains(QStringLiteral("Run Analysis")),
                   "QML executeCommand records the selected command");
    dvatest::check(executionEntry.toString().contains(QStringLiteral("HST file")),
                   "QML executeCommand records output artifacts");
    const QVariantMap executionRecord =
        root->property("lastExecutionRecord").toMap();
    dvatest::check(executionRecord.value("id").toString() == QStringLiteral("EXEC-0001"),
                   "QML executeCommand creates a stable execution id");
    dvatest::check(executionRecord.value("workspace").toString() ==
                       QStringLiteral("simulation"),
                   "QML executeCommand records the workspace id");
    dvatest::check(executionRecord.value("command").toString() ==
                       QStringLiteral("Run Analysis"),
                   "QML executeCommand records the command name");
    dvatest::check(executionRecord.value("riskLevel").toString() ==
                       QStringLiteral("Watch"),
                   "QML executeCommand derives a risk level");
    dvatest::check(executionRecord.value("outputCount").toInt() == 4,
                   "QML executeCommand records output count");
    dvatest::check(commandExecutor.lastRecord().value("id").toString() ==
                       QStringLiteral("EXEC-0001"),
                   "C++ command executor stores the latest execution record");
    const QVariantList activityFeed = root->property("activityFeed").toList();
    dvatest::check(!activityFeed.isEmpty() &&
                       activityFeed.front().toString().contains(QStringLiteral("Run Monte Carlo")),
                   "QML executeCommand pushes an activity feed entry");
}
