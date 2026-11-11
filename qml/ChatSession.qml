// D:/Rixora/RixoraAI_UI_Qt/qml/ChatSession.qml
import QtQml

// СОСТОЯНИЕ ЧАТА И ВСЕ ВЫЗОВЫ СЕТИ.
//
// Сюда переехали флаги (идёт ли генерация, идёт ли голосовой ввод, что
// редактируется), все обращения к networkManager и реакция на ответы сети.
// Модели остаются в ChatStore: сессия пишет в них, но не владеет.
//
// Объект невизуальный (QtObject), поэтому не попадает в сцен-граф. Единственные
// способы повлиять на разметку — сигналы chatViewSyncRequested (обновить индекс
// свежего сообщения пользователя и, если нужно, прокрутить чат вниз) и
// streamTick (во время стриминга догнать низ, пока он не уведён вручную).

QtObject {
    id: root

    // Модели сообщений и истории. Ссылка передаётся из ChatScreen, потому что
    // id из одного QML-файла в другой не видны.
    property var chatStore: null

    // true, пока ИИ обрабатывает запрос.
    property bool isProcessing: false

    // true, пока идёт голосовой ввод: запись удержанием кнопки-микрофона либо
    // распознавание уже записанной речи. Нужно, чтобы кнопка действия не
    // превращалась в «стоп» раньше, чем ИИ начнёт обработку текста.
    property bool voiceInputBusy: false

    // Режим озвучивания ответов (кнопка в правом верхнем углу).
    property bool voiceMode: false

    // true, пока текст ответа приходит потоком (создан пустой пузырь).
    property bool streamActive: false

    // Буфер дельт, ещё не влитых в модель: сливается таймером, чтобы не
    // перестраивать модель на каждый кусок текста.
    property string streamBuffer: ""

    signal chatViewSyncRequested(bool scrollToBottom)

    // Чат догоняет низ только пока пользователь сам не увёл прокрутку.
    signal streamTick()

    // ==========================================
    // ПЕРЕХОДЫ МЕЖДУ ЧАТАМИ
    // ==========================================

    // Общий сброс при смене чата: гасим идущую генерацию, голосовой ввод
    // и страховочный таймер распознавания.
    function resetChatState() {
        root.voiceInputSafety.stop()
        networkManager.stopProcessing()
        isProcessing = false
        voiceInputBusy = false
        discardStream()
    }

    function startNewChat() {
        networkManager.createNewChat()
        networkManager.clearHistory()
        chatStore.clearMessages()
        return chatStore.addGreeting()
    }

    function deleteAllChats() {
        networkManager.deleteAllChats()
        chatStore.clearMessages()
        networkManager.createNewChat()
        return chatStore.addGreeting()
    }

    // ==========================================
    // ДЕЙСТВИЯ ПОЛЬЗОВАТЕЛЯ
    // ==========================================

    // Отправка реплики: ставим флаг, уходим в сеть, добавляем сообщение в чат
    // и сохраняем историю на диск.
    function sendText(text, voice) {
        isProcessing = true
        networkManager.sendMessage(text, voice)
        chatStore.append({"text": text, "isUser": true, "messageId": networkManager.lastMessageId()})
        networkManager.saveCurrentChat()
        chatViewSyncRequested(true)
    }

    function stopGeneration() {
        root.voiceInputSafety.stop()
        networkManager.stopProcessing()
        isProcessing = false
        voiceInputBusy = false
    }

    // ==========================================
    // ЖИЗНЕННЫЙ ЦИКЛ СТРИМИНГА
    // ==========================================

    // Ответ завершён корректно: таймер гасим, пузырь остаётся — его текст
    // и id уже установил finalizeAssistantMessage.
    function finishStream() {
        streamFlushTimer.stop()
        streamActive = false
        streamBuffer = ""
    }

    // Стоп/смена чата/ошибка: недокачанный пузырь убирается, чтобы в чате
    // не оставался обрезок, которого нет в памяти.
    function discardStream() {
        streamFlushTimer.stop()
        if (streamActive) {
            streamActive = false
            streamBuffer = ""
            chatStore.removeAssistantMessage()
        }
    }

    function startRecording() {
        voiceInputBusy = true
        networkManager.startRecording()
    }

    function stopRecording() {
        // Голосовой ввод ещё не завершён: сначала распознаётся речь.
        // isProcessing включим только когда текст уйдёт в ИИ.
        root.voiceInputSafety.start()
        networkManager.stopRecording()
    }

    // Правка сообщения пользователя.
    // res === 1: запущена регенерация — прежний ответ (и хвост после него)
    // уже удалены из памяти, убираем их и из чата, ждём новый и скроллим вниз.
    // res === 0: обновился только текст промпта — скроллить нельзя.
    // res === -1: запрос отклонён (не найден / текст не изменился) — чат не трогаем.
    function commitEdit(id, text) {
        var res = networkManager.editUserMessage(id, text)
        if (res === -1) return
        chatStore.setTextById(id, text)
        if (res === 1) {
            chatStore.truncateAfterId(id)
            isProcessing = true
            chatViewSyncRequested(true)
        } else {
            chatViewSyncRequested(false)
        }
    }

    function toggleVoiceMode() {
        voiceMode = !voiceMode
        networkManager.setVoiceMode(voiceMode)
        if (!voiceMode) networkManager.stopTts()
    }

    // ==========================================
    // ДЕЙСТВИЯ С СООБЩЕНИЯМИ И ЧАТАМИ
    // ==========================================

    // Сначала память, потом чат: если backend отклонил запрос, экран не меняем.
    function deleteMessage(messageId) {
        if (!networkManager.deleteChatMessage(messageId)) return
        chatStore.removeById(messageId)
        chatViewSyncRequested(false)
    }

    function rewriteMessage(messageId) {
        if (!networkManager.rewriteChatMessage(messageId)) return
        chatStore.truncateFromId(messageId)
        isProcessing = true
        chatViewSyncRequested(true)
    }

    function loadChat(chatId) { return networkManager.loadChat(chatId) }

    function restoreLastSession() { return networkManager.restoreLastSession() }

    function copyToClipboard(text) { networkManager.copyToClipboard(text) }

    function speakMessage(id) { networkManager.speakMessage(id) }

    // Страховка: если распознавание речи по какой-то причине не вернуло ни
    // результата, ни ошибки, возвращаем кнопку в обычное состояние, чтобы
    // интерфейс не «залип» в режиме голосового ввода.
    property Timer voiceInputSafety: Timer {
        interval: 15000
        repeat: false
        onTriggered: {
            if (root.voiceInputBusy) {
                root.voiceInputBusy = false
            }
        }
    }

    // Слив накопленных дельт в модель: не чаще раза в 50 мс — модель
    // перестраивает пузырь на каждый setProperty, а визуально поток от
    // частоты слива не теряется. После каждого слива — попытка держать
    // чат у низа (только если пользователь сам не увёл прокрутку).
    property Timer streamFlushTimer: Timer {
        interval: 50
        repeat: true
        onTriggered: {
            if (root.streamBuffer !== "") {
                root.chatStore.appendToAssistant(root.streamBuffer)
                root.streamBuffer = ""
            }
            root.streamTick()
        }
    }

    // ==========================================
    // РЕАКЦИИ НА СОБЫТИЯ СЕТИ
    // ==========================================
    property Connections networkEvents: Connections {
        target: networkManager
        // Первый чанк стриминга: пустой пузырь + запуск таймера слива.
        function onStreamStarted() {
            if (root.streamActive) root.discardStream()
            root.streamActive = true
            root.streamBuffer = ""
            root.chatStore.beginAssistantMessage()
            root.streamFlushTimer.start()
            root.streamTick()
        }
        // Дельта текста: копится в буфер, в модель сольёт таймер (50 мс).
        function onResponseChunk(delta) {
            if (!root.streamActive) {
                root.streamActive = true
                root.chatStore.beginAssistantMessage()
                root.streamFlushTimer.start()
            }
            root.streamBuffer += delta
        }
        // Финал: текст эталонный (его же получила память), он заменяет
        // всё, что накопили дельты, и пузырь получает настоящий messageId.
        function onResponseReceived(response) {
            root.finishStream()
            root.chatStore.finalizeAssistantMessage(response, networkManager.lastMessageId())
            networkManager.saveCurrentChat()
            root.chatViewSyncRequested(true)
        }
        // Ошибка сети/распознавания. Это НЕ сообщение Риксоры, поэтому:
        //   - недокачанный пузырь убирается (его нет в памяти);
        //   - рендерится системным баннером (isSystem), а не пузырём;
        //   - messageId пустой — баннер не адресуется и не получает кнопок;
        //   - saveCurrentChat() здесь намеренно НЕ вызывается: ошибка не
        //     должна попасть в историю чата на диск.
        function onErrorOccurred(message) {
            root.discardStream()
            root.chatStore.append({"text": message, "isUser": false, "messageId": "", "isSystem": true})
            root.chatViewSyncRequested(true)
        }
        function onUserVoiceRecognized(text) {
            root.voiceInputSafety.stop()
            root.voiceInputBusy = false
            root.isProcessing = true
            root.chatStore.append({"text": text, "isUser": true, "messageId": networkManager.lastMessageId()})
            networkManager.saveCurrentChat()
            root.chatViewSyncRequested(true)
        }
        function onProcessingFinished() {
            root.voiceInputSafety.stop()
            root.isProcessing = false
            root.voiceInputBusy = false
            // Стоп или смена чата посреди генерации: обрезок ответа
            // убираем, чтобы чат не расходился с памятью.
            root.discardStream()
        }
    }
}