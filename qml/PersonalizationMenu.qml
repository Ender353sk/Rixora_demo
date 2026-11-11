import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// РАЗДЕЛ НАСТРОЕК "ПЕРСОНАЛИЗАЦИЯ"
// (память только для этого чата и автоопределение языка речи).
// Использует контекстное свойство networkManager и getText для переводов.
Popup {
    id: personalizationMenu
    property var getText: null
    property bool panelExpanded: false
    property real panelExpandedWidth: 110
    property real panelCollapsedWidth: 56

    width: 320
    height: 320
    x: panelExpanded ? panelExpandedWidth + 10 : panelCollapsedWidth + 10
    y: 60
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: "#1F2833"; radius: 12; border.color: "#66FCF1"; border.width: 1 }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Text {
            text: personalizationMenu.getText ? personalizationMenu.getText("personalize") : ""
            color: "#66FCF1"
            font.pixelSize: 16
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#45A29E"
            opacity: 0.4
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    text: personalizationMenu.getText ? personalizationMenu.getText("perChat") : ""
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: personalizationMenu.getText ? personalizationMenu.getText("perChatDesc") : ""
                    color: "#888888"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            Switch {
                id: perChatSwitch
                Layout.preferredWidth: 52
                Layout.alignment: Qt.AlignVCenter
                hoverEnabled: true
                checked: networkManager.perChatMemory
                onToggled: networkManager.perChatMemory = checked

                indicator: Rectangle {
                    implicitWidth: 48
                    implicitHeight: 26
                    radius: 13
                    color: perChatSwitch.checked ? "#45A29E" : "#2C394B"
                    border.color: perChatSwitch.checked ? "#66FCF1" : "#45A29E"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 120 } }

                    Rectangle {
                        width: 20
                        height: 20
                        radius: 10
                        color: "#FFFFFF"
                        x: perChatSwitch.checked ? parent.width - width - 2 : 2
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#45A29E"
            opacity: 0.4
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Image {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                Layout.alignment: Qt.AlignVCenter
                source: "qrc:/icons/Mic.svg"
                sourceSize: Qt.size(48, 48)
                fillMode: Image.PreserveAspectFit
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                Text {
                    Layout.fillWidth: true
                    text: personalizationMenu.getText ? personalizationMenu.getText("speechAuto") : ""
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: personalizationMenu.getText ? personalizationMenu.getText("speechAutoDesc") : ""
                    color: "#888888"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
            }

            Switch {
                id: speechAutoSwitch
                Layout.preferredWidth: 52
                Layout.alignment: Qt.AlignVCenter
                hoverEnabled: true
                checked: networkManager.speechLanguageAuto
                onToggled: networkManager.speechLanguageAuto = checked

                indicator: Rectangle {
                    implicitWidth: 48
                    implicitHeight: 26
                    radius: 13
                    color: speechAutoSwitch.checked ? "#45A29E" : "#2C394B"
                    border.color: speechAutoSwitch.checked ? "#66FCF1" : "#45A29E"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 120 } }

                    Rectangle {
                        width: 20
                        height: 20
                        radius: 10
                        color: "#FFFFFF"
                        x: speechAutoSwitch.checked ? parent.width - width - 2 : 2
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
                    }
                }
            }
        }
    }
}