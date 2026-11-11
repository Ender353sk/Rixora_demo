import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 40

    property var uiTexts
    property string currentLanguage: "ru"
    property string initialGeminiKey: ""
    property string initialGroqKey: ""

    signal loginRequested(string geminiKey, string groqKey)

    Component.onCompleted: {
        geminiKeyInput.text = initialGeminiKey
        groqKeyInput.text = initialGroqKey
    }

    Text {
        text: "RIXORA AI"
        color: "#66FCF1"
        font.pixelSize: 48
        font.bold: true
        font.letterSpacing: 2
        Layout.alignment: Qt.AlignHCenter
    }

    ColumnLayout {
        spacing: 20
        Layout.alignment: Qt.AlignHCenter

        ColumnLayout {
            spacing: 8
            Text { text: "Gemini API Key"; color: "#C5C6C7"; font.pixelSize: 14; font.bold: true }
            TextField {
                id: geminiKeyInput
                placeholderText: (root.uiTexts && root.uiTexts[root.currentLanguage]) ? root.uiTexts[root.currentLanguage].gemini : root.uiTexts["ru"].gemini
                echoMode: TextInput.Password
                color: "#FFFFFF"
                font.pixelSize: 16
                Layout.preferredWidth: 320
                Layout.preferredHeight: 50
                background: Rectangle {
                    color: "#1F2833"
                    radius: 8
                    border.color: geminiKeyInput.activeFocus ? "#66FCF1" : "#45A29E"
                    border.width: geminiKeyInput.activeFocus ? 2 : 1
                }
            }
        }

        ColumnLayout {
            spacing: 8
            Text { text: "Groq API Key"; color: "#C5C6C7"; font.pixelSize: 14; font.bold: true }
            TextField {
                id: groqKeyInput
                placeholderText: (root.uiTexts && root.uiTexts[root.currentLanguage]) ? root.uiTexts[root.currentLanguage].groq : root.uiTexts["ru"].groq
                echoMode: TextInput.Password
                color: "#FFFFFF"
                font.pixelSize: 16
                Layout.preferredWidth: 320
                Layout.preferredHeight: 50
                background: Rectangle {
                    color: "#1F2833"
                    radius: 8
                    border.color: groqKeyInput.activeFocus ? "#66FCF1" : "#45A29E"
                    border.width: groqKeyInput.activeFocus ? 2 : 1
                }
            }
        }
    }

    Button {
        id: startButton
        Layout.preferredWidth: 320
        Layout.preferredHeight: 55
        Layout.topMargin: 20
        Layout.alignment: Qt.AlignHCenter
        hoverEnabled: true
        flat: true

        contentItem: Text {
            text: (root.uiTexts && root.uiTexts[root.currentLanguage]) ? root.uiTexts[root.currentLanguage].save : root.uiTexts["ru"].save
            font.pixelSize: 16
            font.bold: true
            font.letterSpacing: 1
            color: "#0B0C10"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            color: startButton.down ? "#45A29E" : (startButton.hovered ? "#7DFDF3" : "#66FCF1")
            radius: 12
        }

        onClicked: {
            root.loginRequested(geminiKeyInput.text, groqKeyInput.text)
        }
    }
}