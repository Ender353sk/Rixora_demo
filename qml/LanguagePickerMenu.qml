import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ВЫБОР ЯЗЫКА БОТА. Самодостаточный попап (использует только контекстное свойство
// networkManager); позиция зависит от sidePanel (через panelExpanded и т.д.).
Popup {
    id: languageMenu
    property bool panelExpanded: false
    property real panelExpandedWidth: 110
    property real panelCollapsedWidth: 56

    width: 250
    height: 380
    x: panelExpanded ? panelExpandedWidth + 10 : panelCollapsedWidth + 10
    y: 60
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: "#1F2833"; radius: 12; border.color: "#66FCF1"; border.width: 1 }

    ScrollView {
        id: langScrollView
        anchors.fill: parent
        anchors.margins: 10
        clip: false
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ScrollBar.vertical: ScrollBar {
            id: customScrollBar
            parent: langScrollView
            x: langScrollView.width + 4 - width
            y: langScrollView.y + langScrollView.topPadding
            height: langScrollView.availableHeight
            width: 2.5
            padding: 0
            policy: ScrollBar.AsNeeded

            opacity: (customScrollBar.active || customScrollBar.pressed || hideTimer.running) ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

            Timer {
                id: hideTimer
                interval: 2500
                repeat: false
            }

            onActiveChanged: {
                if (active) hideTimer.stop()
                else hideTimer.restart()
            }

            contentItem: Rectangle {
                width: customScrollBar.width
                implicitHeight: 100
                radius: 1
                color: customScrollBar.pressed ? "#66FCF1" : (customScrollBar.hovered ? "#EDF0F5" : "#45A29E")
            }

            background: Rectangle {
                width: customScrollBar.width
                implicitHeight: 100
                radius: 1
                color: "#2A313E"
            }
        }

        ColumnLayout {
            width: parent.width - 15
            spacing: 8

            Repeater {
                model: ListModel {
                    ListElement { name: "🇬🇧 Английский (English)"; code: "en" }
                    ListElement { name: "🇪🇸 Испанский (Español)"; code: "es" }
                    ListElement { name: "🇰🇷 Корейский (한국어)"; code: "ko" }
                    ListElement { name: "🇩🇪 Немецкий (Deutsch)"; code: "de" }
                    ListElement { name: "🇷🇺 Русский (Русский)"; code: "ru" }
                    ListElement { name: "🇺🇦 Украинский (Українська)"; code: "uk" }
                    ListElement { name: "🇫🇷 Французский (Français)"; code: "fr" }
                    ListElement { name: "🇯🇵 Японский (日本語)"; code: "ja" }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    hoverEnabled: true; flat: true

                    contentItem: Text {
                        text: model.name
                        color: networkManager.language === model.code ? "#66FCF1" : "#FFFFFF"
                        font.pixelSize: 15
                        verticalAlignment: Text.AlignVCenter
                        font.bold: networkManager.language === model.code
                    }

                    background: Rectangle {
                        color: parent.down ? "#45A29E" : (parent.hovered ? "#2C394B" : "transparent")
                        radius: 8
                    }

                    onClicked: {
                        networkManager.setLanguage(model.code)
                        languageMenu.close()
                    }
                }
            }
        }
    }
}