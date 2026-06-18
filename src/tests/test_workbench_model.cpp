#include "dva_test.h"

#include <QVariantMap>

#include "CommandExecutionModel.h"
#include "WorkbenchModel.h"

TEST("workbench model exposes all required 3DCS GUI workspaces") {
    opendva::ui::WorkbenchModel model;

    dvatest::check(model.workspaceCount() == 14,
                   "workbench covers all primary 3DCS workspaces");

    const char* requiredIds[] = {"modeling",     "moves",      "tolerances",
                                 "measures",     "simulation", "visualization",
                                 "aao",          "mechanical", "fea",
                                 "reports",      "cad",        "userdll",
                                 "help",         "system"};
    for (const char* id : requiredIds) {
        dvatest::check(model.findById(QString::fromLatin1(id)) != nullptr,
                       QString("workspace exists: %1").arg(id).toStdString());
    }
}

TEST("workbench model keeps gaps visible instead of claiming completion") {
    opendva::ui::WorkbenchModel model;

    const auto* aao = model.findById("aao");
    dvatest::check(aao != nullptr, "AAO workspace exists");
    dvatest::check(aao != nullptr && aao->status == "Shell",
                   "AAO is explicitly marked as a shell");
    dvatest::check(aao != nullptr && aao->commands.contains("GeoFactor"),
                   "AAO includes GeoFactor matrix command");
    dvatest::check(aao != nullptr && aao->gaps.contains("optimizers"),
                   "AAO missing optimizer work remains visible");

    const auto* fea = model.findById("fea");
    dvatest::check(fea != nullptr, "FEA workspace exists");
    dvatest::check(fea != nullptr && fea->status == "Missing",
                   "FEA remains explicitly marked missing");
    dvatest::check(fea != nullptr && fea->commands.contains("StiffGen"),
                   "FEA includes StiffGen command placeholder");
}

TEST("workbench model provides specialized panel data for advanced modules") {
    opendva::ui::WorkbenchModel model;

    const auto* aao = model.findById("aao");
    dvatest::check(aao != nullptr && aao->workflow.contains("Import HLM"),
                   "AAO exposes the HLM/GF2 matrix workflow");
    dvatest::check(aao != nullptr && aao->matrixRows.contains("Tol_A"),
                   "AAO exposes GeoFactor matrix columns");
    dvatest::check(aao != nullptr && aao->editorFields.contains("Percent Filter"),
                   "AAO exposes CTI filter controls");

    const auto* mechanical = model.findById("mechanical");
    dvatest::check(mechanical != nullptr &&
                       mechanical->primaryPanel.contains("DOF Counter"),
                   "Mechanical exposes DOF Counter workspace");
    dvatest::check(mechanical != nullptr &&
                       mechanical->matrixRows.contains("Before DOF"),
                   "Mechanical exposes DOF matrix data");

    const auto* fea = model.findById("fea");
    dvatest::check(fea != nullptr && fea->workflow.contains("StiffGen"),
                   "FEA exposes StiffGen workflow");
    dvatest::check(fea != nullptr && fea->matrixRows.contains("ASET Links"),
                   "FEA exposes ASET link status");

    const auto* reports = model.findById("reports");
    dvatest::check(reports != nullptr &&
                       reports->primaryPanel.contains("Report Generator"),
                   "Reports exposes report generator workspace");
    dvatest::check(reports != nullptr && reports->matrixRows.contains("Analysis Page"),
                   "Reports exposes report page composition data");

    const auto* cad = model.findById("cad");
    dvatest::check(cad != nullptr && cad->commands.contains("PMI Extract"),
                   "CAD integration exposes PMI extraction workspace");

    const auto* userDll = model.findById("userdll");
    dvatest::check(userDll != nullptr && userDll->commands.contains("Measure Hook"),
                   "User DLL exposes external measure hook workspace");

    const auto* help = model.findById("help");
    dvatest::check(help != nullptr && help->commands.contains("Formula Reference"),
                   "Help workspace exposes formula reference content");
}

