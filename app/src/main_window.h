#pragma once

#include "control_api_client.h"
#include "profile_service.h"
#include "tunnel_runtime_service.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void saveProfile();
    void queryPolicy();
    void requestBootstrap();
    void scheduleAutoConnect();
    void clearLoadedPolicy();
    void handlePolicy(const ResourcePolicyResponse &response);
    void handleBootstrap(const BootstrapResponse &response);
    void appendLog(const QString &line);
    void showError(const QString &message);

private:
    void buildUi();
    QWidget *buildConnectPage(QWidget *parent);
    QWidget *buildTunnelPage(QWidget *parent);
    void loadProfileToUi();
    ClientProfile profileFromUi() const;
    TunnelDraft tunnelFromUi() const;
    bool validateTunnel(const TunnelDraft &draft, QString *errorMessage) const;
    void updatePolicySummary();

    ProfileService m_profiles;
    ControlApiClient m_api;
    TunnelRuntimeService m_runtime;
    ClientProfile m_profile;
    ResourcePolicyResponse m_policy;
    bool m_hasPolicy = false;

    QLineEdit *m_apiUrlEdit = nullptr;
    QLineEdit *m_tokenEdit = nullptr;
    QLineEdit *m_clientIdEdit = nullptr;
    QLineEdit *m_frpcPathEdit = nullptr;
    QLineEdit *m_runtimeDirEdit = nullptr;
    QLabel *m_connectionHint = nullptr;
    QLabel *m_policyLabel = nullptr;
    QLabel *m_dpiLabel = nullptr;
    QLineEdit *m_proxyNameEdit = nullptr;
    QComboBox *m_protocolCombo = nullptr;
    QLineEdit *m_localIpEdit = nullptr;
    QSpinBox *m_localPortSpin = nullptr;
    QSpinBox *m_remotePortSpin = nullptr;
    QPushButton *m_queryButton = nullptr;
    QPushButton *m_bootstrapButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_backButton = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;
    QStackedWidget *m_stack = nullptr;
    QTimer m_autoConnectTimer;
};
