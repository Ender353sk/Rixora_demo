import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ПОДТВЕРЖДЕНИЕ УДАЛЕНИЯ ОТДЕЛЬНОГО ОТВЕТА РИКСОРЫ (чат + память).
// Открывается из Main.qml; подтверждение передаётся сигналом confirmed(id).
Popup {
    id: msgDeletePopup
    property var getText: null
    property string pendingDeleteId: ""

    signal confirmed(string id)

    modal: true
    width: 300
    height: 200
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: "#1F2833"; radius: 12; border.color: "#C0392B"; border.width: 1 }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text {
            Layout.fillWidth: true
            text: msgDeletePopup.getText ? msgDeletePopup.getText("deleteMsgTitle") : ""
            color: "#C0392B"
            font.pixelSize: 17
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: msgDeletePopup.getText ? msgDeletePopup.getText("deleteMsgBody") : ""
            color: "#C5C6C7"
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: msgDeletePopup.getText ? msgDeletePopup.getText("no") : ""
                hoverEnabled: true; flat: true
                contentItem: Text { text: parent.text; color: "#0B0C10"; font.pixelSize: 14; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? "#45A29E" : (parent.hovered ? "#7DFDF3" : "#45A29E"); radius: 8 }
                onClicked: msgDeletePopup.close()
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: msgDeletePopup.getText ? msgDeletePopup.getText("yes") : ""
                hoverEnabled: true; flat: true
                contentItem: Text { text: parent.text; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? "#8D2F2F" : (parent.hovered ? "#D9534F" : "#C0392B"); radius: 8 }
                onClicked: {
                    msgDeletePopup.close()
                    msgDeletePopup.confirmed(msgDeletePopup.pendingDeleteId)
                }
            }
        }
    }
}