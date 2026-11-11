import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// МЕНЮ НАСТРОЕК (клавиши API / язык / персонализация / полная очистка).
// Действия передаются сигналами в Main.qml; позиция зависит от sidePanel
// (передаётся через panelExpanded / panelExpandedWidth / panelCollapsedWidth).
Popup {
    id: settingsMenu
    property var getText: null
    property bool panelExpanded: false
    property real panelExpandedWidth: 110
    property real panelCollapsedWidth: 56

    signal keysRequested()
    signal languageRequested()
    signal personalizeRequested()
    signal deleteAllRequested()

    width: 196
    height: 202
    x: panelExpanded ? panelExpandedWidth + 10 : panelCollapsedWidth + 10
    y: 120
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: "#16181F"; radius: 12; border.color: "#262B36"; border.width: 1 }

    ColumnLayout {
        width: settingsMenu.width - 16
        anchors.centerIn: parent
        spacing: 6

        Button {
            id: keysButton
            text: settingsMenu.getText ? settingsMenu.getText("keys") : ""
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            hoverEnabled: true; flat: true
            padding: 0
            contentItem: Item {
                anchors.fill: parent
                Image {
                    id: keyIcon
                    width: 40
                    height: 40
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    source: "qrc:/icons/keys.svg"
                    sourceSize: Qt.size(48, 48)
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    text: keysButton.text
                    anchors.left: keyIcon.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    color: "#FFFFFF"
                    font.pixelSize: 14
                }
            }
            background: Rectangle { color: parent.down ? "#2E3644" : (parent.hovered ? "#1F2530" : "transparent"); radius: 8 }
            onClicked: {
                settingsMenu.close()
                settingsMenu.keysRequested()
            }
        }
        Button {
            id: langButton
            text: settingsMenu.getText ? settingsMenu.getText("lang") : ""
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            hoverEnabled: true; flat: true
            padding: 0
            contentItem: Item {
                anchors.fill: parent
                Image {
                    id: langIcon
                    width: 40
                    height: 40
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    source: "qrc:/icons/ChangeLanguages.svg"
                    sourceSize: Qt.size(48, 48)
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    text: langButton.text
                    anchors.left: langIcon.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    color: "#FFFFFF"
                    font.pixelSize: 14
                }
            }
            background: Rectangle { color: parent.down ? "#2E3644" : (parent.hovered ? "#1F2530" : "transparent"); radius: 8 }
            onClicked: {
                settingsMenu.close()
                settingsMenu.languageRequested()
            }
        }
        Button {
            id: personalizeButton
            text: settingsMenu.getText ? settingsMenu.getText("personalize") : ""
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            hoverEnabled: true; flat: true
            padding: 0
            contentItem: Item {
                anchors.fill: parent
                Image {
                    id: persIcon
                    width: 40
                    height: 40
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    source: "qrc:/icons/Personalization.svg"
                    sourceSize: Qt.size(48, 48)
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    text: personalizeButton.text
                    anchors.left: persIcon.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    color: "#FFFFFF"
                    font.pixelSize: 14
                }
            }
            background: Rectangle { color: parent.down ? "#2E3644" : (parent.hovered ? "#1F2530" : "transparent"); radius: 8 }
            onClicked: {
                settingsMenu.close()
                settingsMenu.personalizeRequested()
            }
        }
        Button {
            id: deleteAllButton
            text: settingsMenu.getText ? settingsMenu.getText("deleteAll") : ""
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            hoverEnabled: true; flat: true
            padding: 0
            contentItem: Item {
                anchors.fill: parent
                Image {
                    id: delIcon
                    width: 40
                    height: 40
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    source: "qrc:/icons/Delete.svg"
                    sourceSize: Qt.size(48, 48)
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    text: deleteAllButton.text
                    anchors.left: delIcon.right
                    anchors.leftMargin: 8
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    color: "#FF6B6B"
                    font.pixelSize: 14
                }
            }
            background: Rectangle { color: parent.down ? "#3A1F22" : (parent.hovered ? "#2A2022" : "transparent"); radius: 8 }
            onClicked: {
                settingsMenu.close()
                settingsMenu.deleteAllRequested()
            }
        }
    }
}