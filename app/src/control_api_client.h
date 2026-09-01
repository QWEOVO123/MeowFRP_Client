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

    void queryNodeDirectory(const QString &controllerApiBaseUrl);
    void queryResourcePolicy(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId);
    void bootstrap(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, const QList<TunnelDraft> &proxies);
    void heartbeat(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, bool frpcRunning);
    void acknowledgeCommand(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, qint64 commandId);
    void logout(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, bool frpcRunning);

signals:
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

private:
    QUrl endpointUrl(const QString &apiBaseUrl, const QString &path) const;
    void postJson(const QUrl &url, const QJsonObject &payload, std::function<void(const QJsonObject &)> onSuccess, std::function<void(const QString &)> onFailure = {});
    void getJson(const QUrl &url, std::function<void(const QJsonObject &)> onSuccess);
    void handleJsonReply(QNetworkReply *reply, std::function<void(const QJsonObject &)> onSuccess, std::function<void(const QString &)> onFailure);

    QNetworkAccessManager m_network;
};
