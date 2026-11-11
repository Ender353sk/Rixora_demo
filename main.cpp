#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include "networkmanager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Указываем данные для QSettings (сохранение ключей)
    app.setOrganizationName("RixoraTeam");
    app.setOrganizationDomain("rixora.ai");
    app.setApplicationName("RixoraAI");

    // Стиль Controls 2 обязан быть задан ДО загрузки QML. На Windows по
    // умолчанию включается нативный стиль, который игнорирует кастомизацию
    // contentItem/background у Menu, ScrollBar, Switch и Dialog. Из-за этого
    // тёмная тема из qml/*.qml молча не применялась, а Qt сыпал ~60
    // предупреждений "current style does not support customization".
    // Basic — штатный не-нативный стиль, который уважает наши стили.
    QQuickStyle::setStyle("Basic");

    // ВАЖНО: NetworkManager объявлен ДО движка QML. Объекты уничтожаются в
    // обратном порядке, поэтому при выходе сначала разрушится engine, и только
    // потом networkManager. При обратном объявлении QML продолжал бы обращаться
    // к уже разрушенному объекту во время разбора компонентов, что давало
    // десятки "Cannot read property 'language' of null" в логе.
    NetworkManager networkManager;

    QQmlApplicationEngine engine;

    // Создаем объект нашего класса и передаем его в QML под именем "networkManager"
    engine.rootContext()->setContextProperty("networkManager", &networkManager);

    // История чатов.
    // ВАЖНО: "chatManager" — это АЛИАС того же NetworkManager, а не отдельный
    // объект ChatManager. Исторически он экспортировался именно так, и QML
    // обращается к нему за историей, текущим чатом и удалением. Если переименовать
    // контекстное свойство или подсунуть сюду настоящий ChatManager, QML потеряет
    // всю цепочку удаления памяти: deleteChat/deleteChatMessage умеют вычищать
    // факты из долговременной памяти, а голый ChatManager — нет.
    engine.rootContext()->setContextProperty("chatManager", &networkManager);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("RixoraAI", "Main");

    return app.exec();
}
