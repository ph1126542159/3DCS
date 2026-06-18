import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1500
    height: 930
    visible: true
    minimumWidth: 1200
    minimumHeight: 760
    title: "OpenDVA - 3DCS Industrial Workbench"

    property int selectedWorkspace: 0
    property var workspace: workbenchModel.get(selectedWorkspace)
    property string selectedCommand: firstCommand(workspace.commands)
    property var commandView: workbenchModel.commandDetail(selectedWorkspace, selectedCommand)
    property int selectedCatalogRow: -1
    property var catalogNode: ({ title: "No official help node selected", group: "", uiSurface: "", href: "", summary: "" })
    property int executionCounter: 0
    property string lastExecution: "No command executed"
    property var lastExecutionRecord: ({ id: "EXEC-0000", status: "Idle", riskLevel: "None", outputCount: 0 })
    property var activityFeed: ["Ready: workbench loaded"]
    property color bg0: "#070a0d"
    property color bg1: "#0c1318"
    property color panel: "#111a20"
    property color panel2: "#16232b"
    property color line: "#2a3d47"
    property color textMain: "#eef8fb"
    property color textDim: "#8da3ad"
    property color cyan: "#35d7ff"
    property color amber: "#f0aa4c"
    property color danger: "#ff6d5d"

    function selectWorkspace(row) {
        if (row >= 0 && row < workbenchModel.workspaceCount) {
            selectedWorkspace = row
            workspace = workbenchModel.get(row)
            selectedCommand = firstCommand(workspace.commands)
            commandView = workbenchModel.commandDetail(selectedWorkspace, selectedCommand)
        }
    }

    function firstCommand(commands) {
        var items = String(commands).split("|")
        return items.length > 0 ? items[0] : ""
    }

    function selectCommand(commandName) {
        selectedCommand = String(commandName)
        commandView = workbenchModel.commandDetail(selectedWorkspace, selectedCommand)
    }

    function selectNavigatorCommand(row, commandName) {
        if (row < 0 || row >= workbenchModel.workspaceCount)
            return false
        selectWorkspace(row)
        selectCommand(commandName)
        return true
    }

    function goToWorkspace(workspaceId, commandName) {
        var row = workbenchModel.indexOf(workspaceId)
        if (row < 0)
            return false
        selectWorkspace(row)
        if (commandName && String(commandName).length > 0)
            selectCommand(commandName)
        return true
    }

    function workspaceIdForCatalogNode(node) {
        if (node.workspaceId && String(node.workspaceId).length > 0)
            return String(node.workspaceId)
        var group = String(node.group).toLowerCase()
        var surface = String(node.uiSurface).toLowerCase()
        var title = String(node.title).toLowerCase()
        if (group.indexOf("aao") >= 0 || title.indexOf("optimizer") >= 0 ||
                title.indexOf("geofactor") >= 0 || title.indexOf("cti") >= 0)
            return "aao"
        if (group.indexOf("mechanical") >= 0 || title.indexOf("dof") >= 0 ||
                title.indexOf("joint") >= 0 || title.indexOf("kinematic") >= 0)
            return "mechanical"
        if (group.indexOf("fea") >= 0 && (title.indexOf("stiff") >= 0 ||
                title.indexOf("compliant") >= 0 || title.indexOf("thermal") >= 0 ||
                title.indexOf("gravity") >= 0 || title.indexOf("aset") >= 0))
            return "fea"
        if (group.indexOf("cad") >= 0 || title.indexOf("catia") >= 0 ||
                title.indexOf("nx") >= 0 || title.indexOf("creo") >= 0 ||
                title.indexOf("solidworks") >= 0 || title.indexOf("multi-cad") >= 0)
            return "cad"
        if (group.indexOf("moves") >= 0 || surface.indexOf("move") >= 0 ||
                title.indexOf("move") >= 0 || title.indexOf("clamp") >= 0 ||
                title.indexOf("contact") >= 0)
            return "moves"
        if (group.indexOf("tolerance") >= 0 || group.indexOf("gd&t") >= 0 ||
                surface.indexOf("tolerance") >= 0 || title.indexOf("datum") >= 0 ||
                title.indexOf("gdt") >= 0 || title.indexOf("gd&t") >= 0)
            return "tolerances"
        if (group.indexOf("measure") >= 0 || surface.indexOf("measure") >= 0 ||
                title.indexOf("measure") >= 0 || title.indexOf("gap") >= 0 ||
                title.indexOf("flush") >= 0)
            return "measures"
        if (group.indexOf("simulation") >= 0 || surface.indexOf("analysis") >= 0 ||
                title.indexOf("analysis") >= 0 || title.indexOf("samples") >= 0 ||
                title.indexOf("histogram") >= 0)
            return "simulation"
        if (surface.indexOf("report") >= 0 || title.indexOf("report") >= 0 ||
                title.indexOf("export") >= 0)
            return "reports"
        if (surface.indexOf("system") >= 0 || title.indexOf("license") >= 0 ||
                title.indexOf("shared") >= 0 || title.indexOf("preferences") >= 0)
            return "system"
        if (title.indexOf("help") >= 0 || group.indexOf("appendix") >= 0 ||
                title.indexOf("tutorial") >= 0)
            return "help"
        return "modeling"
    }

    function commandForCatalogNode(node) {
        if (node.command && String(node.command).length > 0)
            return String(node.command)
        var title = String(node.title)
        var lower = title.toLowerCase()
        if (lower.indexOf("tree link") >= 0)
            return "Tree Link"
        if (lower.indexOf("update model") >= 0)
            return "Update Model"
        if (lower.indexOf("extract") >= 0)
            return "Extract CAD"
        if (lower.indexOf("validate") >= 0)
            return "Validate"
        if (lower.indexOf("stiffgen") >= 0)
            return "StiffGen"
        if (lower.indexOf("tolerance optimizer") >= 0)
            return "Tolerance Optimizer"
        if (lower.indexOf("sequence optimizer") >= 0)
            return "Sequence Optimizer"
        if (lower.indexOf("geofactor") >= 0)
            return "GeoFactor"
        if (lower.indexOf("cti") >= 0)
            return "CTI"
        if (lower.indexOf("color contour") >= 0)
            return "Color Contour"
        if (lower.indexOf("run analysis") >= 0)
            return "Run Analysis"
        if (lower.indexOf("dof") >= 0)
            return "DOF Counter"
        return title
    }

    function selectCatalogNode(row) {
        if (row < 0 || row >= featureCatalog.nodeCount)
            return false
        selectedCatalogRow = row
        catalogNode = featureCatalog.get(row)
        var workspaceId = workspaceIdForCatalogNode(catalogNode)
        return goToWorkspace(workspaceId, commandForCatalogNode(catalogNode))
    }

    function activeMatrixRows() {
        return commandView.matrixRows && commandView.matrixRows.length > 0
                ? String(commandView.matrixRows).split(";")
                : String(workspace.matrixRows).split(";")
    }

    function activeEditorFields() {
        return commandView.editorFields && commandView.editorFields.length > 0
                ? String(commandView.editorFields).split("|")
                : String(workspace.editorFields).split("|")
    }

    function activeMetrics() {
        return commandView.metrics && commandView.metrics.length > 0
                ? String(commandView.metrics).split("|")
                : String(workspace.metrics).split("|")
    }

    function activePrerequisites() {
        return commandView.prerequisites && commandView.prerequisites.length > 0
                ? String(commandView.prerequisites).split("|")
                : []
    }

    function activeSignals() {
        return commandView.signals && commandView.signals.length > 0
                ? String(commandView.signals).split("|")
                : []
    }

    function activeOutputs() {
        return commandView.outputs && commandView.outputs.length > 0
                ? String(commandView.outputs).split("|")
                : []
    }

    function executeCommand() {
        var record = commandExecutor.execute(workspace, commandView, selectedCommand)
        executionCounter = commandExecutor.executionCount
        lastExecutionRecord = record
        var outputs = record.outputs ? String(record.outputs) : ""
        lastExecution = "#" + executionCounter + " " + record.workspaceTitle + " / " +
                record.command + " -> " + record.action
        var entry = lastExecution + " | outputs: " + outputs
        var nextFeed = [entry]
        for (var i = 0; i < Math.min(activityFeed.length, 4); ++i)
            nextFeed.push(activityFeed[i])
        activityFeed = nextFeed
        return entry
    }

    function activeDetailRows() {
        return commandView.detailRows && commandView.detailRows.length > 0
                ? String(commandView.detailRows).split("|")
                : root.activeMetrics()
    }

    function detailCell(row, column) {
        var cells = String(row).split(":")
        return column < cells.length ? cells[column] : ""
    }

    function statusColor(status) {
        if (status === "Partial")
            return cyan
        if (status === "Backend-led")
            return "#8fe388"
        if (status === "Shell")
            return amber
        if (status === "Missing")
            return danger
        return "#b7c8d0"
    }

    function viewportAccent(viewType) {
        if (viewType === "matrix")
            return "#9dd7ff"
        if (viewType === "optimizer")
            return "#ffcf6b"
        if (viewType === "gdt")
            return "#f0aa4c"
        if (viewType === "simulation")
            return "#35d7ff"
        if (viewType === "viewport")
            return "#8fe388"
        if (viewType === "cad")
            return "#6fb7ff"
        if (viewType === "sdk")
            return "#d1a4ff"
        if (viewType === "help")
            return "#b7c8d0"
        if (viewType === "aset")
            return danger
        return cyan
    }

    function rowCells(row) {
        return String(row).split(",")
    }

    function fieldName(field) {
        var parts = String(field).split(":")
        return parts.length > 0 ? parts[0] : ""
    }

    function fieldValue(field) {
        var parts = String(field).split(":")
        if (parts.length <= 1)
            return ""
        parts.shift()
        return parts.join(":")
    }

    background: Rectangle {
        color: bg0
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#111a20" }
            GradientStop { position: 0.45; color: "#071014" }
            GradientStop { position: 1.0; color: "#0b0d10" }
        }
    }

    header: Rectangle {
        height: 128
        color: "#0b1116"
        border.color: "#253942"

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            anchors.topMargin: 12
            anchors.bottomMargin: 10
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                Image {
                    source: "qrc:/icons/run-simulation.png"
                    sourceSize.width: 42
                    sourceSize.height: 42
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text {
                        text: "OpenDVA 3DCS Workbench"
                        color: textMain
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "Qt6 QML + Qt3D ready interface for modeling, MTM, simulation, AAO, Mechanical, FEA and reporting"
                        color: textDim
                        font.pixelSize: 12
                    }
                }

                Button {
                    text: "Run Analysis"
                    icon.source: "qrc:/icons/run-simulation.png"
                    onClicked: root.goToWorkspace("simulation", "Run Analysis")
                }
                Button {
                    text: "Validate"
                    icon.source: "qrc:/icons/validate.png"
                    onClicked: root.goToWorkspace("modeling", "Validate")
                }
                Button {
                    text: "Report"
                    icon.source: "qrc:/icons/report.png"
                    onClicked: root.goToWorkspace("reports", "Generate Report")
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: "#101921"
                radius: 4
                border.color: "#263b45"

                ListView {
                    id: ribbon
                    anchors.fill: parent
                    anchors.margins: 5
                    orientation: ListView.Horizontal
                    spacing: 6
                    clip: true
                    model: workbenchModel
                    currentIndex: root.selectedWorkspace

                    delegate: Button {
                        required property int index
                        required property string title
                        required property string iconSource
                        required property string status

                        width: Math.max(118, title.length * 10 + 46)
                        height: 38
                        text: title
                        icon.source: iconSource
                        highlighted: index === root.selectedWorkspace
                        onClicked: root.selectWorkspace(index)
                        background: Rectangle {
                            radius: 4
                            color: index === root.selectedWorkspace ? "#17313d" : "#121d25"
                            border.color: index === root.selectedWorkspace ? cyan : "#2a3b44"
                        }
                        contentItem: RowLayout {
                            spacing: 7
                            Image {
                                source: iconSource
                                sourceSize.width: 18
                                sourceSize.height: 18
                            }
                            Text {
                                text: title
                                color: index === root.selectedWorkspace ? textMain : "#b8c8cf"
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                width: 7
                                height: 7
                                radius: 3
                                color: root.statusColor(status)
                            }
                        }
                    }
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        Rectangle {
            Layout.preferredWidth: 278
            Layout.fillHeight: true
            radius: 5
            color: panel
            border.color: line

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Model Navigator"
                        color: textMain
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    ToolButton { text: "+"; width: 28; height: 28 }
                }

                ListView {
                    id: navigator
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: workbenchModel
                    currentIndex: root.selectedWorkspace
                    delegate: Rectangle {
                        required property int index
                        required property string title
                        required property string surface
                        required property string status
                        required property string metrics
                        required property string commands
                        property int workspaceRow: index
                        width: ListView.view.width
                        height: index === root.selectedWorkspace ? 48 + Math.min(8, String(commands).split("|").length) * 24 + 10 : 48
                        radius: 4
                        color: index === root.selectedWorkspace ? "#182832" : "#0d151b"
                        border.color: index === root.selectedWorkspace ? cyan : "#20323c"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4

                            Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 32

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: root.selectWorkspace(index)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 8
                                    Rectangle {
                                        width: 8
                                        height: 28
                                        radius: 3
                                        color: root.statusColor(status)
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            text: title
                                            color: textMain
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }
                                        Text {
                                            text: surface
                                            color: textDim
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                    Text {
                                        text: root.fieldValue(metrics).length > 0 ? root.fieldValue(metrics) : status
                                        color: root.statusColor(status)
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideRight
                                        Layout.preferredWidth: 58
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                visible: index === root.selectedWorkspace

                                Repeater {
                                    model: String(commands).split("|").slice(0, 8)
                                    delegate: Rectangle {
                                        required property string modelData
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 21
                                        radius: 3
                                        color: root.selectedCommand === modelData ? "#17313d" : "#101921"
                                        border.color: root.selectedCommand === modelData ? cyan : "#28414c"

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: root.selectNavigatorCommand(workspaceRow, modelData)
                                        }

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 20
                                            anchors.rightMargin: 8
                                            spacing: 6
                                            Rectangle {
                                                width: 6
                                                height: 6
                                                radius: 3
                                                color: root.selectedCommand === modelData ? cyan : textDim
                                            }
                                            Text {
                                                text: modelData
                                                color: root.selectedCommand === modelData ? textMain : "#b8c8cf"
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    text: "Official Help Tree"
                    color: cyan
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }

                ListView {
                    id: officialHelpTree
                    objectName: "officialHelpTree"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                    clip: true
                    spacing: 3
                    model: featureCatalog
                    currentIndex: root.selectedCatalogRow

                    delegate: Rectangle {
                        required property int index
                        required property int level
                        required property string title
                        required property string group
                        required property string uiSurface
                        required property string href

                        width: ListView.view.width
                        height: 26
                        radius: 3
                        color: index === root.selectedCatalogRow ? "#17313d" : "#0d151b"
                        border.color: index === root.selectedCatalogRow ? cyan : "#20323c"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.selectCatalogNode(index)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8 + Math.min(28, Math.max(0, level - 1) * 9)
                            anchors.rightMargin: 8
                            spacing: 6
                            Rectangle {
                                width: 5
                                height: 5
                                radius: 2
                                color: index === root.selectedCatalogRow ? amber : textDim
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text {
                                    text: title
                                    color: index === root.selectedCatalogRow ? textMain : "#b8c8cf"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: group + " / " + uiSurface
                                    color: textDim
                                    font.pixelSize: 8
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 118
                    radius: 4
                    color: "#0b1217"
                    border.color: "#263842"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        Text { text: "Coverage Gate"; color: cyan; font.pixelSize: 12; font.weight: Font.DemiBold }
                        Text {
                            text: featureCatalog.nodeCount + " official help-tree nodes are routable into the QML workbench."
                            color: amber
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: workbenchModel.workspaceCount + " workspaces mapped from doc/需求文档.md; status remains visible for incomplete 3DCS features."
                            color: "#b8c8cf"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        Text {
                            text: catalogNode.title + (catalogNode.href ? " / " + catalogNode.href : "")
                            color: textDim
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 5
                color: "#091015"
                border.color: "#2b4652"

                IndustrialViewport3D {
                    id: viewport
                    objectName: "industrialQt3DViewport"
                    anchors.fill: parent
                    anchors.margins: 1
                    accentColor: root.viewportAccent(commandView.viewType ? commandView.viewType : "metrics")
                    warningColor: root.statusColor(workspace.status)
                    panelColor: "#091015"
                    modeLabel: selectedCommand
                    viewType: commandView.viewType ? commandView.viewType : "metrics"
                    statusLabel: commandView.state ? commandView.state : workspace.status
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 18
                    width: 420
                    height: 96
                    radius: 4
                    color: "#101921dd"
                    border.color: "#2b4652"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                Image { source: workspace.iconSource; sourceSize.width: 24; sourceSize.height: 24 }
                            Text {
                                text: workspace.title + " / " + workspace.surface
                                color: textMain
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }
                        Text {
                            text: workspace.summary
                            color: "#bdd0d7"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 18
                    width: Math.min(parent.width - 36, 660)
                    height: 82
                    radius: 4
                    color: "#101921dd"
                    border.color: "#2b4652"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4
                        Text {
                            text: commandView.panel ? commandView.panel : workspace.primaryPanel
                            color: cyan
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: commandView.description ? commandView.description : workspace.workflow
                            color: "#bdd0d7"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }

                Column {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 18
                    spacing: 7
                    Repeater {
                        model: ["3D", "MTM", "CC", "HST", "HLM"]
                        delegate: Rectangle {
                            width: 52
                            height: 26
                            radius: 3
                            color: index === 0 ? "#163442" : "#101921dd"
                            border.color: index === 0 ? cyan : "#314853"
                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: index === 0 ? textMain : textDim
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 320
                radius: 5
                color: panel
                border.color: line

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: workspace.surface + " / " + selectedCommand
                            color: textMain
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }
                        Repeater {
                            model: String(workspace.workflow).split("|")
                            delegate: Button {
                                text: modelData
                                height: 30
                                flat: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100
                        radius: 4
                        color: "#0b1217"
                        border.color: "#263842"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 9
                            spacing: 10

                            ColumnLayout {
                                Layout.preferredWidth: 190
                                spacing: 2
                                Text {
                                    text: "Execution Stack"
                                    color: cyan
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    text: commandView.state ? commandView.state : workspace.status
                                    color: root.statusColor(workspace.status)
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: lastExecutionRecord.id + " / risk " +
                                          lastExecutionRecord.riskLevel + " / outputs " +
                                          lastExecutionRecord.outputCount
                                    color: textDim
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Text {
                                        text: commandView.action ? commandView.action : "Open workspace"
                                        color: textMain
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    Button {
                                        text: commandView.executeLabel ? commandView.executeLabel : "Open"
                                        height: 26
                                        flat: true
                                        onClicked: root.executeCommand()
                                        background: Rectangle {
                                            radius: 4
                                            color: "#17313d"
                                            border.color: cyan
                                        }
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    Repeater {
                                        model: root.activePrerequisites()
                                        delegate: Rectangle {
                                            required property string modelData
                                            Layout.preferredHeight: 20
                                            Layout.preferredWidth: Math.min(150, Math.max(74, modelData.length * 7 + 18))
                                            radius: 3
                                            color: "#13232b"
                                            border.color: "#2f4650"
                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData
                                                color: "#b8c8cf"
                                                font.pixelSize: 10
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                                Text {
                                    text: commandView.risk ? commandView.risk : workspace.gaps
                                    color: amber
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: root.lastExecution
                                    color: textDim
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            ColumnLayout {
                                Layout.preferredWidth: 170
                                spacing: 2
                                Text {
                                    text: "Output Signals"
                                    color: textDim
                                    font.pixelSize: 10
                                }
                                Text {
                                    text: root.activeSignals().join(" / ")
                                    color: textMain
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 42
                                    spacing: 4
                                    Repeater {
                                        model: root.activeOutputs()
                                        delegate: Rectangle {
                                            required property string modelData
                                            width: Math.min(148, Math.max(62, modelData.length * 6 + 14))
                                            height: 18
                                            radius: 3
                                            color: "#111d24"
                                            border.color: "#33505c"
                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData
                                                color: "#b8c8cf"
                                                font.pixelSize: 9
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        columns: 1
                        columnSpacing: 8
                        rowSpacing: 8

                        Repeater {
                            model: root.activeMatrixRows()
                            delegate: Rectangle {
                                required property int index
                                required property string modelData
                                property var cells: root.rowCells(modelData)
                                property bool isHeader: index === 0
                                Layout.fillWidth: true
                                Layout.preferredHeight: isHeader ? 30 : 38
                                radius: 4
                                color: isHeader ? "#172832" : "#0b1217"
                                border.color: isHeader ? cyan : "#263842"
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 8
                                    Repeater {
                                        model: cells
                                        delegate: Text {
                                            required property string modelData
                                            text: modelData
                                            color: isHeader ? textMain : "#c6d7de"
                                            font.pixelSize: isHeader ? 11 : 12
                                            font.weight: isHeader ? Font.DemiBold : Font.Normal
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 326
            Layout.fillHeight: true
            radius: 5
            color: panel2
            border.color: line

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Image { source: workspace.iconSource; sourceSize.width: 30; sourceSize.height: 30 }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: workspace.title
                            color: textMain
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: workspace.status
                            color: root.statusColor(workspace.status)
                            font.pixelSize: 12
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    radius: 4
                    color: "#0b1217"
                    border.color: "#263842"
                    Text {
                        anchors.fill: parent
                        anchors.margins: 10
                        text: workspace.gaps
                        color: "#c5d7de"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }

                Text { text: "Command Surface"; color: cyan; font.pixelSize: 13; font.weight: Font.DemiBold }
                Flow {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 138
                    spacing: 7
                    Repeater {
                        model: String(workspace.commands).split("|")
                        delegate: Button {
                            required property string modelData
                            text: modelData
                            height: 31
                            flat: true
                            highlighted: root.selectedCommand === modelData
                            onClicked: root.selectCommand(modelData)
                            background: Rectangle {
                                radius: 4
                                color: root.selectedCommand === modelData ? "#17313d" : "#101921"
                                border.color: root.selectedCommand === modelData ? cyan : "#2a3b44"
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 142
                    radius: 4
                    color: "#0b1217"
                    border.color: "#263842"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 4
                        Text {
                            text: commandView.panel ? commandView.panel : workspace.primaryPanel
                            color: textMain
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: commandView.description ? commandView.description : workspace.summary
                            color: "#c5d7de"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Text {
                            text: commandView.sourceChapter ? commandView.sourceChapter + " / " + commandView.officialGui : "Requirement source pending"
                            color: amber
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: activityFeed.length > 0 ? activityFeed[0] : "Ready"
                            color: textDim
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }
                }

                Text { text: "MTM / Property Editor"; color: cyan; font.pixelSize: 13; font.weight: Font.DemiBold }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    rowSpacing: 8
                    columnSpacing: 8

                    Repeater {
                        model: root.activeEditorFields()
                        delegate: Rectangle {
                            required property string modelData
                            Layout.fillWidth: true
                            Layout.preferredHeight: 54
                            radius: 4
                            color: "#0b1217"
                            border.color: "#263842"
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 2
                                Text {
                                    text: root.fieldName(modelData)
                                    color: textDim
                                    font.pixelSize: 10
                                }
                                Text {
                                    text: root.fieldValue(modelData)
                                    color: textMain
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }

                Text {
                    text: "Specialized Workspace / " + (commandView.viewType ? commandView.viewType : "metrics")
                    color: cyan
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 4
                    color: "#0b1217"
                    border.color: "#263842"
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 7
                        Repeater {
                            model: root.activeDetailRows()
                            delegate: RowLayout {
                                required property string modelData
                                property string label: commandView.viewType === "metrics" ? root.fieldName(modelData) : root.detailCell(modelData, 0)
                                property string rawValue: commandView.viewType === "metrics" ? root.fieldValue(modelData) : root.detailCell(modelData, 1)
                                property string stateValue: commandView.viewType === "metrics" ? root.fieldValue(modelData) : root.detailCell(modelData, 2)
                                property real barValue: {
                                    var n = Number(String(rawValue).replace("%", ""))
                                    if (isNaN(n))
                                        return 0.55
                                    if (String(rawValue).indexOf("%") >= 0 || commandView.viewType === "matrix")
                                        return Math.max(0.04, Math.min(1.0, n / 100.0))
                                    if (commandView.viewType === "optimizer")
                                        return Math.max(0.08, Math.min(1.0, n / 60.0))
                                    if (commandView.viewType === "dof")
                                        return Math.max(0.08, Math.min(1.0, n / 6.0))
                                    if (commandView.viewType === "move")
                                        return Math.max(0.08, Math.min(1.0, n / 6.0))
                                    if (commandView.viewType === "gdt")
                                        return Math.max(0.08, Math.min(1.0, n / 50.0))
                                    if (commandView.viewType === "measure")
                                        return Math.max(0.08, Math.min(1.0, n / 50.0))
                                    if (commandView.viewType === "simulation")
                                        return Math.max(0.08, Math.min(1.0, n / 200.0))
                                    if (commandView.viewType === "viewport")
                                        return Math.max(0.08, Math.min(1.0, n / 320.0))
                                    if (commandView.viewType === "cad")
                                        return Math.max(0.08, Math.min(1.0, n / 6.0))
                                    if (commandView.viewType === "sdk")
                                        return Math.max(0.08, Math.min(1.0, n / 4.0))
                                    if (commandView.viewType === "help")
                                        return Math.max(0.08, Math.min(1.0, n / 14.0))
                                    return Math.max(0.04, Math.min(1.0, n / 128.0))
                                }
                                Layout.fillWidth: true
                                spacing: 8
                                Text {
                                    text: label
                                    color: textDim
                                    font.pixelSize: 11
                                    Layout.preferredWidth: 88
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 8
                                    radius: 4
                                    color: "#18262d"
                                    Rectangle {
                                        width: parent.width * barValue
                                        height: parent.height
                                        radius: 4
                                        color: rawValue === "Missing" || rawValue === "Shell" ||
                                               stateValue === "Missing" || stateValue === "Blocked" ? danger :
                                               stateValue === "Partial" || stateValue === "Warning" ||
                                               stateValue === "Preview" || stateValue === "Shell" ? amber : cyan
                                    }
                                }
                                Text {
                                    text: commandView.viewType === "metrics" ? rawValue : stateValue
                                    color: textMain
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignRight
                                    elide: Text.ElideRight
                                    Layout.preferredWidth: 82
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
