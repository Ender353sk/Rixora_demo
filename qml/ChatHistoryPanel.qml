import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// БОКОВАЯ ПАНЕЛЬ: история чатов + ключевые функции.
// Чистый UI-компонент: всё объединение с логикой (Main.qml) — через сигналы
// (historyToggleRequested, newChatRequested, settingsRequested, chatSelected,
// chatMenuRequested) и свойства (getText, historyModel, sideMargin).
Item {
    id: sidePanel
    property bool expanded: false
    readonly property real collapsedWidth: 56
    readonly property real expandedWidth: Math.max(sideMargin - 8, 110)
    property real sideMargin: 0
    property var getText: null
    property var historyModel: null
    property string activeChatId: ""
    property bool menuOpen: false
    property string menuChatId: ""

    anchors.left: parent.left
    anchors.top: parent.top
    anchors.bottom: parent.bottom

    width: expanded ? expandedWidth : collapsedWidth
    Behavior on width { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }

    z: 6
    clip: true

    signal historyToggleRequested()
    signal newChatRequested()
    signal settingsRequested()
    signal chatSelected(string chatId)
    signal chatMenuRequested(string chatId, real x, real y, real btnWidth, real btnHeight)

    // ПОДЛОЖКА ПАНЕЛИ (появляется плавно при разворачивании)
    Rectangle {
        anchors.fill: parent
        radius: 10
        color: sidePanel.expanded ? "#15171E" : "transparent"
        border.color: sidePanel.expanded ? "#262B36" : "transparent"
        border.width: 1
        Behavior on color { ColorAnimation { duration: 280 } }
    }

    Item {
        id: panelContent
        width: sidePanel.expandedWidth
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        // ВЕРХ: фиксированный блок кнопок функций
        ColumnLayout {
            id: topBlock
            width: sidePanel.expandedWidth
            anchors.left: parent.left
            anchors.top: parent.top
            spacing: 4

            SidePanelButton {
                id: historyToggleBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                iconSource: "qrc:/icons/More...svg"
                label: sidePanel.getText ? sidePanel.getText("historyLabel") : ""
                isPanelExpanded: sidePanel.expanded
                onClicked: {
                    sidePanel.historyToggleRequested()
                    sidePanel.expanded = !sidePanel.expanded
                }
            }

            SidePanelButton {
                id: newChatBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                iconSource: "qrc:/icons/New chat.svg"
                label: sidePanel.getText ? sidePanel.getText("newChatLabel") : ""
                isPanelExpanded: sidePanel.expanded
                onClicked: sidePanel.newChatRequested()
            }

            SidePanelButton {
                id: panelSettingsBtn
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                iconSource: "qrc:/icons/Settings.svg"
                label: sidePanel.getText ? sidePanel.getText("settingsLabel") : ""
                isPanelExpanded: sidePanel.expanded
                onClicked: sidePanel.settingsRequested()
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: 6
                Layout.bottomMargin: 6
                color: "#262B36"
                opacity: sidePanel.expanded ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 220 } }
            }

            Text {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                text: (sidePanel.getText ? sidePanel.getText("historyLabel") : "").toUpperCase()
                color: "#8F99AA"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 1.2
                opacity: sidePanel.expanded ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 220 } }
            }
        }

        // НИЗ: область истории (заполняет оставшееся пространство)
        MouseArea {
            id: historyArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: topBlock.bottom
            anchors.bottom: parent.bottom
            acceptedButtons: Qt.NoButton
            onWheel: function(wheel) { wheel.accepted = true }

            Text {
                anchors.centerIn: parent
                width: parent.width - 48
                text: sidePanel.getText ? sidePanel.getText("empty") : ""
                color: "#8A93A3"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                visible: sidePanel.historyModel && sidePanel.historyModel.count === 0
                opacity: sidePanel.expanded ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 220 } }
            }

            ListView {
                id: historyList
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 3
                clip: true
                spacing: 6
                model: sidePanel.historyModel
                visible: sidePanel.historyModel && sidePanel.historyModel.count > 0
                // При свёрнутой панели список невидим, но его делегаты
                // иначе продолжают ловить клики в зоне обрезки (clip).
                enabled: sidePanel.expanded
                opacity: sidePanel.expanded ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: 220 } }

            ScrollBar.vertical: ScrollBar {
                id: historyScrollBar
                width: 3
                padding: 0
                policy: ScrollBar.AsNeeded

                opacity: (historyScrollBar.active || historyScrollBar.pressed || historyScrollBar.hovered || hideHistoryScrollTimer.running) ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

                Timer {
                    id: hideHistoryScrollTimer
                    interval: 2500
                    repeat: false
                }

                onActiveChanged: {
                    if (active) hideHistoryScrollTimer.stop()
                    else hideHistoryScrollTimer.restart()
                }

                contentItem: Rectangle { width: historyScrollBar.width; implicitHeight: 100; radius: 1; color: historyScrollBar.pressed || historyScrollBar.hovered ? "#EDF0F5" : "#45A29E" }
                background: Rectangle { width: historyScrollBar.width; implicitHeight: 100; radius: 1; color: "#2A313E" }
            }

                delegate: Rectangle {
                    id: chatItemBg
                    width: historyList.width - 6
                    height: 52
                    radius: 10
                    property bool isActive: sidePanel.activeChatId === model.chatId
                    color: isActive ? "#1E242F" : (chatItemMouse.containsMouse ? "#1E242F" : "#141720")
                    border.color: isActive ? "#45A29E" : "#2A313E"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 120 } }

                    MouseArea {
                        id: chatItemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: sidePanel.chatSelected(model.chatId)
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        spacing: 6

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                Layout.fillWidth: true
                                text: model.title
                                color: "#EDF0F5"
                                font.pixelSize: 13
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: model.date
                                color: "#7C8696"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            id: menuBtn
                            width: 26
                            height: 26
                            radius: 6
                            color: (menuBtnMouse.containsMouse || (sidePanel.menuOpen && sidePanel.menuChatId === model.chatId)) ? "#2A313E" : "transparent"
                            Layout.alignment: Qt.AlignVCenter
                            opacity: menuBtnVisible ? 1 : 0
                            enabled: menuBtnVisible
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                            property bool menuBtnVisible: chatItemMouse.containsMouse || menuBtnMouse.containsMouse || (sidePanel.menuOpen && sidePanel.menuChatId === model.chatId)

                            Column {
                                anchors.centerIn: parent
                                spacing: 3

                                Rectangle { width: 4; height: 4; radius: 2; color: "#9AA3B2" }
                                Rectangle { width: 4; height: 4; radius: 2; color: "#9AA3B2" }
                                Rectangle { width: 4; height: 4; radius: 2; color: "#9AA3B2" }
                            }

                            MouseArea {
                                id: menuBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    var pt = menuBtn.mapToItem(Window.window.contentItem, 0, 0)
                                    sidePanel.chatMenuRequested(model.chatId, pt.x, pt.y, menuBtn.width, menuBtn.height)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}