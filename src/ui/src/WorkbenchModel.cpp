#include "WorkbenchModel.h"

#include <initializer_list>

#include <QVariantMap>

namespace opendva::ui {
namespace {

QVariantMap mapFromWorkspace(const WorkbenchWorkspace& workspace) {
    return {{"id", workspace.id},
            {"title", workspace.title},
            {"iconSource", workspace.icon},
            {"status", workspace.status},
            {"surface", workspace.surface},
            {"summary", workspace.summary},
            {"commands", workspace.commands},
            {"gaps", workspace.gaps},
            {"workflow", workspace.workflow},
            {"primaryPanel", workspace.primaryPanel},
            {"matrixRows", workspace.matrixRows},
            {"editorFields", workspace.editorFields},
            {"metrics", workspace.metrics}};
}

QString commandDescription(const WorkbenchWorkspace& workspace,
                           const QString& command) {
    if (workspace.id == "modeling") {
        if (command == "Tree Link")
            return "Tree Link panel tracks CAD feature links, broken references, "
                   "extract/update status and validation warnings.";
        if (command == "Extract CAD" || command == "Update Model")
            return "CAD data management imports, updates and compares part, point "
                   "and feature payloads before nominal build.";
        if (command == "Validate")
            return "Validation checks missing references, point ownership, unit "
                   "consistency and display-set readiness.";
    } else if (workspace.id == "moves") {
        if (command == "Contact" || command == "Clamp" || command == "Float")
            return "MTM editor exposes object/target picks, direction control, "
                   "condition logic and solve state for the selected move family.";
        if (command == "Iterate")
            return "Iteration command prepares loop ordering, convergence limits "
                   "and pending solver state for repeated assembly moves.";
    } else if (workspace.id == "tolerances") {
        if (command == "Add GD&T" || command == "Datum Frame")
            return "GD&T editor binds feature controls to datum reference frames, "
                   "zone type, material condition and composite behavior.";
        if (command == "Bonus Shift" || command == "PCDB")
            return "Bonus/datum shift and PCDB material-process lookup remain "
                   "visible as incomplete but mapped workflows.";
    } else if (workspace.id == "measures") {
        if (command == "Equation" || command == "Combination")
            return "Measure builder composes derived outputs from point, feature, "
                   "GD&T and related-list inputs for simulation and reporting.";
        if (command == "Related List")
            return "Related-list view connects measures back to tolerances, moves "
                   "and reporting groups.";
    } else if (workspace.id == "simulation") {
        if (command == "Run Analysis")
            return "Runs the Monte Carlo analysis profile and stages HST/HLM "
                   "outputs for graphs, contributor analysis, reports and AAO.";
        if (command == "Contributor" || command == "Show Graph")
            return "Simulation window pivots between histogram, sample table, "
                   "GeoFactor and contributor-ranked analysis.";
    } else if (workspace.id == "visualization") {
        if (command == "Color Contour")
            return "Color contour overlays six-sigma deviation bands on the 3D "
                   "viewport and prepares captures for reports.";
        if (command == "Animate" || command == "Sweep")
            return "Animation timeline previews nominal build, assemble/separate, "
                   "deviate and sweep states.";
    } else if (workspace.id == "aao") {
        if (command == "GeoFactor")
            return "GeoFactor opens the GF2/HLM coefficient matrix used to relate "
                   "outputs to tolerance inputs.";
        if (command == "CTI")
            return "CTI ranks contributors with percent filtering, output weights "
                   "and matrix sensitivity columns.";
        if (command.contains("Optimizer") || command == "LSA" || command == "SOV")
            return "AAO optimization shell maps TO/SO/DO/LSA/SOV commands, export "
                   "formats and missing optimizer back-end integration.";
    } else if (workspace.id == "mechanical") {
        if (command == "DOF Counter")
            return "DOF Counter compares six-degree freedom state before and "
                   "after constraints, joints and mechanical moves.";
        if (command == "Collision" || command == "Distance")
            return "Mechanical batch checks collision and part-distance results "
                   "against constrained motion states.";
        if (command == "Constraints" || command == "Joints")
            return "Constraint and joint editors prepare planar, revolute and "
                   "kinematic definitions for the solver.";
    } else if (workspace.id == "fea") {
        if (command == "StiffGen")
            return "StiffGen creates ASET links and stiffness-matrix bindings for "
                   "flexible part simulation.";
        if (command == "Clamp" || command == "Join" || command == "Lock DOF")
            return "Compliant move editor binds clamp, join and locked DOF "
                   "operations to flexible parts and ASET nodes.";
        if (command == "Force" || command == "Thermal")
            return "Load editor maps force, thermal and gravity inputs, while the "
                   "validator keeps missing FEA data explicit.";
    } else if (workspace.id == "reports") {
        if (command.contains("Export"))
            return "Export command publishes analysis, tolerance and captured-view "
                   "pages to HTML, CSV or Excel deliverables.";
        if (command == "Generate Report" || command == "Capture View")
            return "Report generator binds simulation data, viewport captures and "
                   "conditional formatting into APQP-style pages.";
    } else if (workspace.id == "cad") {
        if (command == "PMI Extract" || command == "Update Links")
            return "CAD integration maps host assembly structure, PMI/GD&T, "
                   "constraints and feature links back into the 3DCS logic tree.";
        return command + " adapter keeps the host-specific import/update workflow "
               "visible while native CAD APIs remain unimplemented.";
    } else if (workspace.id == "userdll") {
        if (command.contains("Hook"))
            return "User-DLL hook mapping exposes the C ABI slot, function name, "
                   "argument contract and diagnostic state for external MTM logic.";
        return "User-DLL SDK registry manages external libraries, examples, "
               "diagnostics and readiness checks for custom automation.";
    } else if (workspace.id == "help") {
        if (command == "Formula Reference" || command == "GD&T Standard")
            return "Help panel provides quick engineering references for "
                   "statistics, GD&T interpretation and modeling assumptions.";
        return "Tutorial workspace keeps official help contents, index search, "
               "training models and workflow walkthroughs inside the product.";
    } else if (workspace.id == "system") {
        if (command == "License")
            return "License view separates module entitlement for Basic, AAO, "
                   "Mechanical and FEA Compliant capabilities.";
        if (command == "Shared Compute")
            return "Shared compute monitor shows thread, memory and distributed "
                   "queue configuration.";
    }

    return command + " is mapped from the 3DCS requirement document into the "
           + workspace.surface + " surface with gaps still visible.";
}

QString sourceChapter(const QString& workspaceId) {
    if (workspaceId == "modeling") return "Chapter 3: Features & Operations";
    if (workspaceId == "moves") return "Chapter 4: Moves";
    if (workspaceId == "tolerances") return "Chapter 5: Tolerances / GD&T";
    if (workspaceId == "measures") return "Chapter 6: Measures";
    if (workspaceId == "simulation") return "Chapter 7: Statistical Analysis";
    if (workspaceId == "visualization") return "Chapter 8: Visualization / Reports";
    if (workspaceId == "aao") return "Chapter 9: AAO";
    if (workspaceId == "mechanical") return "Chapter 10: Mechanical";
    if (workspaceId == "fea") return "Chapter 11: FEA Compliant";
    if (workspaceId == "reports") return "Chapter 8: Graphical Analysis / Reports";
    if (workspaceId == "cad") return "Chapter 12: CAD Integration";
    if (workspaceId == "userdll") return "Chapter 13: User DLL";
    if (workspaceId == "help") return "Chapter 14: Tutorials / Appendix";
    if (workspaceId == "system") return "Chapter 2: License / Shared Compute";
    return "doc/requirements mapping pending";
}

QString officialGuiSource(const WorkbenchWorkspace& workspace,
                          const QString& command) {
    if (workspace.id == "modeling")
        return "Model Navigator, Tree Link Wizard, Update/Extract, Validation";
    if (workspace.id == "moves")
        return "Move List and MTM move-family parameter dialogs";
    if (workspace.id == "tolerances")
        return "GD&T List, Tolerance List, Distribution, Datum/Bonus panels";
    if (workspace.id == "measures")
        return "Measure List, Equation/Combination builders, Related List";
    if (workspace.id == "simulation")
        return "Run Analysis dialog, Simulation Window, Graph/Samples/Contributor";
    if (workspace.id == "visualization")
        return "Nominal Build, Animation Window, Color Contour, Gap & Flush";
    if (workspace.id == "aao")
        return "GeoFactor Analyzer, CTI, SBS, TO/SO/DO/LSA matrices";
    if (workspace.id == "mechanical")
        return "Constraints, Joints, Kinematics, DOF Counter, Collision";
    if (workspace.id == "fea")
        return "StiffGen, Load FEA Data, Point Link Wizard, Compliant Validate";
    if (workspace.id == "reports")
        return "Report Generator, Spec Study, HTML/Excel/image export";
    if (workspace.id == "cad")
        return "CAD host adapter, PMI/GD&T extract, Tree Link update workflow";
    if (workspace.id == "userdll")
        return "User DLL SDK registration and Move/Tolerance/Measure hook dialogs";
    if (workspace.id == "help")
        return "Help Contents, Index, formula appendix, tutorial model launcher";
    if (workspace.id == "system")
        return "License Status, Shared Compute, Preferences, Diagnostics";
    return workspace.primaryPanel + " / " + command;
}

bool isOneOf(const QString& command, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (command == QLatin1String(name)) return true;
    }
    return false;
}

QVariantMap buildCommandDetail(const WorkbenchWorkspace& workspace,
                               const QString& command) {
    QVariantMap detail{{"title", command},
                       {"description", commandDescription(workspace, command)},
                       {"sourceChapter", sourceChapter(workspace.id)},
                       {"officialGui", officialGuiSource(workspace, command)},
                       {"requirementSource", "doc/需求文档.md + 3DCS official help"},
                       {"panel", workspace.primaryPanel},
                       {"matrixRows", workspace.matrixRows},
                       {"editorFields", workspace.editorFields},
                       {"metrics", workspace.metrics},
                       {"state", workspace.status},
                       {"action", "Open " + command + " workspace"},
                       {"prerequisites", "Model loaded|Feature tree valid|Unit system set"},
                       {"signals", "Selection|Validation|Report-ready output"},
                       {"viewType", "metrics"},
                       {"detailRows", workspace.metrics},
                       {"executeLabel", "Open " + command},
                       {"risk", workspace.gaps},
                       {"outputs", "Selection set|Validation log|Report-ready data"}};

    if (workspace.id == "moves") {
        detail["panel"] = "MTM Solver Card";
        detail["matrixRows"] =
            "Move,Object,Target,DOF,State;Six Plane,Bracket LH,Fixture A,6,Ready;"
            "Best Fit,Rail Inner,Locator Set,6,Ready;Contact,Hinge Reinforcement,Stop Block,1,Ready;"
            "Pattern,Door Ring,Pin Group,3,Preview;Auto Bend,Bracket Flange,Bend Line,Shell,Missing";
        detail["editorFields"] =
            "Move Family:" + command +
            "|Object:Bracket LH|Target:Fixture A|Direction:X/Y/Z|Condition:After previous move";
        detail["metrics"] = "Solved:18|Active DOF:1|Families:21|Compliant Moves:Shell";
        detail["state"] = "Move Solve Preview";
        detail["action"] = "Solve object-target relation and update nominal build order";
        detail["prerequisites"] = "Object picked|Target picked|Move order valid";
        detail["signals"] = "Solved DOF|Residual offset|Iteration state";
        detail["viewType"] = "move";
        detail["detailRows"] = "Six Plane:6:Ready|Best Fit:6:Ready|Contact:1:Ready|Pattern:3:Preview|Iteration:0:Shell|Auto Bend:0:Shell";
        detail["executeLabel"] = "Preview Solve";
        detail["risk"] = "Move-family editors and compliant AutoBend behavior are still partial.";
        detail["outputs"] = "Move order|Solved DOF|Nominal build update";
    } else if (workspace.id == "tolerances") {
        detail["panel"] = "GD&T Control Composer";
        detail["matrixRows"] =
            "Control,Feature,Datum,Zone,State;Position,Hole_A14,A-B-C,Diam 0.40,Ready;"
            "Profile,Surface_C,A-B,0.30,Ready;Flatness,Plane_B,None,0.12,Ready;"
            "Circularity,Cyl_07,None,0.08,Ready;Datum Shift,DRF A-B-C,MMC,Rule,Partial";
        detail["editorFields"] =
            "Control:" + command +
            "|Feature:Hole_A14|Datum Frame:A-B-C|Material:MMC|Bonus Shift:Enabled";
        detail["metrics"] = "Controls:42|Datum Frames:8|Bonus:Partial|PCDB:Missing";
        detail["state"] = "Drafting-grade Control Edit";
        detail["action"] = "Bind tolerance control to feature, datum frame and distribution";
        detail["prerequisites"] = "Feature selected|Datum frame available|Distribution selected";
        detail["signals"] = "Zone|Datum shift|HLM contribution";
        detail["viewType"] = "gdt";
        detail["detailRows"] = "Position:40:Ready|Profile:30:Ready|Flatness:12:Ready|Circularity:8:Ready|Datum Shift:50:Partial";
        detail["executeLabel"] = "Apply Control";
        detail["risk"] = "Composite datum behavior, PCDB lookup and bonus shift rules need deeper implementation.";
        detail["outputs"] = "GD&T control|Datum frame link|Tolerance contribution";
    } else if (workspace.id == "measures") {
        detail["panel"] = "Measure Definition Builder";
        detail["matrixRows"] =
            "Measure,Input A,Input B,Spec,State;Distance_010,Pt11,Pt12,+/-0.5,Ready;"
            "Angle_012,Plane A,Plane B,+/-1 deg,Ready;Gap_014,Pt21,Pt33,0.0..1.5,Ready;"
            "Flush_022,Plane C,Point 8,-0.5..0.5,Ready;"
            "Eqn_Stack_07,4 terms,Formula,User Formula,Shell;"
            "Combo_Closure,3 measures,Weighted,Report,Ready";
        detail["editorFields"] =
            "Measure Type:" + command +
            "|Input A:Pt21|Input B:Pt33|Direction:Projected U|Report Group:Closure";
        detail["metrics"] = "Measures:74|Spec Linked:58|Equations:6|DLL Hooks:2";
        detail["state"] = "Measurement Binding";
        detail["action"] = "Create analysis output and connect it to simulation/report groups";
        detail["prerequisites"] = "Inputs selected|Direction set|Spec limits reviewed";
        detail["signals"] = "Nominal value|Spec band|Report output";
        detail["viewType"] = "measure";
        detail["detailRows"] = "Distance_010:21:Ready|Angle_012:12:Ready|Gap_014:42:Ready|Flush_022:8:Ready|Eqn_Stack_07:4:Shell|Combo_Closure:3:Ready";
        detail["executeLabel"] = "Evaluate Measure";
        detail["risk"] = "Equation builder and User-DLL measure binding remain shell-level workflows.";
        detail["outputs"] = "Nominal value|Spec status|Simulation output";
    } else if (workspace.id == "simulation") {
        detail["panel"] = "Simulation Run Console";
        detail["matrixRows"] =
            "Output,Mean,Ppk,Contributor,State;Gap_014,0.42,1.42,Datum A2,Ready;"
            "Flush_022,-0.08,1.61,Tol_B,Ready;Profile_009,0.18,1.23,Tol_C,Ready;"
            "Hole_Pos_031,0.31,1.18,Locator P7,Warning";
        detail["editorFields"] =
            "Samples:50000|Seed:12345|Analysis:" + command +
            "|HST:staged|HLM:staged";
        detail["metrics"] = "Ppk:1.42|Worst:Datum A2|Runtime:18s|Queue:3";
        detail["state"] = "Monte Carlo Results Ready";
        detail["action"] = "Run or inspect Monte Carlo, graphs, samples and contributors";
        detail["prerequisites"] = "Nominal build solved|Tolerances active|Measures selected";
        detail["signals"] = "Histogram|Contributor|HST/HLM";
        detail["viewType"] = "simulation";
        detail["detailRows"] = "Gap_014:142:Ready|Flush_022:161:Ready|Profile_009:123:Ready|Hole_Pos_031:118:Warning";
        detail["executeLabel"] = "Run Monte Carlo";
        detail["risk"] = "Interactive graph controls and full HST/HLM management still need expansion.";
        detail["outputs"] = "Histogram|Samples table|HST file|HLM file";
    } else if (workspace.id == "visualization") {
        detail["panel"] = "3D View State Director";
        detail["matrixRows"] =
            "Layer,Mode,Frame,State;Nominal Build,Assembly,0,Ready;"
            "Assemble/Separate,Animation,24 frames,Ready;Deviate,Offset Vector,Active,Ready;"
            "Color Contour,6 Sigma,Active,Ready;Gap Section,Closure U,12 pts,Preview;"
            "Spec Study,Measure Overlay,4 bands,Partial;Sweep,X Direction,40 frames,Shell";
        detail["editorFields"] =
            "View Mode:" + command +
            "|Contour:6 Sigma|Section Plane:Closure U|Capture:Report Page";
        detail["metrics"] = "Frames:40|Contour Points:318|Report Captures:12|Spec Bands:4";
        detail["state"] = "Viewport Overlay Preview";
        detail["action"] = "Drive 3D overlays, animation frames and report captures";
        detail["prerequisites"] = "Model displayed|Result set loaded|View template selected";
        detail["signals"] = "Frame|Contour band|Capture state";
        detail["viewType"] = "viewport";
        detail["detailRows"] = "Nominal:0:Ready|Animate:24:Ready|Deviate:318:Active|Contour:318:Active|Gap Section:12:Preview|Spec Study:4:Partial|Sweep:40:Shell";
        detail["executeLabel"] = "Update View";
        detail["risk"] = "Animation state machine and report view templates remain partial.";
        detail["outputs"] = "Viewport overlay|Animation frames|Report capture";
    } else if (workspace.id == "aao" && command == "CTI") {
        detail["panel"] = "CTI Contributor Ranking";
        detail["matrixRows"] =
            "Contributor,Output,GeoFactor %,CTI Rank;Datum A2,Gap_014,33%,1;"
            "Tol_B,Flush_022,21%,2;Tol_C,Profile_009,14%,3;Locator P7,Hole_Pos_031,11%,4";
        detail["editorFields"] =
            "Percent Filter:5%|Output Weight:1.0|Ranking:Descending|Based On:GeoFactor %";
        detail["metrics"] = "Ranked:156|Visible:24|Top CTI:Datum A2|Optimizer:Shell";
        detail["state"] = "Matrix Ready / Optimizer Shell";
        detail["action"] = "Rank contributors and prepare TO/SO handoff";
        detail["prerequisites"] = "HLM/GF2 imported|Simulation outputs selected|Weights reviewed";
        detail["signals"] = "CTI rank|GeoFactor percent|Tolerance contribution";
        detail["viewType"] = "matrix";
        detail["detailRows"] = "Datum A2:33:1|Tol_B:21:2|Tol_C:14:3|Locator P7:11:4";
        detail["executeLabel"] = "Rank CTI";
        detail["risk"] = "AAO optimizers are mapped in the GUI, but solver back-end integration is still shell state.";
        detail["outputs"] = "CTI rank table|Top contributors|TO/SO handoff";
    } else if (workspace.id == "aao" && command == "GeoFactor") {
        detail["panel"] = "GeoFactor Matrix Analyzer";
        detail["editorFields"] =
            "Matrix Mode:Per Output|File:study.gf2|HLM Source:staged|Normalize:On";
        detail["state"] = "GF2/HLM Staged";
        detail["action"] = "Inspect output-to-tolerance coefficient matrix";
        detail["prerequisites"] = "Contributor analysis complete|HLM staged|Tolerance list valid";
        detail["signals"] = "Coefficient|Sensitivity|Rank";
        detail["viewType"] = "matrix";
        detail["detailRows"] = "Tol_A:82:Gap_014|Tol_B:76:Flush_022|Tol_C:64:Profile_009|Locator P7:51:Hole_Pos_031";
        detail["executeLabel"] = "Open GF Matrix";
        detail["risk"] = "GF2/HLM persistence and optimizer handoff still need real back-end integration.";
        detail["outputs"] = "GF2 matrix|Sensitivity columns|Ranked outputs";
    } else if (workspace.id == "aao" &&
               isOneOf(command, {"Tolerance Optimizer", "Sequence Optimizer", "Design Optimizer", "SBS", "LSA", "SOV"})) {
        detail["panel"] = "AAO Optimizer Scenario Board";
        detail["matrixRows"] =
            "Optimizer,Input,Objective,State;Tolerance Optimizer,Cost Table,Ppk >= 1.33,Shell;"
            "Sequence Optimizer,Move Order,Minimize Contribution,Shell;Design Optimizer,Locator Set,Reduce Variation,Shell;"
            "SBS,HLM Sweep,Best Sequence,Partial;LSA,Locator Sensitivity,Rank Points,Partial;SOV,Export Matrix,SOVA File,Shell";
        detail["editorFields"] =
            "Optimizer:" + command +
            "|Objective:Ppk target|Cost Source:Manual|Export:GF2/SOV/JOP/DOP";
        detail["metrics"] = "Scenarios:6|Inputs:156|Objectives:3|Solver:Shell";
        detail["state"] = "Optimizer Scenario Shell";
        detail["action"] = "Configure AAO optimization scenario and review missing solver handoff";
        detail["prerequisites"] = "GF2 matrix available|Cost/weight table reviewed|Output targets selected";
        detail["signals"] = "Objective state|Constraint status|Export package";
        detail["viewType"] = "optimizer";
        detail["detailRows"] = "TO:33:Shell|SO:28:Shell|DO:16:Shell|SBS:52:Partial|LSA:41:Partial|SOV:20:Shell";
        detail["executeLabel"] = "Stage Optimizer";
        detail["risk"] = "AAO optimizer algorithms and persistence remain planned work.";
        detail["outputs"] = "Optimizer setup|Cost table|GF2/SOV export plan";
    } else if (workspace.id == "mechanical" && command == "DOF Counter") {
        detail["panel"] = "Six-DOF Constraint Counter";
        detail["matrixRows"] =
            "Part,Tx,Ty,Tz,Rx,Ry,Rz,State;Door Ring,0,0,0,0,0,0,Fixed;"
            "Hinge Link,0,0,1,0,0,0,One DOF;Seal Carrier,1,1,1,0,0,0,Underconstrained";
        detail["editorFields"] =
            "Counter Mode:After Move|Drop DOF:Yes|Color Teal:Fixed|Color White:Free";
        detail["metrics"] = "Fixed:2|One DOF:1|Underconstrained:1|Warnings:2";
        detail["state"] = "Constraint Audit";
        detail["action"] = "Count remaining Tx/Ty/Tz/Rx/Ry/Rz freedom per part";
        detail["prerequisites"] = "Mechanical license|Move sequence solved|Constraints selected";
        detail["signals"] = "Fixed DOF|Free DOF|Underconstraint warning";
        detail["viewType"] = "dof";
        detail["detailRows"] = "Door Ring:0:Fixed|Hinge Link:1:One DOF|Seal Carrier:3:Underconstrained";
        detail["executeLabel"] = "Count DOF";
        detail["risk"] = "Constraint solving and kinematic playback are not implemented yet.";
        detail["outputs"] = "DOF table|Constraint warnings|Motion state";
    } else if (workspace.id == "mechanical") {
        detail["panel"] = "Mechanical Joint and Constraint Editor";
        detail["matrixRows"] =
            "Item,Type,Free DOF,State;Planar Constraint,Drop DOF,3,Shell;"
            "Revolute Joint,Hinge Axis,1,Shell;Prismatic Joint,Slider Axis,1,Shell;"
            "Gear Pair,Ratio,1,Missing;Collision Job,Part Set,0,Missing";
        detail["editorFields"] =
            "Mechanical Command:" + command +
            "|Joint Type:Revolute/Prismatic/Planar|Drop DOF:Case Match|Playback:Preview";
        detail["metrics"] = "Constraints:2|Joints:5|Free DOF:4|Collision Jobs:0";
        detail["state"] = "Mechanical Definition Shell";
        detail["action"] = "Define mechanical constraints, joints or motion checks";
        detail["prerequisites"] = "Mechanical license|Parts selected|Move sequence solved";
        detail["signals"] = "Joint DOF|Constraint rank|Collision status";
        detail["viewType"] = "dof";
        detail["detailRows"] = "Planar:3:Shell|Revolute:1:Shell|Prismatic:1:Shell|Gear:1:Missing|Collision:0:Missing";
        detail["executeLabel"] = "Preview Mechanism";
        detail["risk"] = "Mechanical solver, gears and kinematic playback are not implemented yet.";
        detail["outputs"] = "Joint definition|DOF audit|Motion/collision plan";
    } else if (workspace.id == "fea" && command == "StiffGen") {
        detail["panel"] = "StiffGen ASET Builder";
        detail["matrixRows"] =
            "ASET Item,Source,Nodes,State;Flexible Part,Bracket LH,128,Planned;"
            "Stiffness Matrix,bracket.k,0,Missing;Mass Matrix,bracket.mass,0,Optional;"
            "Mesh,bracket.op2,0,Missing";
        detail["editorFields"] =
            "Flexible Part:Bracket LH|ASET Nodes:128 planned|Stiffness Matrix:Missing|Validator:Blocked";
        detail["metrics"] = "ASET:0%|Matrices:0|Flexible Parts:1|Validation:Missing";
        detail["state"] = "Blocked / Missing FEA Data";
        detail["action"] = "Generate ASET mapping after stiffness and mesh files load";
        detail["prerequisites"] = "FEA Compliant license|OP2/K file loaded|Flexible part marked";
        detail["signals"] = "ASET coverage|Matrix link|Validation blockers";
        detail["viewType"] = "aset";
        detail["detailRows"] = "Flexible Part:128:Planned|Stiffness Matrix:0:Missing|Mass Matrix:0:Optional|Mesh:0:Missing";
        detail["executeLabel"] = "Generate ASET";
        detail["risk"] = "FEA compliant workflow is blocked until stiffness and mesh data models are implemented.";
        detail["outputs"] = "ASET map|Matrix links|Validation blockers";
    } else if (workspace.id == "fea") {
        detail["panel"] = "Compliant Load and Constraint Stack";
        detail["matrixRows"] =
            "FEA Operation,Data,Dependency,State;Load FEA,OP2/K files,Flexible Part,Missing;"
            "Clamp,ASET Nodes,Stiffness Matrix,Blocked;Join,Paired Nodes,Nominal Build,Blocked;"
            "Thermal,Load Vector,Temperature Delta,Missing;Force,Node Load,Constraint State,Missing;"
            "Validate,Compliant Rules,All Data,Blocked";
        detail["editorFields"] =
            "Operation:" + command +
            "|Flexible Part:Bracket LH|Matrix:Missing|Load Case:Thermal/Force/Gravity";
        detail["metrics"] = "Operations:8|Loads:0|Clamps:0|Validation:Blocked";
        detail["state"] = "Compliant Workflow Blocked";
        detail["action"] = "Stage compliant operation and expose missing FEA prerequisites";
        detail["prerequisites"] = "FEA Compliant license|Stiffness matrix loaded|ASET links validated";
        detail["signals"] = "Load state|Clamp state|Validation blockers";
        detail["viewType"] = "aset";
        detail["detailRows"] = "Load FEA:0:Missing|Clamp:0:Blocked|Join:0:Blocked|Thermal:0:Missing|Force:0:Missing|Validate:0:Blocked";
        detail["executeLabel"] = "Stage Compliant Step";
        detail["risk"] = "Compliant model data, stiffness matrices and load validators remain missing.";
        detail["outputs"] = "Compliant operation|Missing data report|Validation blockers";
    } else if (workspace.id == "reports" && command == "Generate Report") {
        detail["panel"] = "APQP Report Composer";
        detail["matrixRows"] =
            "Page,Input,Output,State;Cover,Model Summary,HTML,Ready;"
            "Analysis,Simulation Window,HTML/Excel,Ready;Tolerance,GD&T Stack,CSV,Partial;"
            "Capture,Viewport Images,PNG,Queued";
        detail["editorFields"] =
            "Template:APQP|Sort:Ppk ascending|Condition:Target yellow-green|Images:12 queued";
        detail["metrics"] = "Pages:4|Exports:3|Images:12|Templates:Partial";
        detail["state"] = "Publish Preview";
        detail["action"] = "Compose report pages and package HTML/CSV/Excel exports";
        detail["prerequisites"] = "Simulation results available|Viewport captures queued|Template selected";
        detail["signals"] = "Page state|Export format|Image queue";
        detail["viewType"] = "report";
        detail["detailRows"] = "Cover:HTML:Ready|Analysis:HTML/Excel:Ready|Tolerance:CSV:Partial|Capture:PNG:Queued";
        detail["executeLabel"] = "Generate Package";
        detail["risk"] = "PowerPoint/Google/APQP templates need richer composition before final parity.";
        detail["outputs"] = "HTML report|CSV export|Excel workbook|Image queue";
    } else if (workspace.id == "cad") {
        detail["panel"] = "CAD Adapter Link Board";
        detail["matrixRows"] =
            "Host,Assembly Link,PMI/GD&T,State;CATIA V5,Publication,FT&A,Planned;"
            "NX,Assembly Tree,PMI,Missing;Creo,Annotate,GD&T Advisor,Missing;"
            "SolidWorks,Feature Tree,DimXpert,Missing";
        detail["editorFields"] =
            "Host:" + command +
            "|Tree Link Mode:Exact/Minimum Name|PMI Scope:Used Features|Update:Creation Mode";
        detail["metrics"] = "Hosts:6|PMI Extractors:0|Broken Links:2|Adapter State:Missing";
        detail["state"] = "CAD Adapter Missing";
        detail["action"] = "Map CAD assembly, PMI and constraints into 3DCS model data";
        detail["prerequisites"] = "Host CAD session|Assembly loaded|Tree link strategy selected";
        detail["signals"] = "CAD tree|PMI controls|Broken references";
        detail["viewType"] = "cad";
        detail["detailRows"] = "CATIA V5:Publication:Planned|NX:PMI:Missing|Creo:GD&T Advisor:Missing|SolidWorks:DimXpert:Missing";
        detail["executeLabel"] = command == "Update Links" ? "Update Links" : "Map CAD Link";
        detail["risk"] = "Native CAD adapters, PMI extraction and constraint mapping are not implemented.";
        detail["outputs"] = "Tree link map|PMI extraction plan|Update diagnostics";
    } else if (workspace.id == "userdll") {
        detail["panel"] = "User DLL SDK Registry";
        detail["matrixRows"] =
            "Hook,ABI Slot,Function,State;Move,C ABI,DCS_MOVE,Shell;"
            "Tolerance,C ABI,DCS_TOL,Missing;Measure,C ABI,DCS_MEASURE,Shell;"
            "Diagnostics,Log,Last Error,Ready";
        detail["editorFields"] =
            "SDK Path:Not set|DLL:" + command +
            "|ABI:C17 bridge|Sandbox:Diagnostics only";
        detail["metrics"] = "Registered DLLs:0|Move Hooks:0|Tolerance Hooks:0|Measure Hooks:0";
        detail["state"] = "SDK Shell";
        detail["action"] = "Register external DLL metadata and validate exported hook signatures";
        detail["prerequisites"] = "DLL selected|Export names known|Model hook target selected";
        detail["signals"] = "ABI check|Hook binding|Diagnostics";
        detail["viewType"] = "sdk";
        detail["detailRows"] = "Move Hook:DCS_MOVE:Shell|Tolerance Hook:DCS_TOL:Missing|Measure Hook:DCS_MEASURE:Shell|Examples:SDK:Missing";
        detail["executeLabel"] = "Validate DLL";
        detail["risk"] = "PluginHost exists, but product-grade SDK registration and tolerance hook UI are incomplete.";
        detail["outputs"] = "Hook registry|ABI diagnostics|Example project state";
    } else if (workspace.id == "help") {
        detail["panel"] = "In-Product Help Center";
        detail["matrixRows"] =
            "Topic,Source,Mode,State;Contents,Official Help,Tree,Ready;"
            "Formula Reference,Appendix,Quick Card,Partial;GD&T Standard,ASME/ANSI,Guide,Partial;"
            "Tutorial Models,Training Lab,Project,Missing";
        detail["editorFields"] =
            "View:" + command +
            "|Search Scope:Contents + Index|Pinned:Modeling workflow|Offline:Doc bundle";
        detail["metrics"] = "Topics:14|Quick Cards:2|Tutorial Models:0|Search:Shell";
        detail["state"] = "Help Shell";
        detail["action"] = "Open contextual help, formula references and tutorial project guidance";
        detail["prerequisites"] = "Documentation indexed|Current workspace known|Example models available";
        detail["signals"] = "Help topic|Formula card|Tutorial handoff";
        detail["viewType"] = "help";
        detail["detailRows"] = "Contents:14:Ready|Index:Search:Shell|Formula Reference:2:Partial|Tutorial Models:0:Missing";
        detail["executeLabel"] = "Open Topic";
        detail["risk"] = "Built-in search, tutorial project launcher and offline help packaging need completion.";
        detail["outputs"] = "Help topic|Reference card|Tutorial launch plan";
    } else if (workspace.id == "system" && command == "License") {
        detail["panel"] = "Module Entitlement";
        detail["matrixRows"] =
            "Module,Required By,License,Effect;Basic,Modeling,Available,Ready;"
            "AAO,Optimizers,Missing,Commands Shell;Mechanical,DOF/Collision,Missing,Commands Shell;"
            "FEA CM,Compliant Model,Missing,Blocked";
        detail["state"] = "Mixed Entitlement";
        detail["action"] = "Review module availability before enabling advanced commands";
        detail["prerequisites"] = "License server reachable|User profile loaded|Module list refreshed";
        detail["signals"] = "Available|Missing|Blocked";
        detail["viewType"] = "license";
        detail["detailRows"] = "Basic:Available:Ready|AAO:Missing:Shell|Mechanical:Missing:Shell|FEA CM:Missing:Blocked";
        detail["executeLabel"] = "Refresh License";
        detail["risk"] = "Distributed queue monitoring and SDK panels are not complete.";
        detail["outputs"] = "Entitlement table|Module state|Diagnostics log";
    }

    return detail;
}

}  // namespace

