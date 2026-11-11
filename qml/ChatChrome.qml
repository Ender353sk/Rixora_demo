// D:/Rixora/RixoraAI_UI_Qt/qml/ChatChrome.qml
import QtQuick

// ПЛАВАЮЩИЕ СЛОИ ПОВЕРХ СПИСКА СООБЩЕНИЙ: шапка с верхним градиентом, нижняя
// панель ввода с градиентом, полоса прокрутки и кнопка голосового вывода.
//
// Всё это «перекрывает» чат, но ничего не решает: ни сети, ни состояния здесь
// нет — только раскладка и разметка. Порядок и z внутри компонента заданы так
// же, как были у отдельных слоёв в ChatScreen до разделения файлов:
// шапка (z 2) → панель ввода (z 2) → скроллбар (z 3) → кнопка голоса (z 4).
// Сам компонент остаётся без z, поэтому эффективные значения не меняются.

Item {
    id: root

    // Поведение и сеть — в ChatSession; здесь только раскладка.
    property var chatSession: null

    // Показывает ли кнопка действия «стоп», микрофон или отправку.
    property bool isProcessing: false
    property bool voiceInputBusy: false

    // Режим озвучивания ответов: переключается кнопкой справа вверху.
    property bool voiceMode: false

    // Словарь интерфейса и отступы строки ввода (та же центральная зона, что у чата).
    property var uiTexts: null
    property real sideMargin: 0

    // Полоса прокрутки для ChatView: напрямую ссылаться на вложенный
    // ChatScrollBar из другого файла нельзя — id не пересекают границы файлов.
    readonly property var scrollBar: chatScrollBarArea.scrollBar

    // Кнопка голоса ничего не знает о сети — просто сообщает о клике.
    signal voiceModeToggled()

    // ---------- режим редактирования (вызывается из ChatScreen) ----------
    function startEdit(id, text) {
        inputBar.startEdit(id, text)
    }

    // Сброс режима редактирования перед сменой чата: очищает строку ввода
    // и снимает флаг editTargetId в ChatInputBar.
    function cancelEdit() {
        inputBar.cancelEdit()
    }

    // ========== ШАПКА С ВЕРХНИМ ГРАДИЕНТОМ ==========
    Rectangle {
        id: headerArea
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 65
        z: 2

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0B0C10" }
            GradientStop { position: 0.4; color: "#D90B0C10" }
            GradientStop { position: 0.7; color: "transparent" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // ========== ПАНЕЛЬ ВВОДА С НИЖНИМ ГРАДИЕНТОМ ==========
    Item {
        id: inputBarArea
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 85
        z: 2

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.7; color: "#D90B0C10" }
                GradientStop { position: 1.0; color: "#0B0C10" }
            }
        }

        ChatInputBar {
            id: inputBar
            anchors.left: parent.left
            anchors.leftMargin: root.sideMargin
            anchors.right: parent.right
            anchors.rightMargin: root.sideMargin
            anchors.verticalCenter: parent.verticalCenter

            isProcessing: root.isProcessing
            voiceInputBusy: root.voiceInputBusy
            uiTexts: root.uiTexts
            currentLanguage: networkManager.language

            onSendMessageRequested: (text) => root.chatSession.sendText(text, root.voiceMode)
            onStopProcessingRequested: root.chatSession.stopGeneration()
            onStartRecordingRequested: root.chatSession.startRecording()
            onStopRecordingRequested: root.chatSession.stopRecording()
            onEditCommitRequested: (id, text) => root.chatSession.commitEdit(id, text)
        }
    }

    // ========== ПОЛОСА ПРОКРУТКИ ==========
    ChatScrollBar {
        id: chatScrollBarArea
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 70
        anchors.bottomMargin: 90
    }

    // ========== КНОПКА ГОЛОСОВОГО ВЫВОДА (правый верхний угол) ==========
    MouseArea {
        id: speakerBtn
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 12
        width: 46
        height: 46
        z: 4
        hoverEnabled: true

        GlowBorder {
            anchors.fill: parent
            fill: speakerBtn.pressed ? Qt.lighter("#1F2833", 1.5) :
                  (speakerBtn.containsMouse ? "#1F2833" : "#0B0C10")
            glow: "#66FCF1"
            glowVisible: root.voiceMode
            cornerRadius: 23
        }

        Image {
            anchors.fill: parent
            smooth: true
            source: root.voiceMode ? "qrc:/icons/Volume.svg" : "qrc:/icons/VolumeOff.svg"
            sourceSize: Qt.size(48, 48)
            fillMode: Image.PreserveAspectFit
        }

        onClicked: root.voiceModeToggled()
    }
}