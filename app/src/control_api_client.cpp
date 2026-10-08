#include "control_api_client.h"
#include "log_safety.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QElapsedTimer>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QSslError>
#include <QtGlobal>

namespace {
QJsonValue diagnosticSummary(const QJsonValue &value, int depth = 0)
{
    if (depth > 6) return "[depth limited]";
    if (value.isArray()) {
        const auto array = value.toArray();
        QJsonArray summary;
        for (qsizetype i = 0; i < qMin(array.size(), qsizetype(32)); ++i)
            summary.append(diagnosticSummary(array.at(i), depth + 1));
        if (array.size() > 32) summary.append(QString("[%1 more items]").arg(array.size() - 32));
        return summary;
    }
    if (!value.isObject()) return value;
    // Allowlist: do not dump future unknown fields, certificates, tokens or TOML.
    static const QStringList allowed{
        "ok", "status", "reason", "error", "client_version", "frpc_running",
        "nodes", "node_id", "tag", "node_type", "online", "api_url",
        "commands", "id", "command", "message", "heartbeat_interval", "heartbeat_timeout",
        "expires_at", "expires_in", "allocations", "proxies", "name", "type",
        "local_ip", "local_port", "remote_port", "policy", "port_start", "port_end",
        "max_ports", "allowed_protocols", "enabled", "dpi", "mode", "frp_server_addr", "frp_server_port"
    };
    QJsonObject summary;
    const auto object = value.toObject();
    for (const auto &key : allowed) {
        if (object.contains(key)) summary.insert(key, diagnosticSummary(object.value(key), depth + 1));
    }
    if (object.contains("access_token")) summary.insert("access_token", "[REDACTED]");
    if (object.contains("client_id")) summary.insert("client_id", "[device ID omitted]");
    if (object.contains("frpc_config")) summary.insert("frpc_config_bytes", object.value("frpc_config").toString().toUtf8().size());
    if (object.contains("lease_id")) summary.insert("lease_id", "[lease present]");
    return summary;
}

QString summaryText(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(diagnosticSummary(object).toObject()).toJson(QJsonDocument::Compact));
}

bool isLoopbackHost(const QString &host)
{
    if (host.compare("localhost", Qt::CaseInsensitive) == 0) {
        return true;
    }
    QHostAddress address;
    return address.setAddress(host) && address.isLoopback();
}

QString endpointValidationError(const QUrl &url)
{
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        return "API 地址无效，请填写完整地址，例如 https://node.example.com/api。";
    }
    if (!url.userInfo().isEmpty() || url.hasQuery() || !url.fragment().isEmpty()) {
        return "API 基础地址不能包含用户名、密码、查询参数或片段。";
    }
    if (url.scheme().compare("https", Qt::CaseInsensitive) != 0
        && !(url.scheme().compare("http", Qt::CaseInsensitive) == 0 && isLoopbackHost(url.host()))) {
        return "远程节点 API 必须使用 HTTPS；仅本机地址允许使用 HTTP。";
    }
    return {};
}
}

ControlApiClient::ControlApiClient(QObject *parent)
    : QObject(parent)
{
}

void ControlApiClient::debugLog(const QString &line)
{
    if (m_debugMode) emit diagnostic(line);
}

void ControlApiClient::releaseLease(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, const QString &leaseId)
{
    postJson(endpointUrl(apiBaseUrl, "/v1/client/release-lease"), {{"access_token", accessToken}, {"client_id", clientId}, {"lease_id", leaseId}},
        [](const QJsonObject &) {}, [this](const QString &message) { emit leaseReleaseFailed(message); }, false);
}

void ControlApiClient::invalidateSession()
{
    ++m_sessionGeneration;
    debugLog(QString("[SESSION] invalidated generation=%1 heartbeat_in_flight=%2")
        .arg(m_sessionGeneration).arg(m_heartbeatInFlight));
    m_heartbeatInFlight = false;
}

void ControlApiClient::queryNodeDirectory(const QString &controllerApiBaseUrl, const QString &accessToken, const QString &clientId)
{
    invalidateSession();
    postJson(endpointUrl(controllerApiBaseUrl, "/v1/client/login"), {{"access_token", accessToken.trimmed()}, {"client_id", clientId}}, [this, accessToken](const QJsonObject &object) {
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
        emit accountAuthenticated(accessToken.trimmed());
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
    }, {}, true, [this, apiBaseUrl, accessToken, clientId](const QJsonObject &object) {
        // A cancelled start may already have allocated a server-side lease.
        const auto response = BootstrapResponse::fromJson(object);
        if (response.ok && !response.leaseId.isEmpty()) releaseLease(apiBaseUrl, accessToken, clientId, response.leaseId);
    });
}

