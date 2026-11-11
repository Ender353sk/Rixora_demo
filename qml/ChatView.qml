import QtQuick
import QtQuick.Controls

Item {
    id: root

    property alias model: chatList.model
    property ScrollBar externalScrollBar: null

    property real topPaddingArea: 65
    property real bottomPaddingArea: 85

    // Отступы слева и справа: центральная зона чата (78% ширины окна)
    property real leftMargin: 15
    property real rightMargin: 15

    // Пузырь сообщения не шире 65% ширины чата (без учёта боковых отступов)
    property real maxBubbleFraction: 0.65

    // true, пока Риксора обрабатывает запрос — кнопки действий прячем,
    // чтобы нельзя было удалять/переписывать сообщение "на лету".
    property bool isProcessing: false

    // Индекс самого свежего промпта пользователя (единственный, у которого
    // показывается кнопка редактирования). -1 = такого сообщения нет.
    property int newestUserMessageIndex: -1

    // Действия над сообщением (эмитятся в Main.qml)
    signal copyRequested(string messageId, string text)
    signal rewriteRequested(string messageId)
    signal deleteRequested(string messageId)
    signal speakRequested(string messageId)
    signal editRequested(string messageId, string text)

    // true — «прилипание» к низу: во время стриминга чат сам догоняет
    // низ при каждом сливе дельт. Сбрасывается, когда пользователь уводит
    // прокрутку от низа (onContentYChanged), возвращается — при любом
    // возврате к низу и в scrollToBottom(). На обычные (нескроллящие)
    // операции не влияет: используется только scrollToBottomIfAtEnd().
    property bool stickToBottom: true

    function scrollToBottom() {
        stickToBottom = true
        // Откладываем прокрутку, чтобы QML успел рассчитать размеры текста и баббла
        Qt.callLater(function() {
            pinToBottom()
        })
    }

    // Прокрутка «если и так внизу» — для стриминга: не сбивает пользователя,
    // читающего историю во время генерации ответа.
    function scrollToBottomIfAtEnd() {
        if (!stickToBottom) return
        pinToBottom()
        Qt.callLater(pinToBottom)
    }

    // Программная прокрутка: contentYChanged в этот момент не должен
    // решать, «прилипать» ли — иначе отставание на кадр раскладки
    // отключало бы следование за потоком.
    function pinToBottom() {
        _pinning = true
        chatList.positionViewAtEnd()
        _pinning = false
    }

    property bool _pinning: false

    ListView {
        id: chatList
        anchors.fill: parent
        // Расстояние между сообщениями. Место под ряд кнопок действий (40px)
        // зарезервировано в самой высоте сообщения, поэтому вёрстка не
        // "прыгает" при наведении курсора.
        spacing: 4
        clip: true

        // Любое изменение прокрутки (колесо, перетаскивание): ушёл от низа —
        // открепляемся, вернулся к низу — снова следуем за стримингом.
        // Наша собственная прокрутка (под флагом _pinning) сюда не попадает.
        onContentYChanged: {
            if (root._pinning) return
            root.stickToBottom = chatList.atYEnd
        }

        // Создаём физические отступы сверху и снизу для плавного заплывания под градиент
        header: Item { width: chatList.width; height: root.topPaddingArea }
        footer: Item { width: chatList.width; height: root.bottomPaddingArea }

        ScrollBar.vertical: root.externalScrollBar

        delegate: Item {
            id: msgItem
            width: chatList.width

            required property bool isUser
            required property string text
            required property int index
            required property string messageId
            // Явно объявляем model, чтобы читать произвольную роль isSystem.
            required property var model

            // Системный баннер (ошибка сети/распознавания). Роль есть не у всех
            // строк модели, поэтому сравниваем строго: undefined у обычных
            // сообщений и приветствия даёт false.
            readonly property bool isBanner: model.isSystem === true

            // id сообщения: настоящие сообщения приходят с id из MemoryManager,
            // приветствие — нет (для него кнопки действий не показываем).
            readonly property string msgId: msgItem.messageId ? msgItem.messageId : ""
            // Баннер принципиально без id: он не адресуется и не сохраняется.
            readonly property bool canAct: msgId !== "" && !msgItem.isBanner
            readonly property bool showActions: canAct && !root.isProcessing
                                                && zoneHover.containsMouse
            // Курсор в координатах ряда кнопок: по нему подсвечивается
            // отдельная кнопка (сами кнопки обрабатывать hover не могут —
            // их отбирает общая зона zoneHover).
            readonly property point hoverInActions: zoneHover.containsMouse
                                                    ? zoneHover.mapToItem(actionsArea, zoneHover.mouseX, zoneHover.mouseY)
                                                    : Qt.point(-1, -1)
            readonly property bool isNewestPrompt: root.newestUserMessageIndex >= 0
                                                   && msgItem.isUser
                                                   && index === root.newestUserMessageIndex

            readonly property real actionRowHeight: 29
            readonly property real actionRowTopMargin: 7

            // Кнопки действий (33px) + отступ (7px) всегда зарезервированы под
            // сообщением, чтобы ряд не сдвигал соседние сообщения при наведении.
            // Баннеру ряд не нужен — под ним ничего не появляется.
            height: bubbleRect.height + (msgItem.isBanner ? 6 : actionRowTopMargin + actionRowHeight)

            Rectangle {
                id: bubbleRect
                width: msgItem.isBanner
                       ? (chatList.width - root.leftMargin - root.rightMargin)
                       : Math.min(chatText.implicitWidth + 30, (chatList.width - root.leftMargin - root.rightMargin) * root.maxBubbleFraction)
                height: chatText.height + 20
                radius: msgItem.isBanner ? 6 : 12
                color: msgItem.isBanner ? "#2A1620" : (msgItem.isUser ? "#45A29E" : "#1F2833")
                border.color: msgItem.isBanner ? "#8E3B4E" : (msgItem.isUser ? "#66FCF1" : "#0B0C10")
                border.width: msgItem.isBanner ? 1 : (msgItem.isUser ? 1 : 0)

                // Баннер тянется на всю ширину чата и прижат к левому отступу.
                anchors.right: (msgItem.isUser && !msgItem.isBanner) ? parent.right : undefined
                anchors.rightMargin: (msgItem.isUser && !msgItem.isBanner) ? root.rightMargin : 0
                anchors.left: (msgItem.isUser && !msgItem.isBanner) ? undefined : parent.left
                anchors.leftMargin: (msgItem.isUser && !msgItem.isBanner) ? 0 : root.leftMargin

                opacity: 0
                Component.onCompleted: fadeInAnim.start()
                NumberAnimation {
                    id: fadeInAnim
                    target: bubbleRect
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 250
                    easing.type: Easing.OutQuad
                }

                Text {
                    id: chatText
                    x: 15
                    y: 10
                    width: parent.width - 30
                    horizontalAlignment: msgItem.isBanner ? Text.AlignHCenter : Text.AlignLeft
                    text: msgItem.text
                    color: msgItem.isBanner ? "#FFC9D2" : "#FFFFFF"
                    font.pixelSize: 15
                    wrapMode: Text.Wrap
                }
            }

            // Контейнер ряда кнопок действий под пузырём
            Item {
                id: actionsArea
                visible: msgItem.showActions
                width: msgItem.isUser ? userActions.width : rixoraActions.width
                height: msgItem.actionRowHeight
                anchors.top: bubbleRect.bottom
                anchors.topMargin: msgItem.actionRowTopMargin
                anchors.left: msgItem.isUser ? undefined : bubbleRect.left
                anchors.right: msgItem.isUser ? bubbleRect.right : undefined

                // Сообщения пользователя: скопировать, удалить, (редактировать —
                // только у самого свежего промпта)
                Row {
                    id: userActions
                    visible: msgItem.isUser
                    spacing: 6

                    MessageActionButton {
                        hoverPoint: msgItem.hoverInActions
                        iconSource: "qrc:/icons/Copy.svg"
                        onClicked: root.copyRequested(msgItem.msgId, msgItem.text)
                    }
                    MessageActionButton {
                        hoverPoint: msgItem.hoverInActions
                        iconSource: "qrc:/icons/Delete.svg"
                        onClicked: root.deleteRequested(msgItem.msgId)
                    }
                    MessageActionButton {
                        hoverPoint: msgItem.hoverInActions
                        iconSource: "qrc:/icons/Edit.svg"
                        visible: msgItem.isNewestPrompt
                        onClicked: root.editRequested(msgItem.msgId, msgItem.text)
                    }
                }

                // Сообщения Риксоры: скопировать, переписать, удалить, озвучить
                Row {
                    id: rixoraActions
                    visible: !msgItem.isUser
                    spacing: 6

                    MessageActionButton {
                        hoverPoint: msgItem.hoverInActions
                        iconSource: "qrc:/icons/Copy.svg"
                        onClicked: root.copyRequested(msgItem.msgId, msgItem.text)
                    }
                    MessageActionButton {
                        hoverPoint: msgItem.hoverInActions
                        iconSource: "qrc:/icons/Refresh.svg"
                        onClicked: root.rewriteRequested(msgItem.msgId)
                    }
                    MessageActionButton {
                        hoverPoint: msgItem.hoverInActions
                        iconSource: "qrc:/icons/Delete.svg"
                        onClicked: root.deleteRequested(msgItem.msgId)
                    }
                    MessageActionButton {
                        hoverPoint: msgItem.hoverInActions
                        iconSource: networkManager.speakingMessageId === msgItem.msgId
                                    ? "qrc:/icons/Stop.svg" : "qrc:/icons/Volume.svg"
                        onClicked: {
                            if (networkManager.speakingMessageId === msgItem.msgId) {
                                networkManager.stopTts()
                            } else {
                                root.speakRequested(msgItem.msgId)
                            }
                        }
                    }
                }
            }

            // Прозрачная "зона наведения" поверх пузыря и ряда кнопок (самый
            // верхний слой). Ловит только hover (клики пропускает к кнопкам),
            // поэтому ряд стабильно виден, пока курсор движется по сообщению
            // и по кнопкам, и не мигает на границах.
            // У баннера зона отключена целиком — он некликабельный.
            MouseArea {
                id: zoneHover
                enabled: !msgItem.isBanner
                x: Math.min(bubbleRect.x, actionsArea.x)
                y: 0
                width: Math.max(bubbleRect.x + bubbleRect.width,
                                actionsArea.x + actionsArea.width) - x + 6
                height: bubbleRect.height + actionRowTopMargin + actionRowHeight + 4
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
        }
    }
}