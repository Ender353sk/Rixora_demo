import QtQuick

// Круглая кнопка действия над сообщением в чате (без рамки).
// Иконка — готовый SVG-файл (qrc:/icons/...), рендерится через Image.
Item {
    id: root

    property string iconSource: ""
    // Позиция курсора в координатах ряда кнопок. Подсветка кнопки считается
    // по ней, потому что поверх сообщений лежит общая hover-зона (zoneHover),
    // которая забирает containsMouse у самих кнопок.
    property point hoverPoint: Qt.point(-1, -1)

    signal clicked()

    width: 29
    height: 29
    implicitWidth: 29
    implicitHeight: 29

    readonly property bool isHovered: {
        if (root.parent === null) return false
        if (hoverPoint.x < 0 || hoverPoint.y < 0) return false
        return hoverPoint.x >= root.x && hoverPoint.x <= root.x + root.width
            && hoverPoint.y >= root.y && hoverPoint.y <= root.y + root.height
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
    }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: area.pressed ? Qt.lighter("#1F2833", 1.5) : (root.isHovered ? "#1F2833" : "#0B0C10")
        antialiasing: true
    }

    Image {
        anchors.fill: parent
        source: root.iconSource
        sourceSize: Qt.size(48, 48)
        smooth: true
        fillMode: Image.PreserveAspectFit
    }
}