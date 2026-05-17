#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <QtQuickControls2/QQuickStyle>

#include "qt_app_bridge.hpp"

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle("Fusion");
    QQuickStyle::setFallbackStyle("Fusion");
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    OctraAppBridge bridge;
    engine.rootContext()->setContextProperty("octraBridge", &bridge);
    engine.load(QUrl(QStringLiteral("qrc:/OctraWallet/native/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
