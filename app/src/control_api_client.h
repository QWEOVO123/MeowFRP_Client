#pragma once

#include "models.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QJsonObject>
#include <QList>
#include <QUrl>

#include <functional>

class ControlApiClient : public QObject {
    Q_OBJECT

public:
    explicit ControlApiClient(QObject *parent = nullptr);
    void invalidateSession();
    void setDebugMode(bool enabled) { m_debugMode = enabled; }

    void queryNodeDirectory(const QString &controllerApiBaseUrl, const QString &accessToken, const QString &clientId);
    void queryResourcePolicy(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId);
    void bootstrap(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, const QList<TunnelDraft> &proxies);
    void heartbeat(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, bool frpcRunning);
    void releaseLease(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, const QString &leaseId);
    void acknowledgeCommand(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, qint64 commandId);
    void logout(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, bool frpcRunning);

signals:
    void diagnostic(const QString &line);
    void accountAuthenticated(const QString &accessToken);
    void nodeDirectoryLoaded(const QList<NodeDirectoryEntry> &nodes);
    void resourcePolicyLoaded(const ResourcePolicyResponse &response);
    void bootstrapLoaded(const BootstrapResponse &response);
    void heartbeatLoaded(const HeartbeatResponse &response);
    void heartbeatFailed(const QString &message);
    void commandAcknowledged(qint64 commandId);
    void commandAcknowledgeFailed(qint64 commandId, const QString &message);
    void logoutFinished();
    void logoutFailed(const QString &message);
    void requestFailed(const QString &message);
    void leaseReleaseFailed(const QString &message);

private:
    QUrl endpointUrl(const QString &apiBaseUrl, const QString &path) const;
    void postJson(const QUrl &url, const QJsonObject &payload, std::function<void(const QJsonObject &)> onSuccess, std::function<void(const QString &)> onFailure = {}, bool sessionBound = true, std::function<void(const QJsonObject &)> onDiscardSuccess = {});
    void getJson(const QUrl &url, std::function<void(const QJsonObject &)> onSuccess);
    void handleJsonReply(QNetworkReply *reply, std::function<void(const QJsonObject &)> onSuccess, std::function<void(const QString &)> onFailure);

    QNetworkAccessManager m_network;
    quint64 m_sessionGeneration = 0;
    bool m_heartbeatInFlight = false;
    bool m_debugMode = false;
    quint64 m_requestSequence = 0;
    void debugLog(const QString &line);
};
