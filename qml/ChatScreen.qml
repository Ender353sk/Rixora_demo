// D:/Rixora/RixoraAI_UI_Qt/qml/ChatScreen.qml
import QtQuick

// ЭКРАН ЧАТА: публичный API для Main.qml и сборка слоёв.
//
// Логику окна (пароль, язык, ключи) и ВСЕ меню/диалоги держит Main.qml — сюда
// состояние приходит готовыми свойствами, а обработчики подтверждений вызывают
// функции этого экрана. Попапы намеренно остаются в корне окна: как до
// рефакторинга, иначе они теряют координаты и ввод (см. комментарий в
// ChatDeleteMenu.qml).
//
// Данные — в ChatStore, состояние и сеть — в ChatSession, плавающие слои — в
// ChatChrome. Здесь остаётся только раскладка и решения верхнего уровня.

Item {
    id: root

    // ========== ВНЕШНЕЕ СОСТОЯНИЕ (задаётся из Main.qml) ==========
    property bool isChatActive: true
    property var getText: null
    property var uiTexts: null

    // Состояние открытого меню удаления чата: им гасится подсветка строки
    // истории, по которой кликнули. Ставится из Main.qml.
    property bool menuOpen: false
    property string menuChatId: ""

    // Геометрия боковой панели нужна меню окна, чтобы вставать вровень.
    readonly property bool panelExpanded: sidePanel.expanded
    readonly property real panelExpandedWidth: sidePanel.expandedWidth
    readonly property real panelCollapsedWidth: sidePanel.collapsedWidth

    // Центральная зона чата: 78% ширины окна, отступы по 16.5% слева и справа
    readonly property real sideMargin: Math.max(width * 0.165, 48)

    // Запросы на действия, чьи попапы живут в корне окна (Main.qml).
    signal messageDeleteConfirmRequested(string messageId)
    signal messageRewriteConfirmRequested(string messageId)
    signal settingsRequested()
    signal chatDeleteMenuRequested(string chatId, real btnX, real btnY, real btnWidth, real btnHeight)

    opacity: isChatActive ? 1 : 0
    visible: opacity > 0
    scale: isChatActive ? 1 : 1.1

    Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }
    Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

    // Оба объекта невизуальные; единственный способ повлиять на экран —
    // сигнал chatViewSyncRequested, который сводится к syncChatView().
    ChatStore {
        id: store
    }

    ChatSession {
        id: session
        chatStore: store
        onChatViewSyncRequested: (scrollToBottom) => root.syncChatView(scrollToBottom)
        // Стриминг: догоняем низ только пока пользователь сам не увёл
        // прокрутку от него (см. ChatView.stickToBottom).
        onStreamTick: chatView.scrollToBottomIfAtEnd()
    }

    // ========== ОБНОВЛЕНИЕ РАЗМЕТКИ ==========
    // Общий вход для ChatStore/ChatSession: обновить индекс свежего сообщения
    // пользователя (на нём показывается кнопка правки) и там, где нужно,
    // прокрутить чат вниз.
    function syncChatView(scrollToBottom) {
        chatView.newestUserMessageIndex = store.newestUserIndex()
        if (scrollToBottom) chatView.scrollToBottom()
    }

    function loadMessages(messages) {
        chatView.newestUserMessageIndex = store.loadMessages(messages)
        // Двойная отсрочка (плюс вторая внутри scrollToBottom) — как до
        // рефакторинга: при открытии чата ListView ещё не посчитан, и
        // мгновенная прокрутка просто ничего не делает.
        Qt.callLater(chatView.scrollToBottom)
    }

    function refreshChatHistory() { store.refreshChatHistory() }

    // ========== СМЕНА ЧАТА ==========
    // Общий сброс перед сменой чата: гасим редактирование и идущую генерацию.
    function resetForChatSwitch() {
        chrome.cancelEdit()
        session.resetChatState()
    }

    function startNewChat() {
        resetForChatSwitch()
        chatView.newestUserMessageIndex = session.startNewChat()
        refreshChatHistory()
    }

    function openChat(chatId) {
        resetForChatSwitch()
        loadMessages(session.loadChat(chatId))
        refreshChatHistory()
    }

    function deleteAllChatsAndMemory() {
        resetForChatSwitch()
        chatView.newestUserMessageIndex = session.deleteAllChats()
        refreshChatHistory()
    }

    // Восстановление последней сессии при запуске и при повторном входе.
    // Повторный вызов ничего не делает: сообщения уже на экране.
    function restoreSession() {
        if (store.chatModel.count !== 0) return
        var messages = session.restoreLastSession()
        if (messages.length === 0) chatView.newestUserMessageIndex = store.addGreeting()
        else loadMessages(messages)
        refreshChatHistory()
    }

    // ========== ОБРАБОТЧИКИ ДИАЛОГОВ (сами диалоги живут в Main.qml) ==========
    // Оба вызываются только после явного подтверждения пользователем.
    function confirmDeleteMessage(messageId) { session.deleteMessage(messageId) }
    function confirmRewriteMessage(messageId) { session.rewriteMessage(messageId) }

    // ========== СПИСОК СООБЩЕНИЙ ==========
    ChatView {
        id: chatView
        anchors.fill: parent
        model: store.chatModel
        externalScrollBar: chrome.scrollBar

        topPaddingArea: 65
        bottomPaddingArea: 85

        leftMargin: root.sideMargin
        rightMargin: root.sideMargin

        // Пока Риксора обрабатывает запрос, кнопки действий над
        // сообщениями скрыты (нельзя удалять/переписывать "на лету").
        isProcessing: session.isProcessing

        onCopyRequested: (id, text) => session.copyToClipboard(text)
        onSpeakRequested: (id) => session.speakMessage(id)
        onRewriteRequested: (id) => root.messageRewriteConfirmRequested(id)
        onDeleteRequested: (id) => root.messageDeleteConfirmRequested(id)
        onEditRequested: (id, text) => chrome.startEdit(id, text)
    }

    // Плавающие слои: шапка, панель ввода, скроллбар, кнопка голосового вывода.
    ChatChrome {
        id: chrome
        anchors.fill: parent

        chatSession: session
        isProcessing: session.isProcessing
        voiceInputBusy: session.voiceInputBusy
        voiceMode: session.voiceMode
        uiTexts: root.uiTexts
        sideMargin: root.sideMargin

        onVoiceModeToggled: session.toggleVoiceMode()
    }

    // ========== БОКОВАЯ ПАНЕЛЬ (вынесена в ChatHistoryPanel) ==========
    ChatHistoryPanel {
        id: sidePanel
        sideMargin: root.sideMargin
        getText: root.getText
        historyModel: store.chatHistoryModel
        activeChatId: store.activeChatId
        menuOpen: root.menuOpen
        menuChatId: root.menuChatId

        onHistoryToggleRequested: root.refreshChatHistory()
        onNewChatRequested: root.startNewChat()
        onSettingsRequested: root.settingsRequested()
        onChatSelected: (chatId) => root.openChat(chatId)
        onChatMenuRequested: (chatId, x, y, w, h) => root.chatDeleteMenuRequested(chatId, x, y, w, h)
    }
}