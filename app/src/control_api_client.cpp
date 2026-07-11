#include "control_api_client.h"

#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtGlobal>

ControlApiClient::ControlApiClient(QObject *parent)
    : QObject(parent)
{
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
    connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess, fail]() {
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
                message = object.value("error").toString(object.value("reason").toString(message));
            }
            fail(QString("HTTP %1：%2").arg(statusCode).arg(message));
            return;
        }
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            fail("服务端返回了无效 JSON。");
            return;
        }
        onSuccess(document.object());
    });
}
