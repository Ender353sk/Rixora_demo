import QtQuick
import QtQuick.Controls

// ВЕРТИКАЛЬНАЯ ПОЛОСА ПРОКРУТКИ ЧАТА.
// Трек и бегунок появляются только во время прокрутки и плавно исчезают
// через 2.5 с после остановки. Используется ChatView через externalScrollBar
// (см. ChatChrome.qml).
Item {
    id: chatScrollBarArea
    property alias scrollBar: mainChatScrollBar

    z: 3
    width: 4

    opacity: (mainChatScrollBar.active || mainChatScrollBar.pressed || mainChatScrollBar.hovered || hideChatScrollTimer.running) ? 1.0 : 0.0
    Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

    Rectangle {
        id: track
        anchors.fill: parent
        radius: 2
        color: "#2A313E"
    }

    ScrollBar {
        id: mainChatScrollBar
        anchors.fill: parent
        padding: 0

        Timer {
            id: hideChatScrollTimer
            interval: 2500
            repeat: false
        }

        onActiveChanged: {
            if (active) hideChatScrollTimer.stop()
            else hideChatScrollTimer.restart()
        }

        background: null

        contentItem: Rectangle {
            width: mainChatScrollBar.width
            implicitHeight: 100
            radius: 2
            color: (mainChatScrollBar.pressed || mainChatScrollBar.hovered) ? "#EDF0F5" : "#45A29E"
        }
    }
}