TEST("workbench model expands dense official 3DCS command families") {
    opendva::ui::WorkbenchModel model;

    const auto* moves = model.findById("moves");
    dvatest::check(moves != nullptr && moves->commands.contains("Auto Bend"),
                   "Moves exposes Auto Bend from the official move family");
    dvatest::check(moves != nullptr && moves->commands.contains("Conditional Logic"),
                   "Moves exposes conditional logic workflow");

    const auto* tolerances = model.findById("tolerances");
    dvatest::check(tolerances != nullptr && tolerances->commands.contains("Circularity"),
                   "Tolerances expose circularity GD&T control");
    dvatest::check(tolerances != nullptr && tolerances->commands.contains("Datum Target"),
                   "Tolerances expose datum target control");

    const auto* measures = model.findById("measures");
    dvatest::check(measures != nullptr && measures->commands.contains("Circle Interference"),
                   "Measures expose circle interference workflow");
    dvatest::check(measures != nullptr && measures->commands.contains("Measurement Generator"),
                   "Measures expose measurement generator workflow");

    const auto* simulation = model.findById("simulation");
    dvatest::check(simulation != nullptr && simulation->commands.contains("Worst Case"),
                   "Simulation exposes worst-case analysis");
    dvatest::check(simulation != nullptr && simulation->commands.contains("HST/HLM Files"),
                   "Simulation exposes HST/HLM file management");

    const auto* visualization = model.findById("visualization");
    dvatest::check(visualization != nullptr && visualization->commands.contains("Spec Study"),
                   "Visualization exposes spec study overlays");

    const auto* fea = model.findById("fea");
    dvatest::check(fea != nullptr && fea->commands.contains("Displacement Scalar"),
                   "FEA exposes displacement scalar workflow");
    dvatest::check(fea != nullptr && fea->commands.contains("Gravity"),
                   "FEA exposes gravity load workflow");
}