WorkbenchModel::WorkbenchModel(QObject* parent) : QAbstractListModel(parent) {
    workspaces_ = {
        {"modeling",
         "Modeling",
         "qrc:/icons/feature.png",
         "Partial",
         "Model Navigator + Data Tools",
         "Assembly tree, point/feature creation, Tree Link, extract/update, "
         "validation and display preferences are organized as the modeling "
         "hub.",
         "New Model|Open|Save|Tree Link|Extract CAD|Update Model|Validate",
         "Tree Link wizard, Extract/Update data management, display preference "
         "pages still need complete product workflows.",
         "Import CAD|Build feature tree|Create points/features|Validate data|Run nominal build",
         "Tree Link Queue|Extracted CAD: 4 parts|Broken references: 2|Display sets: Nominal, Deviated, Color Contour",
         "Part,Points,Features,Status;Body Side Outer,128,36,Ready;Bracket LH,44,12,Needs Update;Rail Inner,87,18,Ready;Hinge Reinforcement,59,20,Tree Link",
         "Assembly:BIW_Master_Study|CAD Source:CATIA V5|Unit:mm|Validation:2 warnings|Display:Color Contour ready",
         "Nodes:318|Features:86|Warnings:2|Updated:92%"},
        {"moves",
         "Moves",
         "qrc:/icons/move.png",
         "Backend-led",
         "MTM Move Editor",
         "Twenty-one move families are exposed through one parameter editor "
         "with object/target picking, direction controls, condition logic and "
         "iteration state.",
         "Six Plane|Best Fit|Contact|Clamp|Float|Translate|Rotate|Pattern|Iterate|Auto Bend|Thermal Move|Conditional Logic",
         "Several solvers exist, but full QML editors for all move families, "
         "AutoBend and compliant moves remain open.",
         "Select move family|Pick object and target features|Solve 6DOF relation|Validate order|Run nominal build",
         "Move Sequence|01 Six-Plane root locator|02 Pin-Slot gravity settle|03 Contact over-closure|04 Iteration loop pending",
         "Move,Object,Target,DOF;Six Plane,Bracket LH,Fixture A,6;Pin Slot,Rail Inner,Locator Set,5;Contact,Hinge Reinforcement,Stop Block,1;Iterate,Door Ring,Closure Loop,Pending",
         "Move Type:Contact|Object Point:O_BKT_014|Target Point:T_FIX_021|Direction:X/Y/Z|Tolerance Link:Measure_014",
         "Solved:18|Families:21|Pending Editors:7|Iteration:Shell"},
        {"tolerances",
         "Tolerances / GD&T",
         "qrc:/icons/gdt.png",
         "Backend-led",
         "Tolerance Stack Editor",
         "Point tolerances, GD&T controls, distributions, datum frames, bonus "
         "shift and PCDB library access are grouped into a drafting-grade "
         "editor.",
         "Add GD&T|Point Tol|Distribution|Datum Frame|Bonus Shift|PCDB|Position|Profile|Flatness|Circularity|Runout|Datum Target",
         "Complete GD&T dialogs, PCDB management and composite/datum behavior "
         "need deeper implementation.",
         "Choose datum frame|Create point or feature tolerance|Bind distribution|Apply bonus shift|Verify HLM level",
         "GD&T Stack|Position composite controls|Datum Reference Frame A-B-C|PCDB material/process filters|Bonus and datum shift",
         "Control,Feature,Zone,Distribution;Position,Hole_A14,Diam 0.40,Normal;Flatness,Plane_B,0.12,Uniform;Profile,Surface_C,0.30,Normal;Datum Shift,DRF A-B-C,MMC,Rule",
         "Control:Position|Datum Frame:A-B-C|Zone:Diametric|Sigma:6|Bonus Shift:Enabled",
         "GD&T:42|Point Tol:114|PCDB Rows:0|Missing Pages:5"},
        {"measures",
         "Measures",
         "qrc:/icons/measure.png",
         "Backend-led",
         "Measure Builder",
         "Point, feature, GD&T, equation, combination and User-DLL measures "
         "share a builder with related-list and generator affordances.",
         "Distance|Angle|Flush|Gap|Diameter|Circle Interference|Equation|Combination|Related List|Measurement Generator|User-DLL Measure",
         "Equation builder, measurement generator and User-DLL measure binding "
         "are not yet complete.",
         "Select measure family|Pick points/features|Set spec limits|Evaluate nominal value|Bind reporting output",
         "Measure Builder|Gap and Flush generator|Equation composer|Related-list preview|User-DLL measure hook",
         "Measure,Inputs,Spec,State;Gap_014,Pt21-Pt33,0.0..1.5,Ready;Flush_022,Plane C/Point 8,-0.5..0.5,Ready;Eqn_Stack_07,4 terms,User Formula,Shell;DLL_Check_02,Plugin,External,Shell",
         "Measure Type:Gap|Direction:Projected U|LSL:0.00|USL:1.50|Report Group:Closure",
         "Measures:74|Spec Linked:58|Equations:6|DLL Hooks:2"},
        {"simulation",
         "Simulation",
         "qrc:/icons/run-simulation.png",
         "Partial",
         "Simulation Window",
         "Monte Carlo, contributor, GeoFactor, samples, histograms, HST/HLM "
         "files and batch processing are presented as one analysis console.",
         "Run Analysis|Settings|Show Graph|Samples|Contributor|GeoFactor|Worst Case|Batch|HST/HLM Files",
         "Interactive graph controls, full settings pages and HST/HLM "
         "management need expansion.",
         "Configure samples|Run Monte Carlo|Run Contributor/HLM|Open Simulation Window|Feed contour, report and AAO",
         "Simulation Window|Summary + Analysis Settings|Histogram and samples|Contributor table|HST/HLM file management",
         "Measure,MC Mean,GeoFactor,Contributor;Gap_014,0.42,0.39,33%;Flush_022,-0.08,-0.10,21%;Profile_009,0.18,0.17,14%;Hole_Pos_031,0.31,0.29,11%",
         "Samples:50000|Seed:12345|Pp:6|HST:staged|HLM:staged",
         "Ppk:1.42|Worst:Datum A2|Runtime:18s|Queue:3"},
        {"visualization",
         "Visualization",
         "qrc:/icons/color-contour.png",
         "Partial",
         "3D Viewport + Animation",
         "Nominal build, assemble/separate, deviate, sweep, color contour, "
         "gap/flush and spec-study tools converge in the central 3D viewport.",
         "Nominal Build|Assemble/Separate|Animate|Deviate|Sweep|Color Contour|Color Map Lines|Gap & Flush|Spec Study|Capture View",
         "Animation state machine, report-ready view templates and gap/flush "
         "workflows are still incomplete.",
         "Nominal build|Animate assemble/deviate/sweep|Apply color contour|Create gap/flush section|Capture report view",
         "3D Visualization|Color contour legend|Animation timeline|Gap & Flush section plane|Spec Study overlays",
         "Layer,Mode,Value,State;Nominal Build,Assembly,0.018 mm,Ready;Color Contour,6 Sigma,+/-0.75,Active;Gap Section,Plane U,12 pts,Preview;Sweep,X Direction,40 frames,Shell",
         "View Mode:Deviated|Contour:6 Sigma|Legend:Auto|Section Plane:Closure U|Capture:Report Page",
         "Frames:40|Contour Points:318|Report Captures:12|Spec Bands:4"},
        {"aao",
         "AAO",
         "qrc:/icons/variant.png",
         "Shell",
         "AAO Matrix Studio",
         "GeoFactor matrix, CTI, tolerance optimization, sequence optimization "
         "and location sensitivity are laid out as a matrix-first workspace.",
         "GeoFactor|CTI|Tolerance Optimizer|Sequence Optimizer|Design Optimizer|SBS|LSA|SOV|GF2 Export",
         "AAO data model, matrix persistence, optimizers and result charts "
         "need real back-end integration.",
         "Import HLM/GF2|Review GeoFactor matrix|Rank CTI contributors|Run TO/SO/DO optimizer|Export SOV/GF2/JOP/DOP",
         "AAO Matrix Studio|GeoFactor Analyzer|CTI ranked chart|Tolerance Optimizer cost table|SBS/LSA validation",
         "Output,Tol_A,Tol_B,Tol_C,Rank;Gap_014,0.82,0.35,0.18,1;Flush_022,0.21,0.76,0.14,2;Profile_009,0.11,0.22,0.64,3;Hole_Pos_031,0.44,0.18,0.51,4",
         "Matrix Mode:Per Tolerance|Percent Filter:5%|Weight:1.0|Based On:GeoFactor %|File:study.gf2",
         "Outputs:24|Inputs:156|Top CTI:Datum A2|Optimizer:Shell"},
        {"mechanical",
         "Mechanical",
         "qrc:/icons/batch-processor.png",
         "Shell",
         "Mechanical Constraint Lab",
         "Constraints, joints, kinematics, gears, DOF counter, collision and "
         "part distance tools receive a dedicated mechanical workspace.",
         "Constraints|Joints|DOF Counter|Kinematics|Collision|Distance|Gear|Drop DOF|Part Distance",
         "Constraint solving, joint editors, DOF matrix and kinematic playback "
         "are not implemented yet.",
         "Create constraints|Create joints|Run DOF counter|Check collision and part distance|Preview kinematic motion",
         "Mechanical Constraint Lab|Drop DOF options|Joint library|DOF Counter matrix|Collision and Part Distance batch",
         "Part,Before DOF,After Move,State;Door Ring,6,0,Teal;Hinge Link,6,1,White;Latch Pin,6,0,Teal;Seal Carrier,6,3,White",
         "Constraint:Planar|Joint:Revolute|Drop DOF:Yes|Contact:External|Solution:Exact",
         "Constrained:2|Underconstrained:2|Joints:5|Collision Jobs:0"},
        {"fea",
         "FEA Compliant",
         "qrc:/icons/user-dll.png",
         "Missing",
         "Compliant Model Workbench",
         "StiffGen, FEA mesh binding, clamp/join/lock DOF, force, thermal and "
         "gravity loads are represented as a compliant-model workflow.",
         "Load FEA|StiffGen|Clamp|UnClamp|Join|Lock DOF|Contact|Force|Thermal|Gravity|Displacement Scalar|Validate",
         "Flexible part data model, stiffness matrix handling, ASET binding "
         "and load validators remain missing.",
         "Mark flexible part|Load FEA data|Generate ASET with StiffGen|Apply Clamp/Join/LockDOF|Validate compliant model",
         "Compliant Model Workbench|Stiffness and mesh binding|ASET link monitor|Clamp/Join/LockDOF sequence|Force/Thermal/Gravity loads",
         "FEA Item,File,Link,State;Stiffness Matrix,bracket.k,Missing,Blocked;Mesh File,bracket.op2,Missing,Blocked;ASET Links,0/128,Missing,Blocked;Mass Matrix,bracket.mass,Optional,Missing",
         "Flexible Part:Bracket LH|Stiffness Matrix:Not loaded|Mesh:Not loaded|LockDOF:X/Y/Z|Validation:Blocked",
         "ASET:0%|Loads:0|Clamps:0|Validation:Missing"},
        {"reports",
         "Reports",
         "qrc:/icons/report.png",
         "Partial",
         "Report Generator",
         "HTML/CSV/Excel exports, APQP-style report structure, image capture "
         "and specification-study outputs are grouped as publish tools.",
         "Generate Report|Capture View|Export HTML|Export CSV|Export Excel",
         "PowerPoint/Google/APQP templates and richer report composition are "
         "still needed.",
         "Select pages|Bind Simulation Window data|Capture images|Apply conditional formatting|Export deliverables",
         "Report Generator|Cover and model summary pages|Analysis/Tolerance pages|Image capture queue|APQP and slide export backlog",
         "Page,Source,Template,State;Cover Page,Viewport,600x400,Ready;Model Summary,Model Tree,Word Template,Ready;Analysis Page,Simulation Window,Conditional,Ready;Tolerance Page,Tolerance Stack,Table,Partial",
         "Pages:Cover/Model/Analysis/Tolerance|Sort:Ppk ascending|Format:Target yellow-green|Export:HTML CSV Excel",
         "Pages:4|Images:12|Exports:3|Templates:Partial"},
        {"cad",
         "CAD Integration",
         "qrc:/icons/feature.png",
         "Missing",
         "CAD Adapter + PMI Extract",
         "NX, CATIA, Creo, SolidWorks, 3DEXPERIENCE and Multi-CAD adapters "
         "are separated into a host-link workspace for tree update, PMI/GD&T "
         "extract and constraint mapping.",
         "NX|CATIA|Creo|SolidWorks|3DEXPERIENCE|Multi-CAD|PMI Extract|Update Links",
         "Native CAD adapters, PMI extraction, Tree Link automation and PLM "
         "handoff are not implemented.",
         "Open host assembly|Link CAD tree to 3DCS tree|Extract PMI/GD&T|Update changed geometry|Validate references",
         "CAD Adapter Link Board|Tree Link Wizard|PMI/GD&T Extract|Constraint Mapper|PLM handoff",
         "Host,Assembly Link,PMI/GD&T,State;CATIA V5,Publication,FT&A,Planned;NX,Assembly Tree,PMI,Missing;Creo,Annotate,GD&T Advisor,Missing;SolidWorks,Feature Tree,DimXpert,Missing",
         "Host:CATIA V5|Tree Link:Exact Name|PMI Scope:Used Features|Update Mode:Creation|PLM:Not connected",
         "Hosts:6|PMI Extractors:0|Broken Links:2|Adapter State:Missing"},
        {"userdll",
         "User DLL / SDK",
         "qrc:/icons/user-dll.png",
         "Shell",
         "External Hook Registry",
         "C ABI registration, Move/Tolerance/Measure hook mapping, diagnostics "
         "and example DLL workflows are exposed as a dedicated SDK surface.",
         "Register DLL|Move Hook|Tolerance Hook|Measure Hook|Diagnostics|Examples",
         "PluginHost foundations exist, but SDK management, tolerance hooks and "
         "example panels still need product-grade GUI.",
         "Choose DLL|Validate exported symbols|Bind hook to Move/Tolerance/Measure|Run diagnostics|Open SDK example",
         "User DLL SDK Registry|Hook Binding Table|ABI Diagnostics|Example Project Launcher",
         "Hook,ABI Slot,Function,State;Move,C ABI,DCS_MOVE,Shell;Tolerance,C ABI,DCS_TOL,Missing;Measure,C ABI,DCS_MEASURE,Shell;Diagnostics,Log,Last Error,Ready",
         "SDK Path:Not set|DLL:Not loaded|ABI:C bridge|Last Error:None|Example:Missing",
         "Registered DLLs:0|Move Hooks:0|Tolerance Hooks:0|Measure Hooks:0"},
        {"help",
         "Help / Tutorials",
         "qrc:/icons/report.png",
         "Shell",
         "Help Center + Training Lab",
         "Official help contents, index search, formula references, GD&T "
         "standards and tutorial models become an in-product learning surface.",
         "Contents|Index|Formula Reference|GD&T Standard|Tutorial Models|Training Lab",
         "Built-in search, tutorial project launcher and offline help packaging "
         "are not complete.",
         "Browse help tree|Search index|Open formula card|Review GD&T standard|Launch tutorial model",
         "Help Center|Contents and Index|Formula Quick Cards|GD&T Standard Guide|Tutorial Model Launcher",
         "Topic,Source,Mode,State;Contents,Official Help,Tree,Ready;Formula Reference,Appendix,Quick Card,Partial;GD&T Standard,ASME/ANSI,Guide,Partial;Tutorial Models,Training Lab,Project,Missing",
         "Search:Contents + Index|Pinned:Modeling workflow|Offline Bundle:Doc|Tutorial Model:Missing",
         "Topics:14|Quick Cards:2|Tutorial Models:0|Search:Shell"},
        {"system",
         "System",
         "qrc:/icons/validate.png",
         "Shell",
         "License + Compute Center",
         "Licensing, module entitlement, shared memory, distributed compute, "
         "preferences and diagnostics sit in one system area.",
         "License|Shared Compute|Preferences|Diagnostics",
         "License source management, distributed queue monitoring and SDK "
         "example panels are split into dedicated workspaces where appropriate.",
         "Check license|Inspect module entitlement|Configure shared compute|Open preferences|Open diagnostics",
         "System Center|License status|Shared compute monitor|Preferences and diagnostics",
         "Module,License,State,Notes;Basic,Available,Ready,Core modeling;AAO,Missing,Shell,Optimizer disabled;Mechanical,Missing,Shell,DOF tools disabled;FEA CM,Missing,Missing,Compliant disabled",
         "License Server:Local|Threads:8|Shared Memory:On|Diagnostics:Ready|Preferences:Partial",
         "Modules:4|Licensed:1|Threads:8|Warnings:3"},
    };
}

int WorkbenchModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : workspaces_.size();
}

QVariant WorkbenchModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= workspaces_.size()) {
        return {};
    }

    const WorkbenchWorkspace& workspace = workspaces_.at(index.row());
    switch (role) {
        case IdRole:
            return workspace.id;
        case TitleRole:
            return workspace.title;
        case IconRole:
            return workspace.icon;
        case StatusRole:
            return workspace.status;
        case SurfaceRole:
            return workspace.surface;
        case SummaryRole:
            return workspace.summary;
        case CommandsRole:
            return workspace.commands;
        case GapsRole:
            return workspace.gaps;
        case WorkflowRole:
            return workspace.workflow;
        case PrimaryPanelRole:
            return workspace.primaryPanel;
        case MatrixRowsRole:
            return workspace.matrixRows;
        case EditorFieldsRole:
            return workspace.editorFields;
        case MetricsRole:
            return workspace.metrics;
        default:
            return {};
    }
}

QHash<int, QByteArray> WorkbenchModel::roleNames() const {
    return {{IdRole, "workspaceId"},
            {TitleRole, "title"},
            {IconRole, "iconSource"},
            {StatusRole, "status"},
            {SurfaceRole, "surface"},
            {SummaryRole, "summary"},
            {CommandsRole, "commands"},
            {GapsRole, "gaps"},
            {WorkflowRole, "workflow"},
            {PrimaryPanelRole, "primaryPanel"},
            {MatrixRowsRole, "matrixRows"},
            {EditorFieldsRole, "editorFields"},
            {MetricsRole, "metrics"}};
}

int WorkbenchModel::workspaceCount() const {
    return workspaces_.size();
}

const WorkbenchWorkspace* WorkbenchModel::findById(const QString& id) const {
    for (const WorkbenchWorkspace& workspace : workspaces_) {
        if (workspace.id == id) return &workspace;
    }
    return nullptr;
}

QVariantMap WorkbenchModel::get(int row) const {
    if (row < 0 || row >= workspaces_.size()) return {};
    return mapFromWorkspace(workspaces_.at(row));
}

int WorkbenchModel::indexOf(const QString& id) const {
    for (int i = 0; i < workspaces_.size(); ++i) {
        if (workspaces_.at(i).id == id) return i;
    }
    return -1;
}

QVariantMap WorkbenchModel::commandDetail(int row, const QString& command) const {
    if (row < 0 || row >= workspaces_.size()) return {};

    const WorkbenchWorkspace& workspace = workspaces_.at(row);
    const QString selectedCommand =
        command.trimmed().isEmpty() ? workspace.commands.section('|', 0, 0)
                                    : command.trimmed();
    return buildCommandDetail(workspace, selectedCommand);
}

}  // namespace opendva::ui
