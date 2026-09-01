#include "control_api_client.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtGlobal>

ControlApiClient::ControlApiClient(QObject *parent)
    : QObject(parent)
{
}

void ControlApiClient::queryNodeDirectory(const QString &controllerApiBaseUrl)
{
    getJson(endpointUrl(controllerApiBaseUrl, "/v1/public/nodes"), [this](const QJsonObject &object) {
        if (!object.value("ok").toBool(false)) {
            emit requestFailed(object.value("error").toString("中心节点拒绝了目录请求。"));
            return;
        }
        QList<NodeDirectoryEntry> nodes;
        for (const auto &value : object.value("nodes").toArray()) {
            const auto node = NodeDirectoryEntry::fromJson(value.toObject());
            if (!node.nodeId.isEmpty() && !node.apiUrl.isEmpty()) nodes << node;
        }
        if (nodes.isEmpty()) {
            emit requestFailed("中心节点当前没有可用的 FRP 节点。");
            return;
        }
        emit nodeDirectoryLoaded(nodes);
    });
}

void ControlApiClient::queryResourcePolicy(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId)
{
    QJsonObject payload{
        {"access_token", accessToken.trimmed()},
        {"client_id", clientId.trimmed()},
    };
    postJson(endpointUrl(apiBaseUrl, "/v1/client/resource-policy"), payload, [this](const QJsonObject &object) {
        const ResourcePolicyResponse response = ResourcePolicyResponse::fromJson(object);
        if (!response.ok) {
            emit requestFailed(response.reason.isEmpty() ? "服务端拒绝了权限查询。" : response.reason);
            return;
        }
        emit resourcePolicyLoaded(response);
    });
}

void ControlApiClient::bootstrap(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, const QList<TunnelDraft> &proxies)
{
    QJsonArray proxyArray;
    for (const auto &proxy : proxies) {
        proxyArray.append(proxy.toJson());
    }
    QJsonObject payload{
        {"access_token", accessToken.trimmed()},
        {"client_id", clientId.trimmed()},
        {"client_version", QString(APP_VERSION)},
        {"proxies", proxyArray},
    };
    postJson(endpointUrl(apiBaseUrl, "/v1/client/bootstrap"), payload, [this](const QJsonObject &object) {
        const BootstrapResponse response = BootstrapResponse::fromJson(object);
        if (!response.ok) {
            emit requestFailed(response.reason.isEmpty() ? "服务端拒绝了配置下发。" : response.reason);
            return;
        }
        emit bootstrapLoaded(response);
    });
}

void ControlApiClient::heartbeat(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, bool frpcRunning)
{
    QJsonObject payload{
        {"access_token", accessToken.trimmed()},
        {"client_id", clientId.trimmed()},
        {"client_version", QString(APP_VERSION)},
        {"frpc_running", frpcRunning},
    };
    postJson(endpointUrl(apiBaseUrl, "/v1/client/heartbeat"), payload, [this](const QJsonObject &object) {
        const HeartbeatResponse response = HeartbeatResponse::fromJson(object);
        emit heartbeatLoaded(response);
    }, [this](const QString &message) {
        emit heartbeatFailed(message);
    });
}

void ControlApiClient::acknowledgeCommand(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, qint64 commandId)
{
    if (commandId <= 0) {
        return;
    }
    const QString path = QString("/v1/client/commands/%1/ack").arg(commandId);
    const QUrl url = endpointUrl(apiBaseUrl, path);
    const QString host = url.host().toLower();
    const bool localDevelopment = host == "127.0.0.1" || host == "localhost" || host == "::1";
    if (url.scheme().compare("https", Qt::CaseInsensitive) != 0 && !localDevelopment) {
        emit commandAcknowledgeFailed(commandId, "远程命令 ACK 必须通过 HTTPS 节点 API 发送。");
        return;
    }
    QJsonObject payload{
        {"access_token", accessToken.trimmed()},
        {"client_id", clientId.trimmed()},
    };
    postJson(url, payload, [this, commandId](const QJsonObject &object) {
        if (!object.value("ok").toBool(false)) {
            emit commandAcknowledgeFailed(commandId, object.value("reason").toString(object.value("error").toString("服务端拒绝了命令确认。")));
            return;
        }
        emit commandAcknowledged(commandId);
    }, [this, commandId](const QString &message) {
        emit commandAcknowledgeFailed(commandId, message);
    });
}

void ControlApiClient::logout(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, bool frpcRunning)
{
    QJsonObject payload{
        {"access_token", accessToken.trimmed()},
        {"client_id", clientId.trimmed()},
        {"client_version", QString(APP_VERSION)},
        {"frpc_running", frpcRunning},
    };
    postJson(endpointUrl(apiBaseUrl, "/v1/client/logout"), payload, [this](const QJsonObject &object) {
        const bool ok = object.value("ok").toBool(false);
        if (!ok) {
            emit logoutFailed(object.value("reason").toString(object.value("error").toString("服务端拒绝了下线请求。")));
            return;
        }
        emit logoutFinished();
    }, [this](const QString &message) {
        emit logoutFailed(message);
    });
}

QUrl ControlApiClient::endpointUrl(const QString &apiBaseUrl, const QString &path) const
{
    QString base = apiBaseUrl.trimmed();
    if (!base.contains("://")) {
        base.prepend("http://");
    }
    while (base.endsWith('/')) {
        base.chop(1);
    }
    return QUrl(base + path);
}

void ControlApiClient::postJson(const QUrl &url, const QJsonObject &payload, std::function<void(const QJsonObject &)> onSuccess, std::function<void(const QString &)> onFailure)
{
    auto fail = [this, onFailure](const QString &message) {
        if (onFailure) {
            onFailure(message);
        } else {
            emit requestFailed(message);
        }
    };
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        fail("API 地址无效，请填写完整的 API 基础地址，例如 http://127.0.0.1:8080/api。");
        return;
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(15000);
#endif

    auto *reply = m_network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    handleJsonReply(reply, onSuccess, fail);
}

void ControlApiClient::getJson(const QUrl &url, std::function<void(const QJsonObject &)> onSuccess)
{
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        emit requestFailed("中心 API 地址无效，请填写完整地址，例如 https://controller.example.com/api。");
        return;
    }
    QNetworkRequest request(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(15000);
#endif
    handleJsonReply(m_network.get(request), onSuccess, [this](const QString &message) { emit requestFailed(message); });
}

void ControlApiClient::handleJsonReply(QNetworkReply *reply, std::function<void(const QJsonObject &)> onSuccess, std::function<void(const QString &)> onFailure)
{
    connect(reply, &QNetworkReply::finished, this, [reply, onSuccess, onFailure]() {
        const QByteArray body = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = reply->error();
        const QString networkErrorString = reply->errorString();
        reply->deleteLater();

        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        if (networkError != QNetworkReply::NoError) {
            QString message = networkErrorString;
            if (document.isObject()) {
                const auto object = document.object();
                message = object.value("reason").toString(object.value("error").toString(message));
            }
            onFailure(QString("HTTP %1：%2").arg(statusCode).arg(message));
            return;
        }
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            onFailure("服务端返回了无效 JSON。");
            return;
        }
        onSuccess(document.object());
    });
}
