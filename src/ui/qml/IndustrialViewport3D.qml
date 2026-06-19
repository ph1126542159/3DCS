import QtQuick
import QtQuick.Scene3D
import Qt3D.Core
import Qt3D.Render
import Qt3D.Input
import Qt3D.Extras

Item {
    id: root
    property string accentColor: "#35d7ff"
    property string warningColor: "#f0aa4c"
    property string panelColor: "#091015"
    property string modeLabel: "Nominal Build"
    property string viewType: "metrics"
    property string statusLabel: "Ready"
    property real activityPulse: 0.0
    property real scanPhase: 0.0
    property real motionScale: viewType === "simulation" || viewType === "viewport" ? 1.8
                               : viewType === "move" || viewType === "cad" ? 1.1
                               : viewType === "matrix" || viewType === "gdt" ? 0.75
                               : 0.45

    SequentialAnimation on activityPulse {
        loops: Animation.Infinite
        NumberAnimation { from: -1.0; to: 1.0; duration: 2600; easing.type: Easing.InOutSine }
        NumberAnimation { from: 1.0; to: -1.0; duration: 2600; easing.type: Easing.InOutSine }
    }

    NumberAnimation on scanPhase {
        loops: Animation.Infinite
        from: 0.0
        to: 1.0
        duration: 3600
        easing.type: Easing.InOutSine
    }

    Rectangle {
        anchors.fill: parent
        color: panelColor
    }

    Scene3D {
        id: sceneView
        objectName: "qt3dScene3D"
        anchors.fill: parent
        anchors.margins: 1
        aspects: ["input", "logic", "render"]
        cameraAspectRatioMode: Scene3D.AutomaticAspectRatio

        Entity {
            id: sceneRoot
            components: [
                RenderSettings {
                    activeFrameGraph: ForwardRenderer {
                        camera: camera
                        clearColor: panelColor
                    }
                },
                InputSettings {}
            ]

            Camera {
                id: camera
                projectionType: CameraLens.PerspectiveProjection
                fieldOfView: 38
                nearPlane: 0.1
                farPlane: 1000
                position: Qt.vector3d(0, 7.5, 17)
                viewCenter: Qt.vector3d(0, 0.2, 0)
                upVector: Qt.vector3d(0, 1, 0)
            }

            OrbitCameraController {
                camera: camera
                linearSpeed: 24
                lookSpeed: 140
            }

            DirectionalLight {
                id: keyLight
                worldDirection: Qt.vector3d(-0.4, -0.9, -0.25)
                color: "#dff9ff"
                intensity: 1.35
            }

            Entity {
                id: fixtureBase
                components: [
                    CuboidMesh {
                        xExtent: 10.5
                        yExtent: 0.28
                        zExtent: 5.5
                    },
                    PhongMaterial {
                        diffuse: "#24343c"
                        ambient: "#18242a"
                        specular: "#708893"
                        shininess: 55
                    },
                    Transform {
                        translation: Qt.vector3d(0, -1.15, 0)
                    }
                ]
            }

            Entity {
                id: bodyPanel
                components: [
                    CuboidMesh {
                        xExtent: 6.2
                        yExtent: 0.42
                        zExtent: 2.8
                    },
                    PhongMaterial {
                        diffuse: "#1c2c35"
                        ambient: "#102028"
                        specular: accentColor
                        shininess: 85
                    },
                    Transform {
                        id: bodyTransform
                        translation: Qt.vector3d(-1.8, 0.02, 0)
                        rotationY: -12 + root.activityPulse * root.motionScale
                        rotationZ: 1.5 + root.activityPulse * 0.35
                    }
                ]
            }

            Entity {
                id: closurePanel
                components: [
                    CuboidMesh {
                        xExtent: 4.8
                        yExtent: 0.36
                        zExtent: 2.3
                    },
                    PhongMaterial {
                        diffuse: "#283841"
                        ambient: "#16262d"
                        specular: warningColor
                        shininess: 72
                    },
                    Transform {
                        id: closureTransform
                        translation: Qt.vector3d(2.2, 0.52 + root.activityPulse * root.motionScale * 0.04, 0.65)
                        rotationY: 16 - root.activityPulse * root.motionScale
                        rotationZ: -2.2 + root.activityPulse * 0.25
                    }
                ]
            }

            Entity {
                id: datumRail
                components: [
                    CuboidMesh {
                        xExtent: 0.18
                        yExtent: 2.6
                        zExtent: 0.18
                    },
                    PhongMaterial {
                        diffuse: accentColor
                        ambient: "#0d3945"
                        specular: "#b6f7ff"
                        shininess: 90
                    },
                    Transform {
                        id: datumTransform
                        translation: Qt.vector3d(4.0, 0.15, -1.5 + root.activityPulse * root.motionScale * 0.05)
                    }
                ]
            }

            Entity {
                id: deviationHalo
                components: [
                    TorusMesh {
                        radius: 2.9
                        minorRadius: 0.018
                        rings: 72
                        slices: 12
                    },
                    PhongMaterial {
                        diffuse: viewType === "optimizer" || viewType === "gdt" ? warningColor : accentColor
                        ambient: "#0a3038"
                        specular: "#e8fbff"
                        shininess: 100
                    },
                    Transform {
                        translation: Qt.vector3d(0.15, 0.34, 0.18)
                        rotationX: 90
                        rotationY: 8 + root.activityPulse * root.motionScale * 8
                    }
                ]
            }

            Entity {
                id: locatorA
                components: [
                    SphereMesh { radius: 0.18 },
                    PhongMaterial {
                        diffuse: accentColor
                        ambient: "#0c3b48"
                        specular: "#ddfbff"
                        shininess: 95
                    },
                    Transform {
                        translation: Qt.vector3d(-4.4, 0.25, -1.8)
                    }
                ]
            }

            Entity {
                id: locatorB
                components: [
                    SphereMesh { radius: 0.18 },
                    PhongMaterial {
                        diffuse: warningColor
                        ambient: "#4a3210"
                        specular: "#ffe2b1"
                        shininess: 88
                    },
                    Transform {
                        translation: Qt.vector3d(3.1, 0.72, 1.95)
                    }
                ]
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "#1e4652"
        opacity: 0.75
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        y: 28 + root.scanPhase * Math.max(1, parent.height - 72)
        height: 2
        color: accentColor
        opacity: 0.22
    }

    Item {
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.56
        height: width
        Repeater {
            model: 3
            Rectangle {
                anchors.centerIn: parent
                width: parent.width - index * 74 + root.activityPulse * 18
                height: width
                radius: width / 2
                color: "transparent"
                border.color: index === 1 ? warningColor : accentColor
                border.width: 1
                opacity: 0.10 + index * 0.05
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        spacing: 6
        Repeater {
            model: [
                "MC seed locked",
                "GF²σ² map",
                "6DOF solve",
                "HST/HLM ready"
            ]
            Rectangle {
                width: Math.max(78, modelData.length * 6 + 16)
                height: 22
                radius: 3
                color: "#101921cc"
                border.color: index === 1 ? warningColor : "#2b4652"
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: index === 1 ? "#ffe1a9" : "#bdd0d7"
                    font.pixelSize: 9
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 12
        width: 310
        height: 32
        radius: 4
        color: "#101921cc"
        border.color: "#2b4652"
        Row {
            anchors.centerIn: parent
            spacing: 8
            Rectangle {
                width: 7
                height: 7
                radius: 3
                color: accentColor
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: "Qt3D / " + modeLabel + " / " + viewType + " / " + statusLabel
                color: "#bdd0d7"
                font.pixelSize: 11
            }
        }
    }
}
