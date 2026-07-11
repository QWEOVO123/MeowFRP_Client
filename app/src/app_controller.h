#pragma once

#include "control_api_client.h"
#include "profile_service.h"
#include "tunnel_runtime_service.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString apiBaseUrl READ apiBaseUrl WRITE setApiBaseUrl NOTIFY profileChanged)
    Q_PROPERTY(QString accessToken READ accessToken WRITE setAccessToken NOTIFY profileChanged)
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY profileChanged)
    Q_PROPERTY(QString frpcPath READ frpcPath WRITE setFrpcPath NOTIFY profileChanged)
    Q_PROPERTY(QString runtimeDir READ runtimeDir WRITE setRuntimeDir NOTIFY profileChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool frpcRunning READ frpcRunning NOTIFY stateChanged)
    Q_PROPERTY(bool quitInProgress READ quitInProgress NOTIFY stateChanged)
    Q_PROPERTY(bool allowClose READ allowClose NOTIFY stateChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(QString logText READ logText NOTIFY logsChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY policyChanged)
    Q_PROPERTY(QString tokenName READ tokenName NOTIFY policyChanged)
    Q_PROPERTY(QString frpEndpoint READ frpEndpoint NOTIFY policyChanged)
    Q_PROPERTY(int portStart READ portStart NOTIFY policyChanged)
    Q_PROPERTY(int portEnd READ portEnd NOTIFY policyChanged)
    Q_PROPERTY(int maxPorts READ maxPorts NOTIFY policyChanged)
    Q_PROPERTY(QVariantList allowedProtocols READ allowedProtocols NOTIFY policyChanged)
    Q_PROPERTY(QString allowedProtocolsText READ allowedProtocolsText NOTIFY policyChanged)
    Q_PROPERTY(bool dpiEnabled READ dpiEnabled NOTIFY policyChanged)
    Q_PROPERTY(QString dpiMode READ dpiMode NOTIFY policyChanged)
    Q_PROPERTY(QString dpiBlockedText READ dpiBlockedText NOTIFY policyChanged)
    Q_PROPERTY(QString dpiDetectorsText READ dpiDetectorsText NOTIFY policyChanged)
    Q_PROPERTY(QVariantList tunnelList READ tunnelList NOTIFY tunnelsChanged)
    Q_PROPERTY(int tunnelCount READ tunnelCount NOTIFY tunnelsChanged)
    Q_PROPERTY(int tunnelLimit READ tunnelLimit NOTIFY policyChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    QString apiBaseUrl() const;
    void setApiBaseUrl(const QString &value);
    QString accessToken() const;
    void setAccessToken(const QString &value);
    QString clientId() const;
    void setClientId(const QString &value);
    QString frpcPath() const;
    void setFrpcPath(const QString &value);
    QString runtimeDir() const;
    void setRuntimeDir(const QString &value);

    bool connected() const;
    bool busy() const;
    bool frpcRunning() const;
    bool quitInProgress() const;
    bool allowClose() const;
    QString statusMessage() const;
    QString errorMessage() const;
    QString logText() const;
    QString userName() const;
    QString tokenName() const;
    QString frpEndpoint() const;
    int portStart() const;
    int portEnd() const;
    int maxPorts() const;
    QVariantList allowedProtocols() const;
    QString allowedProtocolsText() const;
    bool dpiEnabled() const;
    QString dpiMode() const;
    QString dpiBlockedText() const;
    QString dpiDetectorsText() const;
    QVariantList tunnelList() const;
    int tunnelCount() const;
    int tunnelLimit() const;

    Q_INVOKABLE void saveProfile();
    Q_INVOKABLE void connectToServer();
    Q_INVOKABLE void createTunnel(const QString &name, const QString &type, const QString &localIp, int localPort, int remotePort);
    Q_INVOKABLE void addTunnel(const QString &name, const QString &type, const QString &localIp, int localPort, int remotePort);
    Q_INVOKABLE void removeTunnel(int index);
    Q_INVOKABLE void startTunnels();
    Q_INVOKABLE void stopTunnel(int index);
    Q_INVOKABLE void stopAllTunnels();
    Q_INVOKABLE void copyRemoteEndpoint(int index);
    Q_INVOKABLE void stopFrpc();
    Q_INVOKABLE void quitClient();
    Q_INVOKABLE void clearLogs();

signals:
    void profileChanged();
    void stateChanged();
    void policyChanged();
    void logsChanged();
    void tunnelsChanged();
    void remoteMessageRequested(const QString &message);

private:
    enum class LogoutAction {
        None,
        ReturnToAuth,
        QuitApplication,
    };

    void setBusy(bool value, const QString &message = {});
    void setError(const QString &message);
    void setStatus(const QString &message);
    void appendLog(const QString &line);
    bool validateTunnel(const TunnelDraft &draft, QString *errorMessage) const;
    bool validateTunnelList(const QList<TunnelDraft> &tunnels, QString *errorMessage) const;
    QString remoteEndpoint(const TunnelDraft &tunnel) const;
    void requestBootstrapForTunnels(const QList<TunnelDraft> &tunnels, const QString &statusMessage);
    void applyPolicy(const ResourcePolicyResponse &response);
    void startHeartbeat();
    void stopHeartbeat();
    void sendHeartbeat();
    void handleHeartbeatResponse(const HeartbeatResponse &response);
    void executeClientCommand(const ClientCommand &command);
    void returnToAuthScreen(const QString &message);
    void requestLogout(LogoutAction action, const QString &message);
    void finishLogout();

    ProfileService m_profiles;
    ControlApiClient m_api;
    TunnelRuntimeService m_runtime;
    QTimer m_heartbeatTimer;
    ClientProfile m_profile;
    ResourcePolicyResponse m_policy;
    bool m_connected = false;
    bool m_busy = false;
    bool m_frpcRunning = false;
    bool m_quitInProgress = false;
    bool m_allowClose = false;
    LogoutAction m_logoutAction = LogoutAction::None;
    QString m_logoutMessage;
    QString m_statusMessage = "等待连接服务器";
    QString m_errorMessage;
    QString m_logText;
    int m_logLineCount = 0;
    QList<TunnelDraft> m_tunnels;
};
