#include "appearance_controller.h"
#include "ui_preview_controller.h"
#include <QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSettings>
#include <QFontDatabase>
#include <QDir>
#include <memory>

class ClientUiTest : public QObject {
    Q_OBJECT
    std::unique_ptr<AppearanceController> appearance;
    std::unique_ptr<UiPreviewController> demo;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QStringList warnings;
    QQuickWindow *window = nullptr;
    bool load(const QString &page) {
        appearance = std::make_unique<AppearanceController>(true);
        appearance->setThemeMode("light");
        demo = std::make_unique<UiPreviewController>(page);
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->rootContext()->setContextProperty("appearance", appearance.get());
        engine->rootContext()->setContextProperty("appController", demo.get());
        engine->rootContext()->setContextProperty("uiPreviewMode", true);
        engine->rootContext()->setContextProperty("uiPreviewPage", page);
        connect(engine.get(), &QQmlApplicationEngine::warnings, this, [this](const QList<QQmlError> &errors) {
            for (const auto &error : errors) warnings << error.toString();
        });
        engine->load(QUrl::fromLocalFile(QStringLiteral(CLIENT_UI_SOURCE)));
        if (engine->rootObjects().isEmpty()) return false;
        window = qobject_cast<QQuickWindow *>(engine->rootObjects().first());
        return window != nullptr;
    }
private slots:
    void initTestCase() {
        QQuickStyle::setStyle("Basic");
        for (const QString &name : {QString("msyh.ttc"), QString("msyhbd.ttc"), QString("segoeui.ttf"), QString("segmdl2.ttf"), QString("consola.ttf")})
            QFontDatabase::addApplicationFont(QDir(qEnvironmentVariable("WINDIR", "C:/Windows")).filePath("Fonts/" + name));
    }
    void cleanup() { engine.reset(); window = nullptr; demo.reset(); appearance.reset(); warnings.clear(); }
    void allPagesAdaptToLightAndDark_data() {
        QTest::addColumn<QString>("page");
        for (const auto &page : {"login", "dashboard", "nodes", "logs", "settings"}) QTest::newRow(page) << QString(page);
    }
    void allPagesAdaptToLightAndDark() {
        QFETCH(QString, page);
        QVERIFY(load(page));
        QTRY_VERIFY(!window->property("dark").toBool());
        appearance->setThemeMode("dark");
        QTRY_VERIFY(window->property("dark").toBool());
        appearance->setThemeMode("light");
        QTRY_VERIFY(!window->property("dark").toBool());
        appearance->setThemeMode("invalid");
        QCOMPARE(appearance->themeMode(), QString("light"));
        appearance->setGlassEnabled(false);
        QVERIFY(!appearance->backdropActive());
        QTest::qWait(30);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void navigationAndScrollKeepControlsAvailableAtMinimumSize() {
        QVERIFY(load("dashboard"));
        window->resize(880, 640);
        auto *remote = window->findChild<QObject *>("remotePort");
        auto *local = window->findChild<QObject *>("localPort");
        QVERIFY(remote && local);
        QCOMPARE(remote->property("from").toInt(), 10000);
        QCOMPARE(remote->property("to").toInt(), 30000);
        QCOMPARE(local->property("value").toInt(), 22);
        auto *scroll = window->findChild<QObject *>("tunnelScroll");
        QVERIFY(scroll);
        QTRY_VERIFY(scroll->property("contentHeight").toReal() > scroll->property("availableHeight").toReal());
        auto *logs = window->findChild<QObject *>("navLogs");
        QVERIFY(logs && QMetaObject::invokeMethod(logs, "clicked"));
        QCOMPARE(window->property("pageIndex").toInt(), 1);
        auto *settings = window->findChild<QObject *>("navSettings");
        QVERIFY(settings && QMetaObject::invokeMethod(settings, "clicked"));
        QCOMPARE(window->property("pageIndex").toInt(), 2);
        auto *settingsScroll = window->findChild<QObject *>("settingsScroll");
        QVERIFY(settingsScroll);
        QTRY_VERIFY(settingsScroll->property("contentHeight").toReal() > settingsScroll->property("availableHeight").toReal());
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void nodeSelectionTransitionsWithoutProductionServices() {
        QVERIFY(load("nodes"));
        auto *dialog = window->findChild<QObject *>("nodeSelectionDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("visible").toBool());
        QVERIFY(!demo->value("connected").toBool());
        QCOMPARE(demo->value("nodeList").toList().size(), 3);
        demo->selectNode(0);
        QTRY_VERIFY(window->findChild<QObject *>("navTunnels") != nullptr);
        QVERIFY(demo->value("connected").toBool());
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void demoThemeChangesDoNotPersistPreferences() {
        QSettings settings;
        const auto beforeMode = settings.value("appearance/theme");
        const auto beforeGlass = settings.value("appearance/glass");
        AppearanceController isolated(true);
        isolated.setThemeMode("dark"); isolated.setGlassEnabled(false); isolated.setThemeMode("system");
        QCOMPARE(settings.value("appearance/theme"), beforeMode);
        QCOMPARE(settings.value("appearance/glass"), beforeGlass);
    }
};

QTEST_MAIN(ClientUiTest)
#include "ui_test.moc"
