import QtQuick
import "translations.js" as Translations

// КОРЕНЬ ПРИЛОЖЕНИЯ: окно, вход и меню/диалоги уровня окна.
//
// Содержимое чата разнесено по четырём файлам: ChatScreen (эта сборка и
// публичный API), ChatStore (модели), ChatSession (состояние и сеть),
// ChatChrome (плавающие слои). Здесь остаётся только то, что меню и попапы
// получают от окна: ключи/язык/пароль, переключение «вход ↔ чат» и сами меню.
//
// Меню и диалоги объявлены именно здесь, а не внутри ChatScreen: попапу нужен
// корень окна, иначе он теряет координаты и ввод (см. комментарий в
// ChatDeleteMenu.qml). Поэтому ChatScreen обращается к ним сигналами.
Window {
    id: mainWindow
    width: Screen.desktopAvailableWidth * 0.78
    height: Screen.desktopAvailableHeight * 0.8
    minimumWidth: 520
    minimumHeight: 600
    visible: true
    title: qsTr("RixoraAI")

    // Центрирование окна относительно рабочей области экрана
    function centerOnScreen() {
        x = Math.round((Screen.desktopAvailableWidth - width) / 2)
        y = Math.round((Screen.desktopAvailableHeight - height) / 2)
    }

    // Темный футуристичный фон
    color: "#0B0C10"

    // true = ключи уже сохранены, показываем чат; false = экран входа.
    property bool isChatActive: networkManager.loadSavedKeys()

    // СЛОВАРЬ ПЕРЕВОДОВ ИНТЕРФЕЙСА (вынесен в translations.js)
    property var uiTexts: Translations.uiTexts

    function lang(key) {
        return Translations.lang(networkManager.language, key)
    }

    Component.onCompleted: {
        centerOnScreen()
        if (isChatActive) chatScreen.restoreSession()
    }

    // ==========================================
    // ЭКРАН 1: АВТОРИЗАЦИЯ
    // ==========================================
    LoginView {
        id: loginScreen
        anchors.centerIn: parent
        uiTexts: mainWindow.uiTexts
        currentLanguage: networkManager.language
        initialGeminiKey: networkManager.getGeminiKey()
        initialGroqKey: networkManager.getGroqKey()

        opacity: mainWindow.isChatActive ? 0 : 1
        visible: opacity > 0
        scale: mainWindow.isChatActive ? 0.9 : 1

        Behavior on opacity { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }
        Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }

        onLoginRequested: (geminiKey, groqKey) => {
            networkManager.saveApiKeys(geminiKey, groqKey)
            // restoreSession() сам ничего не делает, если чат уже не пуст.
            chatScreen.restoreSession()
            mainWindow.isChatActive = true
        }
    }

    // ==========================================
    // ЭКРАН 2: ЧАТ ИИ (вынесен в ChatScreen)
    // ==========================================
    ChatScreen {
        id: chatScreen
        anchors.fill: parent

        isChatActive: mainWindow.isChatActive
        getText: mainWindow.lang
        uiTexts: mainWindow.uiTexts

        onMessageRewriteConfirmRequested: (messageId) => {
            msgRewritePopup.pendingRewriteId = messageId
            msgRewritePopup.open()
        }
        onMessageDeleteConfirmRequested: (messageId) => {
            msgDeletePopup.pendingDeleteId = messageId
            msgDeletePopup.open()
        }
        onSettingsRequested: {
            if (settingsMenu.opened) settingsMenu.close()
            else {
                languageMenu.close()
                personalizationMenu.close()
                settingsMenu.open()
            }
        }
        onChatDeleteMenuRequested: (chatId, btnX, btnY, btnW, btnH) => {
            chatDeleteMenu.showFor(chatId, btnX, btnY, btnW, btnH)
        }
    }

    // ==========================================
    // ПОДТВЕРЖДЕНИЕ ПОЛНОЙ ОЧИСТКИ (вынесено в DeleteConfirmDialog)
    // ==========================================
    DeleteConfirmDialog {
        id: deleteConfirmPopup
        getText: mainWindow.lang
        onConfirmed: {
            // ВАЖНО: этот диалог сам себя не закрывает — в отличие от
            // MessageDeleteDialog/MessageRewriteDialog, где close() есть прямо
            // в кнопке подтверждения. Поэтому закрываем явно, иначе после
            // очистки он остался бы висеть поверх чата.
            deleteConfirmPopup.close()
            chatScreen.deleteAllChatsAndMemory()
        }
    }

    // ==========================================
    // ПОДТВЕРЖДЕНИЕ УДАЛЕНИЯ ОТДЕЛЬНОГО СООБЩЕНИЯ
    // ==========================================
    MessageDeleteDialog {
        id: msgDeletePopup
        getText: mainWindow.lang
        onConfirmed: (messageId) => chatScreen.confirmDeleteMessage(messageId)
    }

    // ==========================================
    // ПОДТВЕРЖДЕНИЕ ПЕРЕГЕНЕРАЦИИ ОТВЕТА
    // ==========================================
    MessageRewriteDialog {
        id: msgRewritePopup
        getText: mainWindow.lang
        onConfirmed: (messageId) => chatScreen.confirmRewriteMessage(messageId)
    }

    // ==========================================
    // ПОПАПЫ (МЕНЮ НАСТРОЕК И ЯЗЫКОВ)
    // ==========================================
    SettingsMenu {
        id: settingsMenu
        getText: mainWindow.lang
        panelExpanded: chatScreen.panelExpanded
        panelExpandedWidth: chatScreen.panelExpandedWidth
        panelCollapsedWidth: chatScreen.panelCollapsedWidth

        onKeysRequested: mainWindow.isChatActive = false
        onLanguageRequested: languageMenu.open()
        onPersonalizeRequested: personalizationMenu.open()
        onDeleteAllRequested: deleteConfirmPopup.open()
    }

    // ==========================================
    // МЕНЮ УДАЛЕНИЯ ОТДЕЛЬНОГО ЧАТА (уровень окна — как SettingsMenu,
    // чтобы клик по "Удалить чат" гарантированно попадал в кнопку)
    // ==========================================
    ChatDeleteMenu {
        id: chatDeleteMenu
        getText: mainWindow.lang

        onOpened: {
            chatScreen.menuOpen = true
            chatScreen.menuChatId = chatDeleteMenu.targetChatId
        }
        onClosed: {
            chatScreen.menuOpen = false
            chatScreen.menuChatId = ""
        }

        onDeleteRequested: (chatId) => {
            // Признак «это текущий чат» снимаем ДО удаления: после
            // getCurrentChatId() уже не вернёт прежний id.
            var wasCurrent = chatManager.getCurrentChatId() === chatId
            chatManager.deleteChat(chatId)
            chatScreen.refreshChatHistory()
            if (wasCurrent) {
                chatScreen.startNewChat()
            }
        }
    }

    LanguagePickerMenu {
        id: languageMenu
        panelExpanded: chatScreen.panelExpanded
        panelExpandedWidth: chatScreen.panelExpandedWidth
        panelCollapsedWidth: chatScreen.panelCollapsedWidth
    }

    PersonalizationMenu {
        id: personalizationMenu
        getText: mainWindow.lang
        panelExpanded: chatScreen.panelExpanded
        panelExpandedWidth: chatScreen.panelExpandedWidth
        panelCollapsedWidth: chatScreen.panelCollapsedWidth
    }
}