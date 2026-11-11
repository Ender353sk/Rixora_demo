import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ПОДТВЕРЖДЕНИЕ ПОЛНОЙ ОЧИСТКИ (чаты + вся память Риксоры).
// Открывается из Main.qml; подтверждение передаётся сигналом confirmed().
Popup {
    id: deleteConfirmPopup
    property var getText: null

    signal confirmed()

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
            text: deleteConfirmPopup.getText ? deleteConfirmPopup.getText("confirmTitle") : ""
            color: "#C0392B"
            font.pixelSize: 17
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: deleteConfirmPopup.getText ? deleteConfirmPopup.getText("confirmBody") : ""
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
                text: deleteConfirmPopup.getText ? deleteConfirmPopup.getText("no") : ""
                hoverEnabled: true; flat: true
                contentItem: Text { text: parent.text; color: "#0B0C10"; font.pixelSize: 14; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? "#45A29E" : (parent.hovered ? "#7DFDF3" : "#45A29E"); radius: 8 }
                onClicked: deleteConfirmPopup.close()
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: deleteConfirmPopup.getText ? deleteConfirmPopup.getText("yes") : ""
                hoverEnabled: true; flat: true
                contentItem: Text { text: parent.text; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? "#8D2F2F" : (parent.hovered ? "#D9534F" : "#C0392B"); radius: 8 }
                onClicked: deleteConfirmPopup.confirmed()
            }
        }
    }
}