TEST("workbench model returns command-specific workbench details") {
    opendva::ui::WorkbenchModel model;

    const int aaoIndex = model.indexOf("aao");
    const QVariantMap cti = model.commandDetail(aaoIndex, "CTI");
    dvatest::check(cti.value("panel").toString().contains("CTI"),
                   "AAO CTI command exposes contributor ranking panel");
    dvatest::check(cti.value("sourceChapter").toString().contains("Chapter 9"),
                   "AAO CTI command maps back to the official AAO chapter");
    dvatest::check(cti.value("officialGui").toString().contains("GeoFactor Analyzer"),
                   "AAO CTI command exposes official GUI source context");
    dvatest::check(cti.value("matrixRows").toString().contains("GeoFactor %"),
                   "AAO CTI command exposes ranking matrix rows");
    dvatest::check(cti.value("state").toString().contains("Matrix Ready"),
                   "AAO CTI command exposes execution state");
    dvatest::check(cti.value("prerequisites").toString().contains("HLM/GF2"),
                   "AAO CTI command exposes matrix prerequisites");
    dvatest::check(cti.value("viewType").toString() == QStringLiteral("matrix"),
                   "AAO CTI command uses a matrix specialized view");
    dvatest::check(cti.value("detailRows").toString().contains("Datum A2:33:1"),
                   "AAO CTI command exposes matrix detail rows");
    dvatest::check(cti.value("executeLabel").toString() == QStringLiteral("Rank CTI"),
                   "AAO CTI command exposes an execution label");
    dvatest::check(cti.value("outputs").toString().contains("TO/SO handoff"),
                   "AAO CTI command exposes downstream output artifacts");

    const int mechanicalIndex = model.indexOf("mechanical");
    const QVariantMap dof = model.commandDetail(mechanicalIndex, "DOF Counter");
    dvatest::check(dof.value("matrixRows").toString().contains("Tx"),
                   "Mechanical DOF Counter exposes six-axis state columns");
    dvatest::check(dof.value("signals").toString().contains("Free DOF"),
                   "Mechanical DOF Counter exposes output signals");

    const int feaIndex = model.indexOf("fea");
    const QVariantMap stiffGen = model.commandDetail(feaIndex, "StiffGen");
    dvatest::check(stiffGen.value("description").toString().contains("ASET"),
                   "FEA StiffGen command explains ASET workflow");
    dvatest::check(stiffGen.value("metrics").toString().contains("Validation:Missing"),
                   "FEA StiffGen keeps missing validation status visible");
    dvatest::check(stiffGen.value("state").toString().contains("Blocked"),
                   "FEA StiffGen exposes blocked execution state");
    dvatest::check(stiffGen.value("viewType").toString() == QStringLiteral("aset"),
                   "FEA StiffGen command uses an ASET specialized view");
    dvatest::check(stiffGen.value("risk").toString().contains("blocked"),
                   "FEA StiffGen command exposes blocking risk");
    dvatest::check(stiffGen.value("sourceChapter").toString().contains("Chapter 11"),
                   "FEA StiffGen maps back to the official FEA chapter");

    const int cadIndex = model.indexOf("cad");
    const QVariantMap pmi = model.commandDetail(cadIndex, "PMI Extract");
    dvatest::check(pmi.value("viewType").toString() == QStringLiteral("cad"),
                   "CAD PMI Extract command uses a CAD specialized view");
    dvatest::check(pmi.value("matrixRows").toString().contains("CATIA V5"),
                   "CAD PMI Extract command exposes host adapter rows");
    dvatest::check(pmi.value("sourceChapter").toString().contains("Chapter 12"),
                   "CAD PMI Extract maps back to the CAD integration chapter");

    const int userDllIndex = model.indexOf("userdll");
    const QVariantMap hook = model.commandDetail(userDllIndex, "Measure Hook");
    dvatest::check(hook.value("viewType").toString() == QStringLiteral("sdk"),
                   "User DLL Measure Hook command uses an SDK specialized view");
    dvatest::check(hook.value("sourceChapter").toString().contains("Chapter 13"),
                   "User DLL Measure Hook maps back to the User DLL chapter");

    const int helpIndex = model.indexOf("help");
    const QVariantMap formula = model.commandDetail(helpIndex, "Formula Reference");
    dvatest::check(formula.value("viewType").toString() == QStringLiteral("help"),
                   "Help Formula Reference command uses a help specialized view");
}

TEST("workbench model returns specialized details for expanded nodes") {
    opendva::ui::WorkbenchModel model;

    const QVariantMap autoBend =
        model.commandDetail(model.indexOf("moves"), QStringLiteral("Auto Bend"));
    dvatest::check(autoBend.value("viewType").toString() == QStringLiteral("move"),
                   "Auto Bend uses the move specialized GUI");
    dvatest::check(autoBend.value("detailRows").toString().contains("Auto Bend"),
                   "Auto Bend keeps compliant move shell state visible");

    const QVariantMap circularity =
        model.commandDetail(model.indexOf("tolerances"), QStringLiteral("Circularity"));
    dvatest::check(circularity.value("viewType").toString() == QStringLiteral("gdt"),
                   "Circularity uses the GD&T specialized GUI");
    dvatest::check(circularity.value("detailRows").toString().contains("Circularity"),
                   "Circularity appears in the GD&T detail rows");

    const QVariantMap worstCase =
        model.commandDetail(model.indexOf("simulation"), QStringLiteral("Worst Case"));
    dvatest::check(worstCase.value("viewType").toString() == QStringLiteral("simulation"),
                   "Worst Case uses the simulation specialized GUI");
    dvatest::check(worstCase.value("outputs").toString().contains("HST file"),
                   "Worst Case shares the simulation output artifact model");

    const QVariantMap optimizer =
        model.commandDetail(model.indexOf("aao"), QStringLiteral("Tolerance Optimizer"));
    dvatest::check(optimizer.value("viewType").toString() == QStringLiteral("optimizer"),
                   "Tolerance Optimizer uses an optimizer specialized GUI");
    dvatest::check(optimizer.value("matrixRows").toString().contains("Sequence Optimizer"),
                   "Optimizer board includes adjacent AAO optimizer scenarios");

    const QVariantMap gravity =
        model.commandDetail(model.indexOf("fea"), QStringLiteral("Gravity"));
    dvatest::check(gravity.value("viewType").toString() == QStringLiteral("aset"),
                   "Gravity uses the compliant FEA specialized GUI");
    dvatest::check(gravity.value("detailRows").toString().contains("Thermal"),
                   "FEA load stack exposes thermal and force neighbors");
}

