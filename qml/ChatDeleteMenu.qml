import QtQuick
import QtQuick.Controls

// МЕНЮ УДАЛЕНИЯ ОТДЕЛЬНОГО ЧАТА.
// Объявлено в корне окна (как SettingsMenu) — так попап получает корректные
// координаты и ввод; позиционируется справа от кнопки-троеточия в истории.
// Удаление передаётся сигналом deleteRequested(chatId) в Main.qml.
Popup {
    id: chatDeleteMenu
    property var getText: null
    property string targetChatId: ""

    signal deleteRequested(string chatId)

    width: 172
    height: 42
    padding: 4
    margins: 8
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle { color: "#16181F"; radius: 10; border.color: "#262B36"; border.width: 1 }

    function showFor(chatId, x, y, btnWidth, btnHeight) {
        if (chatDeleteMenu.opened) {
            chatDeleteMenu.close()
            if (chatDeleteMenu.targetChatId === chatId) return
        }
        chatDeleteMenu.targetChatId = chatId
        chatDeleteMenu.x = Math.max(8, x + btnWidth + 6)
        chatDeleteMenu.y = Math.max(8, y + (btnHeight - chatDeleteMenu.height) / 2)
        chatDeleteMenu.open()
    }

    Button {
        anchors.fill: parent
        text: chatDeleteMenu.getText ? chatDeleteMenu.getText("deleteChat") : "Delete chat"
        hoverEnabled: true
        flat: true

        contentItem: Text {
            text: parent.text
            color: "#FF6B6B"
            font.pixelSize: 13
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            leftPadding: 12
        }

        background: Rectangle {
            color: parent.down ? "#3A1F22" : (parent.hovered ? "#2A2022" : "transparent")
            radius: 7
        }

        onClicked: {
            chatDeleteMenu.close()
            chatDeleteMenu.deleteRequested(chatDeleteMenu.targetChatId)
        }
    }
}