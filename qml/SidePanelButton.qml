import QtQuick

Item {
    id: root

    property string iconSource: ""
    property string label: ""
    property bool isPanelExpanded: false

    signal clicked()

    implicitHeight: 48

    Rectangle {
        id: actionBg
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height - 8
        radius: 12
        width: root.isPanelExpanded ? parent.width - 12 : 46

        Behavior on width { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }

        color: {
            if (!root.isPanelExpanded) {
                // Свёрнутая панель: при нажатии — подсветка как при наведении
                // в развёрнутом состоянии.
                return actionMouse.pressed ? "#272E3B" : "transparent"
            }
            if (actionMouse.pressed) return "#3A4252"
            if (actionMouse.containsMouse) return "#272E3B"
            return "#1B202B"
        }
        Behavior on color { ColorAnimation { duration: 180 } }
    }

    Rectangle {
        id: iconCell
        width: 38
        height: 38
        radius: 11
        anchors.left: root.left
        anchors.leftMargin: 9
        anchors.verticalCenter: parent.verticalCenter
        color: {
            if (root.isPanelExpanded) return actionMouse.containsMouse ? "#2E3644" : "#232936"
            // Свёрнутая панель: hover чуть заметнее, нажатие — как при
            // наведении в развёрнутом состоянии.
            if (actionMouse.pressed) return "#2E3644"
            return actionMouse.containsMouse ? "#202832" : "transparent"
        }
        Behavior on color { ColorAnimation { duration: 180 } }

        // SVG-иконка (если iconSource задан) — рисуется по центру ячейки.
        Image {
            anchors.centerIn: parent
            visible: root.iconSource !== ""
            width: 42
            height: 42
            smooth: true
            source: root.iconSource
            sourceSize: Qt.size(48, 48)
            fillMode: Image.PreserveAspectFit
        }
    }

    Text {
        id: actionLabel
        anchors.left: iconCell.right
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: "#DEE2EA"
        font.pixelSize: 15
        opacity: root.isPanelExpanded ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutQuad } }
    }

    MouseArea {
        id: actionMouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
    }
}