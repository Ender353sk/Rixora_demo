// D:/Rixora/RixoraAI_UI_Qt/qml/ChatStore.qml
import QtQml

// ДАННЫЕ ЧАТА: обе ListModel и все операции над ними.
//
// Здесь нет ни состояния, ни вызовов сети — только модели и работа с ними.
// Поведение (переключение чатов, голосовой ввод, реакция на ответы) живёт в
// ChatSession, разметка — в ChatScreen.
//
// ChatStore — невизуальный объект (QtObject), поэтому не попадает в сцен-граф
// и не влияет ни на отрисовку, ни на раскладку, ни на скорость ответов.

QtObject {
    id: root

    // ========== МОДЕЛИ ==========
    // Объекты объявлены свойствами, а не детьми: у QtObject нет default
    // property, поэтому ListModel/Connections нельзя просто вложить
    // (qmllint: missing-property). Так они остаются невизуальными.
    property ListModel messagesModel: ListModel {}
    property ListModel historyModel: ListModel {}

    // Наружу модели отдаются как var: property alias на объект, объявленный
    // свойством, qmllint не резолвит (unresolved-alias).
    readonly property var chatModel: messagesModel
    readonly property var chatHistoryModel: historyModel

    // Идентификатор чата, подсвеченного в боковой панели. Обновляется из
    // refreshChatHistory(), потому что у контекстного свойства chatManager нет
    // сигнала об изменении и привязка вычислялась бы один раз.
    property string activeChatId: ""

    // Алиас контекстного свойства chatManager: обращения к нему идут только
    // через root.chatApi, чтобы не полагаться на неквалифицированный поиск.
    readonly property var chatApi: chatManager

    // ==========================================
    // СООБЩЕНИЯ
    // ==========================================

    // Индекс самого свежего сообщения пользователя (у него показывается кнопка
    // редактирования). -1 = такого сообщения в чате нет.
    function newestUserIndex() {
        for (var i = messagesModel.count - 1; i >= 0; i--) {
            if (messagesModel.get(i).isUser) return i
        }
        return -1
    }

    function loadMessages(messages) {
        messagesModel.clear()
        for (var i = 0; i < messages.length; i++) {
            var m = messages[i]
            var text = ""
            var isUser = false
            if (m.parts !== undefined && m.parts.length > 0) {
                text = m.parts[0].text
                isUser = (m.role === "user")
            } else {
                text = m.text
                isUser = m.isUser
            }
            messagesModel.append({"text": text, "isUser": isUser, "messageId": m.id !== undefined ? m.id : ""})
        }
        return newestUserIndex()
    }

    function append(obj) {
        messagesModel.append(obj)
        return newestUserIndex()
    }

    // Приветствие: сообщение Риксоры без id, поэтому действий над ним нет.
    function addGreeting() {
        return append({"text": networkManager.getGreeting(), "isUser": false})
    }

    // ==========================================
    // СТРИМИНГ ОТВЕТА ИИ
    // ==========================================
    // Пустой пузырь создаётся в момент streamStarted, текст дописывается
    // дельтами (appendToAssistant) и заменяется целиком финальным ответом
    // (finalizeAssistantMessage) — он эталонный: его же получает память.
    // Недокачанный пузырь (стоп/ошибка) убирается через removeAssistant.

    function beginAssistantMessage() {
        messagesModel.append({"text": "", "isUser": false, "messageId": ""})
    }

    function appendToAssistant(delta) {
        if (!delta) return
        var i = messagesModel.count - 1
        if (i < 0) return
        var m = messagesModel.get(i)
        // Пузырь стриминга — только последняя строка, без id и без баннера.
        if (m.isUser || m.isSystem === true || m.messageId !== "") return
        messagesModel.setProperty(i, "text", m.text + delta)
    }

    function finalizeAssistantMessage(text, messageId) {
        var i = messagesModel.count - 1
        if (i >= 0) {
            var m = messagesModel.get(i)
            if (!m.isUser && m.isSystem !== true && m.messageId === "") {
                messagesModel.setProperty(i, "text", text)
                messagesModel.setProperty(i, "messageId", messageId)
                return
            }
        }
        // Пузыря нет (ответ пришёл без стриминга) — добавляем как раньше.
        append({"text": text, "isUser": false, "messageId": messageId})
    }

    function removeAssistantMessage() {
        var i = messagesModel.count - 1
        if (i < 0) return
        var m = messagesModel.get(i)
        if (m.isUser || m.isSystem === true || m.messageId !== "") return
        messagesModel.remove(i)
    }

    function clearMessages() {
        messagesModel.clear()
    }

    function removeById(id) {
        for (var i = 0; i < messagesModel.count; i++) {
            if (messagesModel.get(i).messageId === id) {
                messagesModel.remove(i)
                break
            }
        }
        return newestUserIndex()
    }

    function setTextById(id, text) {
        for (var i = 0; i < messagesModel.count; i++) {
            if (messagesModel.get(i).messageId === id) {
                messagesModel.setProperty(i, "text", text)
                break
            }
        }
    }

    // Удаляет из модели всё, что шло ПОСЛЕ сообщения с данным id (само сообщение
    // остаётся). Используется при правке промпта: изменённый промпт должен
    // продолжить отображаться, пока Риксора переписывает ответ.
    function truncateAfterId(id) {
        for (var i = 0; i < messagesModel.count; i++) {
            if (messagesModel.get(i).messageId === id) {
                while (messagesModel.count > i + 1) {
                    messagesModel.remove(i + 1)
                }
                break
            }
        }
        return newestUserIndex()
    }

    // Удаляет само сообщение и всё, что шло после него (переписывание ответа).
    function truncateFromId(id) {
        truncateAfterId(id)
        return removeById(id)
    }

    // ==========================================
    // ИСТОРИЯ ЧАТОВ
    // ==========================================
    function refreshChatHistory() {
        activeChatId = root.chatApi.getCurrentChatId()
        var list = root.chatApi.getChatList()
        historyModel.clear()
        for (var i = 0; i < list.length; i++) {
            var item = list[i]
            historyModel.append({
                chatId: item.id,
                title: item.title,
                date: item.updatedAt,
                count: item.messageCount
            })
        }
    }

    // У контекстного свойства chatManager нет дочерних элементов, поэтому
    // сигнал об изменении списка чатов ловится здесь — ровно один раз.
    property Connections historyEvents: Connections {
        target: root.chatApi
        function onChatListChanged() {
            root.refreshChatHistory()
        }
    }
}