void ControlApiClient::heartbeat(const QString &apiBaseUrl, const QString &accessToken, const QString &clientId, bool frpcRunning)
{
    if (m_heartbeatInFlight) {
        debugLog("[HEARTBEAT] skipped: previous request still in flight");
        return;
    }
    m_heartbeatInFlight = true;
    QJsonObject payload{
        {"access_token", accessToken.trimmed()},
        {"client_id", clientId.trimmed()},
        {"client_version", QString(APP_VERSION)},
        {"frpc_running", frpcRunning},
    };
    postJson(endpointUrl(apiBaseUrl, "/v1/client/heartbeat"), payload, [this](const QJsonObject &object) {
        m_heartbeatInFlight = false;
        const HeartbeatResponse response = HeartbeatResponse::fromJson(object);
        emit heartbeatLoaded(response);
    }, [this](const QString &message) {
        m_heartbeatInFlight = false;
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
    const QString validationError = endpointValidationError(url);
    if (!validationError.isEmpty()) {
        emit commandAcknowledgeFailed(commandId, validationError);
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
        const QUrl guessed(QStringLiteral("https://") + base);
        base.prepend(isLoopbackHost(guessed.host()) ? "http://" : "https://");
    }
    while (base.endsWith('/')) {
        base.chop(1);
    }
    if (QUrl(base).path().isEmpty()) base += "/api";
    if (base.endsWith("/api/v1")) base.chop(3);
    return QUrl(base + path);
}

void ControlApiClient::postJson(const QUrl &url, const QJsonObject &payload, std::function<void(const QJsonObject &)> onSuccess, std::function<void(const QString &)> onFailure, bool sessionBound, std::function<void(const QJsonObject &)> onDiscardSuccess)
{
    const auto generation = m_sessionGeneration;
    const auto requestId = ++m_requestSequence;
    LogSafety::rememberSecret(payload.value("access_token").toString());
    debugLog(QString("[HTTP #%1] POST %2 generation=%3 session_bound=%4 timeout_ms=15000 request=%5")
        .arg(requestId).arg(LogSafety::url(url)).arg(generation).arg(sessionBound).arg(summaryText(payload)));
    auto fail = [this, onFailure, generation, sessionBound, requestId](const QString &message) {
        if (sessionBound && generation != m_sessionGeneration) {
            debugLog(QString("[HTTP #%1] stale failure ignored generation=%2 current=%3")
                .arg(requestId).arg(generation).arg(m_sessionGeneration));
            return;
        }
        debugLog(QString("[HTTP #%1] failure=%2").arg(requestId).arg(message));
        if (onFailure) {
            onFailure(message);
        } else {
            emit requestFailed(message);
        }
    };
    const QString validationError = endpointValidationError(url);
    if (!validationError.isEmpty()) {
        fail(validationError);
        return;
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(15000);
#endif

    auto *reply = m_network.post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    reply->setProperty("diagnosticRequestId", QVariant::fromValue(requestId));
    handleJsonReply(reply, [this, generation, sessionBound, onSuccess, onDiscardSuccess, requestId](const QJsonObject &object) {
        if (sessionBound && generation != m_sessionGeneration) {
            debugLog(QString("[HTTP #%1] stale success discarded generation=%2 current=%3 release_cancelled_lease=%4")
                .arg(requestId).arg(generation).arg(m_sessionGeneration).arg(bool(onDiscardSuccess)));
            if (onDiscardSuccess) onDiscardSuccess(object);
            return;
        }
        onSuccess(object);
    }, fail);
}

void ControlApiClient::getJson(const QUrl &url, std::function<void(const QJsonObject &)> onSuccess)
{
    const auto requestId = ++m_requestSequence;
    debugLog(QString("[HTTP #%1] GET %2 timeout_ms=15000").arg(requestId).arg(LogSafety::url(url)));
    const QString validationError = endpointValidationError(url);
    if (!validationError.isEmpty()) {
        emit requestFailed(validationError);
        return;
    }
    QNetworkRequest request(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(15000);
#endif
    auto *reply = m_network.get(request);
    reply->setProperty("diagnosticRequestId", QVariant::fromValue(requestId));
    handleJsonReply(reply, onSuccess, [this](const QString &message) { emit requestFailed(message); });
}

void ControlApiClient::handleJsonReply(QNetworkReply *reply, std::function<void(const QJsonObject &)> onSuccess, std::function<void(const QString &)> onFailure)
{
    QElapsedTimer timer;
    timer.start();
    const auto requestId = reply->property("diagnosticRequestId").toULongLong();
    connect(reply, &QNetworkReply::socketStartedConnecting, this, [this, requestId]() {
        debugLog(QString("[HTTP #%1] socket started connecting").arg(requestId));
    });
    connect(reply, &QNetworkReply::requestSent, this, [this, requestId]() {
        debugLog(QString("[HTTP #%1] request sent").arg(requestId));
    });
    connect(reply, &QNetworkReply::encrypted, this, [this, reply, requestId]() {
        const auto ssl = reply->sslConfiguration();
        debugLog(QString("[HTTP #%1] TLS encrypted protocol=%2 cipher=%3")
            .arg(requestId).arg(static_cast<int>(ssl.sessionProtocol())).arg(ssl.sessionCipher().name()));
    });
    connect(reply, &QNetworkReply::sslErrors, this, [this, requestId](const QList<QSslError> &errors) {
        for (const auto &error : errors)
            debugLog(QString("[HTTP #%1] TLS error code=%2 text=%3; certificate verification NOT bypassed")
                .arg(requestId).arg(static_cast<int>(error.error())).arg(error.errorString()));
    });
    connect(reply, &QNetworkReply::redirected, this, [this, requestId](const QUrl &target) {
        debugLog(QString("[HTTP #%1] redirect target=%2").arg(requestId).arg(LogSafety::url(target)));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess, onFailure, timer, requestId]() {
        const QByteArray body = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = reply->error();
        const QString networkErrorString = reply->errorString();
        debugLog(QString("[HTTP #%1] finished status=%2 network_error=%3 elapsed_ms=%4 response_bytes=%5 url=%6")
            .arg(requestId).arg(statusCode).arg(static_cast<int>(networkError)).arg(timer.elapsed()).arg(body.size())
            .arg(LogSafety::url(reply->url())));
        reply->deleteLater();

        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        if (document.isObject()) {
            debugLog(QString("[HTTP #%1] response=%2").arg(requestId).arg(summaryText(document.object())));
        } else {
            debugLog(QString("[HTTP #%1] invalid JSON parse_error=%2 offset=%3 body omitted")
                .arg(requestId).arg(static_cast<int>(parseError.error)).arg(parseError.offset));
        }
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
