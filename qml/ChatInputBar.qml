// D:/Rixora/RixoraAI_UI_Qt/qml/ChatInputBar.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root
    spacing: 10

    property bool isProcessing: false
    // true, пока идёт голосовой ввод: запись удержанием кнопки либо распознавание
    // уже записанной речи. Пока флаг стоит, кнопка остаётся микрофоном.
    property bool voiceInputBusy: false
    property var uiTexts
    property string currentLanguage: "ru"

    // Режим редактирования сообщения: та же строка ввода, но вместо отправки
    // нового промпта изменения коммитятся через editCommitRequested.
    property string editTargetId: ""
    readonly property bool isEditMode: editTargetId !== ""

    signal sendMessageRequested(string text)
    signal stopProcessingRequested()
    signal startRecordingRequested()
    signal stopRecordingRequested()
    signal editCommitRequested(string id, string text)

    function clearEditState() {
        root.editTargetId = ""
        messageInput.text = ""
    }

    function startEdit(id, text) {
        root.editTargetId = id
        messageInput.text = text
        messageInput.forceActiveFocus()
    }

    function cancelEdit() {
        clearEditState()
    }

    function commitEdit() {
        if (!root.isEditMode) return
        var id = root.editTargetId
        var newText = messageInput.text
        clearEditState()
        if (newText.trim() !== "") {
            root.editCommitRequested(id, newText)
        }
    }

    function placeholderText() {
        var dict = root.uiTexts && root.uiTexts[root.currentLanguage]
            ? root.uiTexts[root.currentLanguage] : root.uiTexts["ru"]
        return root.isEditMode ? dict.editPrompt : dict.prompt
    }

    // Кнопка-заглушка слева от строки ввода. В обычном режиме — белый плюс
    // (без функций), в режиме редактирования тот же знак поворачивается
    // в крестик и отменяет редактирование.
    MouseArea {
        id: newChatBtn
        Layout.preferredWidth: 50
        Layout.preferredHeight: 50
        hoverEnabled: true

        GlowBorder {
            anchors.fill: parent
            fill: newChatBtn.pressed ? Qt.lighter("#1F2833", 1.5) :
                  (newChatBtn.containsMouse ? "#1F2833" : "#0B0C10")
            glow: root.isEditMode ? "#58A6FF" : "#66FCF1"
            cornerRadius: 25
        }

        // В режиме редактирования знак плюса поворачивается в крестик:
        // та же кнопка, того же размера, останавливает редактирование.
        Image {
            anchors.fill: parent
            smooth: true
            source: "qrc:/icons/Add.svg"
            sourceSize: Qt.size(48, 48)
            fillMode: Image.PreserveAspectFit
            // Вращаем сам знак, чтобы получился крестик отмены правки
            rotation: root.isEditMode ? 45 : 0
        }

        onClicked: {
            if (root.isEditMode) root.cancelEdit()
        }
    }

    TextField {
        id: messageInput
        Layout.fillWidth: true
        Layout.preferredHeight: 50
        placeholderText: root.placeholderText()
        placeholderTextColor: "#888888"
        color: "#FFFFFF"
        font.pixelSize: 16

        background: GlowBorder {
            fill: "#1F2833"
            glow: root.isEditMode ? "#58A6FF" : "#66FCF1"
            glowVisible: messageInput.activeFocus || root.isEditMode
            cornerRadius: 25
        }

        Keys.onEscapePressed: {
            if (root.isEditMode) {
                root.cancelEdit()
            }
        }

        // ИСПРАВЛЕННЫЙ обработчик onAccepted
        onAccepted: {
            if (root.isEditMode) {
                root.commitEdit()
            } else if (root.isProcessing) {
                // Если идет обработка, Enter всегда должен останавливать
                root.stopProcessingRequested()
            } else if (!root.voiceInputBusy && messageInput.text.trim() !== "") {
                // Если обработки нет и есть текст, отправляем сообщение.
                // Во время распознавания речи ввод игнорируем, чтобы не уйти
                // двумя запросами одновременно.
                root.sendMessageRequested(messageInput.text)
                messageInput.text = ""
            }
        }
    }

    // ЕДИНАЯ КНОПКА ДЕЙСТВИЯ: Микрофон / Отправить / Стоп.
    // - при пустом тексте — микрофон (удержание = запись голоса);
    // - при непустом тексте — отправка (➤);
    // - во время генерации ответа ИИ — стоп (⏹).
    // Пока идёт голосовой ввод (recording/распознавание) кнопка остаётся
    // микрофоном и мягко подсвечивается, а не превращается в стоп.
    MouseArea {
        id: actionBtn
        Layout.preferredWidth: 50
        Layout.preferredHeight: 50
        hoverEnabled: true

        property bool recording: false
        property bool showStop: root.isProcessing && !root.voiceInputBusy
        property bool showSend: !root.isProcessing && !root.voiceInputBusy && messageInput.text.trim() !== ""
        property bool showMic: !root.isProcessing && !root.voiceInputBusy && messageInput.text.trim() === ""
        property bool micHighlight: root.voiceInputBusy
        property real pulsePeak: 0.6

        // Лёгкое «дыхание» свечения, пока идёт голосовой ввод.
        SequentialAnimation on pulsePeak {
            running: actionBtn.micHighlight
            loops: Animation.Infinite
            NumberAnimation { from: 0.3; to: 0.95; duration: 650; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.95; to: 0.3; duration: 650; easing.type: Easing.InOutSine }
        }

        GlowBorder {
            anchors.fill: parent
            fill: actionBtn.pressed ? Qt.lighter("#1F2833", 1.5) :
                  (actionBtn.containsMouse ? "#1F2833" : "#0B0C10")
            glow: actionBtn.showStop ? "#FF4C4C" : (root.isEditMode ? "#58A6FF" : "#66FCF1")
            glowPeak: actionBtn.micHighlight ? actionBtn.pulsePeak : 0.6
            cornerRadius: 25
        }

        // Стоп: готовая SVG-иконка (qrc:/icons/Stop.svg).
        Image {
            anchors.fill: parent
            visible: actionBtn.showStop
            source: "qrc:/icons/Stop.svg"
            sourceSize: Qt.size(48, 48)
            smooth: true
            fillMode: Image.PreserveAspectFit
        }

        // Микрофон / Отправить: готовые SVG-иконки, заполняют кнопку целиком
        // (фон значка совпадает с фоном кнопки).
        Image {
            anchors.fill: parent
            visible: !actionBtn.showStop
            source: actionBtn.showSend ? "qrc:/icons/Send.svg" : "qrc:/icons/Mic.svg"
            sourceSize: Qt.size(48, 48)
            smooth: true
            fillMode: Image.PreserveAspectFit
        }

        onPressed: {
            if (actionBtn.showMic) {
                actionBtn.recording = true
                root.startRecordingRequested()
            }
        }
        onReleased: {
            if (actionBtn.recording) {
                actionBtn.recording = false
                root.stopRecordingRequested()
            }
        }
        onClicked: {
            if (actionBtn.showStop) {
                root.stopProcessingRequested()
            } else if (actionBtn.showSend) {
                if (root.isEditMode) {
                    root.commitEdit()
                } else {
                    root.sendMessageRequested(messageInput.text)
                    messageInput.text = ""
                }
            }
        }
    }
}