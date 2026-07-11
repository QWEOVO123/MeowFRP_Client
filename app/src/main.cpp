#include "app_controller.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("frp-control-client");
    QGuiApplication::setOrganizationName("frp-control");
    QGuiApplication::setApplicationVersion(APP_VERSION);
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/app_icon_256.png")));
    QQuickStyle::setStyle("Basic");

    qRegisterMetaType<ResourcePolicyResponse>("ResourcePolicyResponse");
    qRegisterMetaType<BootstrapResponse>("BootstrapResponse");
    qRegisterMetaType<HeartbeatResponse>("HeartbeatResponse");

    AppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController", &controller);

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList qmlCandidates{
        QDir(appDir).filePath(QStringLiteral("qml/Main.qml")),
        QDir(appDir).filePath(QStringLiteral("FrpControlClient/qml/Main.qml")),
    };
    QUrl mainUrl(QStringLiteral("qrc:/FrpControlClient/qml/Main.qml"));
    for (const auto &path : qmlCandidates) {
        if (QFileInfo::exists(path)) {
            mainUrl = QUrl::fromLocalFile(path);
            break;
        }
    }

    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings) {
            qWarning() << warning.toString();
        }
    });
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}
