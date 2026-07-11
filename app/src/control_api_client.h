#pragma once

#include "models.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QJsonObject>
#include <QList>
#include <QUrl>

#include <functional>

class ControlApiClient : public QObject {
    Q_OBJECT

public:
    explicit ControlApiClient(QObject *parent = nullptr);

    void queryResourcePolicy(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId);
    void bootstrap(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, const QList<TunnelDraft> &proxies);
    void heartbeat(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, bool frpcRunning);
    void logout(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, bool frpcRunning);

signals:
    void resourcePolicyLoaded(const ResourcePolicyResponse &response);
    void bootstrapLoaded(const BootstrapResponse &response);
    void heartbeatLoaded(const HeartbeatResponse &response);
    void heartbeatFailed(const QString &message);
    void logoutFinished();
    void logoutFailed(const QString &message);
    void requestFailed(const QString &message);

private:
    QUrl endpointUrl(const QString &apiBaseUrl, const QString &path) const;
    void postJson(const QUrl &url, const QJsonObject &payload, std::function<void(const QJsonObject &)> onSuccess, std::function<void(const QString &)> onFailure = {});

    QNetworkAccessManager m_network;
};
