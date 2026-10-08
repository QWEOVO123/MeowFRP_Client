#include "appearance_controller.h"
#include <QGuiApplication>
#include <QStyleHints>
#include <QSettings>
#include <QTimer>
#include <QPalette>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

AppearanceController::AppearanceController(bool preview, QObject *parent)
    : QObject(parent), m_preview(preview)
{
    if (!preview) {
        QSettings settings;
        m_mode = settings.value("appearance/theme", "system").toString();
        if (m_mode != "light" && m_mode != "dark") m_mode = "system";
        m_glass = settings.value("appearance/glass", true).toBool();
    }
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] { refresh(); });
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this] { refresh(); });
    qGuiApp->installNativeEventFilter(this);
    refresh();
}

AppearanceController::~AppearanceController() { qGuiApp->removeNativeEventFilter(this); }

QString AppearanceController::materialDescription() const
{
    if (m_highContrast) return "高对比度模式 · 透明效果已关闭";
    if (m_backdrop) return "Windows Mica · 原生系统背景材质";
    if (!m_glass) return "实色模式 · 已关闭透明效果";
    return "实色兼容模式 · 当前系统不支持或未启用透明材质";
}

void AppearanceController::setThemeMode(const QString &mode)
{
    if (mode != "system" && mode != "light" && mode != "dark") return;
    if (m_mode == mode) return;
    m_mode = mode; persist(); refresh();
}

void AppearanceController::setGlassEnabled(bool enabled)
{
    if (m_glass == enabled) return;
    m_glass = enabled; persist(); refresh();
}

void AppearanceController::persist()
{
    if (m_preview) return;
    QSettings settings;
    settings.setValue("appearance/theme", m_mode);
    settings.setValue("appearance/glass", m_glass);
}

void AppearanceController::attachWindow(QWindow *window)
{
    m_window = window;
    connect(window, &QWindow::visibleChanged, this, [this] { refresh(); });
    refresh();
}

bool AppearanceController::nativeEventFilter(const QByteArray &, void *message, qintptr *)
{
#ifdef Q_OS_WIN
    auto *msg = static_cast<MSG *>(message);
    if (msg->message == WM_SETTINGCHANGE || msg->message == WM_THEMECHANGED || msg->message == WM_DWMCOMPOSITIONCHANGED)
        QTimer::singleShot(0, this, [this] { refresh(); });
#else
    Q_UNUSED(message)
#endif
    return false;
}

void AppearanceController::refresh()
{
    bool systemDark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    bool transparency = true;
    m_highContrast = false;
#ifdef Q_OS_WIN
    QSettings system("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
    systemDark = system.value("AppsUseLightTheme", !systemDark).toInt() == 0;
    transparency = system.value("EnableTransparency", 1).toInt() != 0;
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0))
        m_highContrast = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
#endif
    m_dark = m_mode == "system" ? systemDark : m_mode == "dark";
    if (m_highContrast) {
#ifdef Q_OS_WIN
        const COLORREF color = GetSysColor(COLOR_WINDOW);
        m_dark = (GetRValue(color) + GetGValue(color) + GetBValue(color)) < 384;
#endif
    }
    m_backdrop = false;
#ifdef Q_OS_WIN
    if (m_window && QGuiApplication::platformName() == "windows") {
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        const BOOL dark = m_dark;
        // Official DWM attributes. Unsupported Windows versions return failure;
        // keep native decoration and opaque QML background in that case.
        DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
        const DWORD rounded = 2 /* DWMWCP_ROUND */;
        DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &rounded, sizeof(rounded));
        const bool wantMaterial = m_glass && transparency && !m_highContrast;
        const DWORD material = wantMaterial ? 2 /* DWMSBT_MAINWINDOW */ : 1 /* DWMSBT_NONE */;
        const HRESULT result = DwmSetWindowAttribute(hwnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */, &material, sizeof(material));
        if (SUCCEEDED(result)) {
            MARGINS margins = wantMaterial ? MARGINS{-1, -1, -1, -1} : MARGINS{0, 0, 0, 0};
            m_backdrop = wantMaterial && SUCCEEDED(DwmExtendFrameIntoClientArea(hwnd, &margins));
        }
    }
#else
    Q_UNUSED(transparency)
#endif
    emit changed();
}
