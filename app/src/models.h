#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QtGlobal>

struct UserResourcePolicy {
    int userId = 0;
    int portStart = 0;
    int portEnd = 0;
    int maxPorts = 1;
    QStringList allowedProtocols;
    bool enabled = false;

    static UserResourcePolicy fromJson(const QJsonObject &object)
    {
        UserResourcePolicy policy;
        policy.userId = object.value("user_id").toInt();
        policy.portStart = object.value("port_start").toInt();
        policy.portEnd = object.value("port_end").toInt();
        policy.maxPorts = object.value("max_ports").toInt(1);
        policy.enabled = object.value("enabled").toBool(false);
        const auto protocols = object.value("allowed_protocols").toArray();
        for (const auto &value : protocols) {
            policy.allowedProtocols << value.toString();
        }
        return policy;
    }
};

struct ClientDpiStatus {
    bool enabled = false;
    QString mode = "monitor";
    QStringList enabledDetectors;
    QStringList blockedTrafficTypes;
    QStringList allowedTrafficTypes;
    bool blockOnAnyFinding = false;

    static ClientDpiStatus fromJson(const QJsonObject &object)
    {
        ClientDpiStatus status;
        status.enabled = object.value("enabled").toBool(false);
        status.mode = object.value("mode").toString("monitor");
        status.blockOnAnyFinding = object.value("block_on_any_finding").toBool(false);
        auto readList = [](const QJsonArray &array) {
            QStringList values;
            for (const auto &value : array) {
                values << value.toString();
            }
            return values;
        };
        status.enabledDetectors = readList(object.value("enabled_detectors").toArray());
        status.blockedTrafficTypes = readList(object.value("blocked_traffic_types").toArray());
        status.allowedTrafficTypes = readList(object.value("allowed_traffic_types").toArray());
        return status;
    }
};

struct ResourcePolicyResponse {
    bool ok = false;
    QString status;
    QString reason;
    QString user;
    QString tokenName;
    UserResourcePolicy policy;
    ClientDpiStatus dpi;
    QString frpServerAddr;
    int frpServerPort = 0;
    bool frpTransportTls = false;

    static ResourcePolicyResponse fromJson(const QJsonObject &object)
    {
        ResourcePolicyResponse response;
        response.ok = object.value("ok").toBool(false);
        response.status = object.value("status").toString();
        response.reason = object.value("reason").toString(object.value("error").toString());
        response.user = object.value("user").toString();
        response.tokenName = object.value("token").toString();
        response.policy = UserResourcePolicy::fromJson(object.value("policy").toObject());
        response.dpi = ClientDpiStatus::fromJson(object.value("dpi").toObject());
        response.frpServerAddr = object.value("frp_server_addr").toString();
        response.frpServerPort = object.value("frp_server_port").toInt();
        response.frpTransportTls = object.value("frp_transport_tls").toBool(false);
        return response;
    }
};

struct TunnelDraft {
    QString name = "ssh";
    QString type = "tcp";
    QString localIp = "127.0.0.1";
    int localPort = 22;
    int remotePort = 6001;

    QJsonObject toJson() const
    {
        return QJsonObject{
            {"name", name},
            {"type", type},
            {"local_ip", localIp},
            {"local_port", localPort},
            {"remote_port", remotePort},
        };
    }
};

struct BootstrapResponse {
    bool ok = false;
    QString status;
    QString reason;
    QString leaseId;
    QString expiresAt;
    int expiresIn = 0;
    QString frpcConfig;

    static BootstrapResponse fromJson(const QJsonObject &object)
    {
        BootstrapResponse response;
        response.ok = object.value("ok").toBool(false);
        response.status = object.value("status").toString();
        response.reason = object.value("reason").toString(object.value("error").toString());
        response.leaseId = object.value("lease_id").toString();
        response.expiresAt = object.value("expires_at").toString();
        response.expiresIn = object.value("expires_in").toInt();
        response.frpcConfig = object.value("frpc_config").toString();
        return response;
    }
};

struct ClientCommand {
    qint64 id = 0;
    QString command;
    QString message;

    static ClientCommand fromJson(const QJsonObject &object)
    {
        ClientCommand command;
        command.id = static_cast<qint64>(object.value("id").toDouble());
        command.command = object.value("command").toString();
        command.message = object.value("message").toString();
        return command;
    }
};

struct HeartbeatResponse {
    bool ok = false;
    QString status;
    QString reason;
    QList<ClientCommand> commands;

    static HeartbeatResponse fromJson(const QJsonObject &object)
    {
        HeartbeatResponse response;
        response.ok = object.value("ok").toBool(false);
        response.status = object.value("status").toString();
        response.reason = object.value("reason").toString(object.value("error").toString());
        const auto commands = object.value("commands").toArray();
        for (const auto &value : commands) {
            response.commands << ClientCommand::fromJson(value.toObject());
        }
        return response;
    }
};

Q_DECLARE_METATYPE(ResourcePolicyResponse)
Q_DECLARE_METATYPE(BootstrapResponse)
Q_DECLARE_METATYPE(HeartbeatResponse)
