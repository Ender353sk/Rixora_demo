// GlowBorder.qml
import QtQuick

Item {
    id: root

    // Цвет заливки самого объекта и цвет свечения
    property color fill: "#0B0C10"
    property color glow: "#66FCF1"
    property real cornerRadius: 25
    property bool glowVisible: true

    // Параметры градиентного бордюра: чем дальше от края, тем прозрачнее
    property real glowSpread: 10
    property int glowSteps: 20
    property real glowPeak: 0.6
    property real glowFalloff: 1.8

    // Слои-гало позади объекта (дискретный радиальный градиент)
    Repeater {
        model: root.glowSteps

        delegate: Rectangle {
            property real offset: root.glowSpread * (index + 0.5) / root.glowSteps

            anchors.fill: root
            anchors.margins: -offset
            radius: root.cornerRadius + offset
            color: "transparent"
            border.color: root.glow
            border.width: 1
            antialiasing: true
            opacity: root.glowVisible
                     ? root.glowPeak * Math.pow(1 - offset / root.glowSpread, root.glowFalloff)
                     : 0

            Behavior on opacity { NumberAnimation { duration: 150 } }
            Behavior on border.color { ColorAnimation { duration: 250 } }
        }
    }

    // Сам объект
    Rectangle {
        id: body
        anchors.fill: parent
        radius: root.cornerRadius
        color: root.fill
        antialiasing: true
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}