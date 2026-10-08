#pragma once

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QPointer>
#include <QWindow>

class AppearanceController : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY changed)
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(bool glassEnabled READ glassEnabled WRITE setGlassEnabled NOTIFY changed)
    Q_PROPERTY(bool backdropActive READ backdropActive NOTIFY changed)
    Q_PROPERTY(bool highContrast READ highContrast NOTIFY changed)
    Q_PROPERTY(QString materialDescription READ materialDescription NOTIFY changed)
public:
    explicit AppearanceController(bool preview = false, QObject *parent = nullptr);
    ~AppearanceController() override;
    QString themeMode() const { return m_mode; }
    bool dark() const { return m_dark; }
    bool glassEnabled() const { return m_glass; }
    bool backdropActive() const { return m_backdrop; }
    bool highContrast() const { return m_highContrast; }
    QString materialDescription() const;
    void setThemeMode(const QString &mode);
    void setGlassEnabled(bool enabled);
    void attachWindow(QWindow *window);
    bool nativeEventFilter(const QByteArray &, void *, qintptr *) override;
signals:
    void changed();
private:
    void refresh();
    void persist();
    bool m_preview = false;
    QString m_mode = "system";
    bool m_dark = false;
    bool m_glass = true;
    bool m_backdrop = false;
    bool m_highContrast = false;
    QPointer<QWindow> m_window;
};
