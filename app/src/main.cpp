#include "app_controller.h"
#include "appearance_controller.h"
#include "ui_preview_controller.h"
#include "log_safety.h"

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
#include <QQuickWindow>
#include <QTimer>
#include <QImage>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <memory>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("frp-control-client");
    QGuiApplication::setOrganizationName("frp-control");
    QGuiApplication::setApplicationVersion(APP_VERSION);
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/app_icon_256.png")));
    QQuickStyle::setStyle("Basic");
    QQuickWindow::setDefaultAlphaBuffer(true);

    qRegisterMetaType<ResourcePolicyResponse>("ResourcePolicyResponse");
    qRegisterMetaType<BootstrapResponse>("BootstrapResponse");
    qRegisterMetaType<HeartbeatResponse>("HeartbeatResponse");

    const QStringList args = app.arguments();
    const bool preview = args.contains("--ui-preview");
    // The offscreen platform has no native Windows font discovery. Use the
    // machine's installed fonts for accurate QA renders; never redistribute them.
    if (preview && QGuiApplication::platformName() == "offscreen") {
        for (const QString &name : {QString("msyh.ttc"), QString("msyhbd.ttc"), QString("segoeui.ttf"), QString("segmdl2.ttf"), QString("consola.ttf")})
            QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts/" + name));
    }
    auto optionValue = [&args](const QString &name) {
        const int index = args.indexOf(name);
        return index >= 0 ? args.value(index + 1) : QString{};
    };
    const QString previewPage = optionValue("--ui-preview");
    AppearanceController appearance(preview);
    if (preview && !optionValue("--ui-theme").isEmpty()) appearance.setThemeMode(optionValue("--ui-theme"));
    std::unique_ptr<AppController> controller;
    std::unique_ptr<UiPreviewController> demo;
    QObject *uiController = nullptr;
    if (preview) { demo = std::make_unique<UiPreviewController>(previewPage); uiController = demo.get(); }
    else { controller = std::make_unique<AppController>(); uiController = controller.get(); }
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController", uiController);
    engine.rootContext()->setContextProperty("appearance", &appearance);
    engine.rootContext()->setContextProperty("uiPreviewMode", preview);
    engine.rootContext()->setContextProperty("uiPreviewPage", previewPage);

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

    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [&controller](const QList<QQmlError> &warnings) {
        for (const auto &warning : warnings) {
            const auto message = LogSafety::redact(warning.toString());
            qWarning() << message;
            if (controller) controller->recordUiWarning(message);
        }
    });
    engine.load(mainUrl);
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (window) appearance.attachWindow(window);
    // Offscreen visual regression rendering, available only in isolated demo mode.
    const QString screenshot = optionValue("--ui-screenshot");
    const QString report = optionValue("--ui-report");
    if (preview && window && !report.isEmpty()) {
        QTimer::singleShot(1200, &app, [&appearance, window, report, screenshot] {
            QFile file(report);
            const QJsonObject data{{"platform", QGuiApplication::platformName()}, {"theme", appearance.themeMode()}, {"dark", appearance.dark()}, {"backdrop_active", appearance.backdropActive()}, {"high_contrast", appearance.highContrast()}, {"material", appearance.materialDescription()}, {"width", window->width()}, {"height", window->height()}};
            const bool ok = file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(data).toJson()) > 0;
            if (screenshot.isEmpty()) QCoreApplication::exit(ok ? 0 : 2);
        });
    }
    if (preview && window && !screenshot.isEmpty()) {
        const QString size = optionValue("--ui-size");
        const QStringList dimensions = size.split('x');
        if (dimensions.size() == 2 && dimensions[0].toInt() >= 880 && dimensions[1].toInt() >= 640)
            window->resize(dimensions[0].toInt(), dimensions[1].toInt());
        QTimer::singleShot(1200, &app, [window, screenshot] {
            const QImage image = window->grabWindow();
            QCoreApplication::exit(!image.isNull() && image.save(screenshot) ? 0 : 2);
        });
    }
    return app.exec();
}
