import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// ПОДТВЕРЖДЕНИЕ ПЕРЕГЕНЕРАЦИИ ОТВЕТА (кнопка "обновить").
// Действо разрушительное: перезапись ответа на выбранной реплике отбрасывает
// ВСЮ переписку ниже неё. Раньше это происходило по одному клику, без
// предупреждения, и часть работы пользователя исчезала безвозвратно.
Popup {
    id: msgRewritePopup
    property var getText: null
    property string pendingRewriteId: ""

    signal confirmed(string id)

    modal: true
    width: 320
    height: 210
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: "#1F2833"; radius: 12; border.color: "#E67E22"; border.width: 1 }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Text {
            Layout.fillWidth: true
            text: msgRewritePopup.getText ? msgRewritePopup.getText("rewriteMsgTitle") : ""
            color: "#E67E22"
            font.pixelSize: 17
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: msgRewritePopup.getText ? msgRewritePopup.getText("rewriteMsgBody") : ""
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
                text: msgRewritePopup.getText ? msgRewritePopup.getText("no") : ""
                hoverEnabled: true; flat: true
                contentItem: Text { text: parent.text; color: "#0B0C10"; font.pixelSize: 14; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? "#45A29E" : (parent.hovered ? "#7DFDF3" : "#45A29E"); radius: 8 }
                onClicked: msgRewritePopup.close()
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: msgRewritePopup.getText ? msgRewritePopup.getText("rewriteMsgConfirm") : ""
                hoverEnabled: true; flat: true
                contentItem: Text { text: parent.text; color: "#FFFFFF"; font.pixelSize: 14; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? "#9C4A0A" : (parent.hovered ? "#F0A85A" : "#E67E22"); radius: 8 }
                onClicked: {
                    msgRewritePopup.close()
                    msgRewritePopup.confirmed(msgRewritePopup.pendingRewriteId)
                }
            }
        }
    }
}