TEST("workbench model returns specialized views for core workbench commands") {
    opendva::ui::WorkbenchModel model;

    const QVariantMap contact =
        model.commandDetail(model.indexOf("moves"), QStringLiteral("Contact"));
    dvatest::check(contact.value("viewType").toString() == QStringLiteral("move"),
                   "Contact move command uses a move solver view");
    dvatest::check(contact.value("detailRows").toString().contains("Iteration:0:Shell"),
                   "Move command keeps iteration shell state visible");

    const QVariantMap gdt =
        model.commandDetail(model.indexOf("tolerances"), QStringLiteral("Add GD&T"));
    dvatest::check(gdt.value("viewType").toString() == QStringLiteral("gdt"),
                   "GD&T command uses a drafting control view");
    dvatest::check(gdt.value("matrixRows").toString().contains("Datum Shift"),
                   "GD&T command exposes datum shift rows");

    const QVariantMap gap =
        model.commandDetail(model.indexOf("measures"), QStringLiteral("Gap"));
    dvatest::check(gap.value("viewType").toString() == QStringLiteral("measure"),
                   "Gap measure command uses a measure builder view");

    const QVariantMap run =
        model.commandDetail(model.indexOf("simulation"), QStringLiteral("Run Analysis"));
    dvatest::check(run.value("viewType").toString() == QStringLiteral("simulation"),
                   "Run Analysis command uses a simulation view");
    dvatest::check(run.value("signals").toString().contains("HST/HLM"),
                   "Simulation command exposes HST/HLM output signal");
    dvatest::check(run.value("outputs").toString().contains("HST file"),
                   "Simulation command exposes HST output artifact");

    const QVariantMap contour =
        model.commandDetail(model.indexOf("visualization"), QStringLiteral("Color Contour"));
    dvatest::check(contour.value("viewType").toString() == QStringLiteral("viewport"),
                   "Color Contour command uses a viewport view");
    dvatest::check(contour.value("detailRows").toString().contains("Contour:318:Active"),
                   "Viewport command exposes contour overlay detail");
}

TEST("command execution model creates structured audit records") {
    opendva::ui::WorkbenchModel model;
    opendva::ui::CommandExecutionModel executor;

    const QVariantMap workspace = model.get(model.indexOf("simulation"));
    const QVariantMap command =
        model.commandDetail(model.indexOf("simulation"), QStringLiteral("Run Analysis"));
    const QVariantMap record =
        executor.execute(workspace, command, QStringLiteral("Run Analysis"));

    dvatest::check(record.value("id").toString() == QStringLiteral("EXEC-0001"),
                   "execution model creates a stable execution id");
    dvatest::check(record.value("workspace").toString() ==
                       QStringLiteral("simulation"),
                   "execution model records the workspace id");
    dvatest::check(record.value("command").toString() ==
                       QStringLiteral("Run Analysis"),
                   "execution model records the selected command");
    dvatest::check(record.value("action").toString() ==
                       QStringLiteral("Run Monte Carlo"),
                   "execution model records the command action");
    dvatest::check(record.value("riskLevel").toString() == QStringLiteral("Watch"),
                   "execution model derives command risk from model state");
    dvatest::check(record.value("outputCount").toInt() == 4,
                   "execution model counts declared output artifacts");
    dvatest::check(record.value("outputs").toString().contains(QStringLiteral("HST file")),
                   "execution model summarizes output artifacts");
    dvatest::check(executor.executionCount() == 1,
                   "execution model exposes the execution count");
}
