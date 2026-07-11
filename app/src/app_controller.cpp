#include "app_controller.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

#include <utility>

namespace {
constexpr int kMaxLogLines = 10000;
constexpr qsizetype kMaxLogCharacters = 2 * 1024 * 1024;
}

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    m_profile = m_profiles.load();
    m_heartbeatTimer.setInterval(10000);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &AppController::sendHeartbeat);
    connect(&m_api, &ControlApiClient::resourcePolicyLoaded, this, [this](const ResourcePolicyResponse &response) {
        setBusy(false);
        applyPolicy(response);
    });
    connect(&m_api, &ControlApiClient::bootstrapLoaded, this, [this](const BootstrapResponse &response) {
        setBusy(false, "服务端已下发配置");
        appendLog("服务端已下发配置，租约：" + response.leaseId);
        if (!response.expiresAt.isEmpty()) {
            appendLog("租约过期时间：" + response.expiresAt);
        }
        m_runtime.start(m_profile, response);
        m_frpcRunning = m_runtime.isRunning();
        emit stateChanged();
        emit tunnelsChanged();
    });
    connect(&m_api, &ControlApiClient::requestFailed, this, [this](const QString &message) {
        setBusy(false);
        setError(message);
    });
    connect(&m_api, &ControlApiClient::heartbeatLoaded, this, &AppController::handleHeartbeatResponse);
    connect(&m_api, &ControlApiClient::heartbeatFailed, this, [this](const QString &message) {
        appendLog("心跳失败：" + message);
    });
    connect(&m_api, &ControlApiClient::logoutFinished, this, [this]() {
        appendLog("服务端已确认客户端下线。");
        finishLogout();
    });
    connect(&m_api, &ControlApiClient::logoutFailed, this, [this](const QString &message) {
        appendLog("下线请求失败：" + message);
        finishLogout();
    });
    connect(&m_runtime, &TunnelRuntimeService::logLine, this, &AppController::appendLog);
    connect(&m_runtime, &TunnelRuntimeService::failed, this, [this](const QString &message) {
        m_frpcRunning = m_runtime.isRunning();
        setError(message);
    });
    connect(&m_runtime, &TunnelRuntimeService::statusChanged, this, [this](const QString &message) {
        m_frpcRunning = m_runtime.isRunning();
        setStatus(message);
        appendLog(message);
    });
}

QString AppController::apiBaseUrl() const { return m_profile.apiBaseUrl; }
QString AppController::accessToken() const { return m_profile.accessToken; }
QString AppController::clientId() const { return m_profile.clientId; }
QString AppController::frpcPath() const { return m_profile.frpcPath; }
QString AppController::runtimeDir() const { return m_profile.runtimeDir; }
bool AppController::connected() const { return m_connected; }
bool AppController::busy() const { return m_busy; }
bool AppController::frpcRunning() const { return m_frpcRunning; }
bool AppController::quitInProgress() const { return m_quitInProgress; }
bool AppController::allowClose() const { return m_allowClose; }
QString AppController::statusMessage() const { return m_statusMessage; }
QString AppController::errorMessage() const { return m_errorMessage; }
QString AppController::logText() const { return m_logText; }
QString AppController::userName() const { return m_policy.user; }
QString AppController::tokenName() const { return m_policy.tokenName; }
int AppController::portStart() const { return m_policy.policy.portStart; }
int AppController::portEnd() const { return m_policy.policy.portEnd; }
int AppController::maxPorts() const { return m_policy.policy.maxPorts; }
bool AppController::dpiEnabled() const { return m_policy.dpi.enabled; }
QString AppController::dpiMode() const { return m_policy.dpi.mode; }

void AppController::setApiBaseUrl(const QString &value)
{
    if (m_profile.apiBaseUrl == value.trimmed()) {
        return;
    }
    m_profile.apiBaseUrl = value.trimmed();
    m_connected = false;
    stopHeartbeat();
    emit profileChanged();
    emit stateChanged();
}

void AppController::setAccessToken(const QString &value)
{
    if (m_profile.accessToken == value.trimmed()) {
        return;
    }
    m_profile.accessToken = value.trimmed();
    m_connected = false;
    stopHeartbeat();
    emit profileChanged();
    emit stateChanged();
}

void AppController::setClientId(const QString &value)
{
    Q_UNUSED(value);
    const QString hardwareClientId = m_profiles.ensureClientId();
    if (m_profile.clientId == hardwareClientId) {
        return;
    }
    m_profile.clientId = hardwareClientId;
    m_connected = false;
    stopHeartbeat();
    emit profileChanged();
    emit stateChanged();
}

void AppController::setFrpcPath(const QString &value)
{
    if (m_profile.frpcPath == value.trimmed()) {
        return;
    }
    m_profile.frpcPath = value.trimmed();
    emit profileChanged();
}

void AppController::setRuntimeDir(const QString &value)
{
    if (m_profile.runtimeDir == value.trimmed()) {
        return;
    }
    m_profile.runtimeDir = value.trimmed();
    emit profileChanged();
}

QString AppController::frpEndpoint() const
{
    if (m_policy.frpServerAddr.isEmpty() || m_policy.frpServerPort <= 0) {
        return "-";
    }
    return QString("%1:%2").arg(m_policy.frpServerAddr).arg(m_policy.frpServerPort);
}

QVariantList AppController::allowedProtocols() const
{
    QVariantList protocols;
    for (const auto &protocol : m_policy.policy.allowedProtocols) {
        if (protocol == "tcp" || protocol == "udp") {
            protocols << protocol;
        }
    }
    return protocols;
}

QString AppController::allowedProtocolsText() const
{
    return m_policy.policy.allowedProtocols.isEmpty() ? "-" : m_policy.policy.allowedProtocols.join(", ");
}

QString AppController::dpiBlockedText() const
{
    return m_policy.dpi.blockedTrafficTypes.isEmpty() ? "无" : m_policy.dpi.blockedTrafficTypes.join(", ");
}

QString AppController::dpiDetectorsText() const
{
    return m_policy.dpi.enabledDetectors.isEmpty() ? "无" : m_policy.dpi.enabledDetectors.join(", ");
}

QVariantList AppController::tunnelList() const
{
    QVariantList items;
    for (int i = 0; i < m_tunnels.size(); ++i) {
        const auto &tunnel = m_tunnels.at(i);
        QVariantMap item;
        item["index"] = i;
        item["name"] = tunnel.name;
        item["type"] = tunnel.type;
        item["local_ip"] = tunnel.localIp;
        item["local_port"] = tunnel.localPort;
        item["remote_port"] = tunnel.remotePort;
        item["remote_endpoint"] = remoteEndpoint(tunnel);
        item["status"] = m_frpcRunning ? "运行中" : "待启动";
        items << item;
    }
    return items;
}

int AppController::tunnelCount() const
{
    return m_tunnels.size();
}

int AppController::tunnelLimit() const
{
    return m_policy.policy.maxPorts > 0 ? m_policy.policy.maxPorts : 1;
}

void AppController::saveProfile()
{
    QString error;
    if (!m_profiles.save(m_profile, &error)) {
        setError(error);
        return;
    }
    setStatus("配置已保存");
}

void AppController::connectToServer()
{
    if (m_logoutAction != LogoutAction::None) {
        return;
    }
    m_allowClose = false;
    const QString hardwareClientId = m_profiles.ensureClientId();
    if (m_profile.clientId != hardwareClientId) {
        m_profile.clientId = hardwareClientId;
        emit profileChanged();
    }
    if (m_profile.apiBaseUrl.trimmed().isEmpty() || m_profile.accessToken.trimmed().isEmpty()) {
        setError("请填写 API 地址和用户 Token");
        return;
    }
    saveProfile();
    setBusy(true, "正在连接服务器并获取权限");
    appendLog("正在连接服务器并获取权限...");
    m_api.queryResourcePolicy(m_profile.apiBaseUrl, m_profile.accessToken, m_profile.clientId);
}

void AppController::createTunnel(const QString &name, const QString &type, const QString &localIp, int localPort, int remotePort)
{
    TunnelDraft draft;
    draft.name = name.trimmed();
    draft.type = type.trimmed();
    draft.localIp = localIp.trimmed();
    draft.localPort = localPort;
    draft.remotePort = remotePort;

    m_tunnels = QList<TunnelDraft>{draft};
    emit tunnelsChanged();
    startTunnels();
}

void AppController::addTunnel(const QString &name, const QString &type, const QString &localIp, int localPort, int remotePort)
{
    TunnelDraft draft;
    draft.name = name.trimmed();
    draft.type = type.trimmed();
    draft.localIp = localIp.trimmed();
    draft.localPort = localPort;
    draft.remotePort = remotePort;

    QString error;
    if (!validateTunnel(draft, &error)) {
        setError(error);
        return;
    }
    if (m_tunnels.size() >= tunnelLimit()) {
        setError(QString("最多只能添加 %1 个隧道").arg(tunnelLimit()));
        return;
    }
    for (const auto &tunnel : std::as_const(m_tunnels)) {
        if (tunnel.name == draft.name) {
            setError("隧道名称不能重复");
            return;
        }
        if (tunnel.remotePort == draft.remotePort) {
            setError("服务端端口不能重复");
            return;
        }
    }
    m_tunnels << draft;
    appendLog(QString("已加入隧道列表：%1 %2:%3 -> :%4")
                  .arg(draft.name, draft.localIp)
                  .arg(draft.localPort)
                  .arg(draft.remotePort));
    emit tunnelsChanged();
}

void AppController::removeTunnel(int index)
{
    if (index < 0 || index >= m_tunnels.size()) {
        setError("隧道索引无效");
        return;
    }
    const auto removed = m_tunnels.takeAt(index);
    appendLog("已从列表移除隧道：" + removed.name);
    emit tunnelsChanged();
}

void AppController::startTunnels()
{
    requestBootstrapForTunnels(m_tunnels, "正在启动隧道列表");
}

void AppController::stopTunnel(int index)
{
    if (index < 0 || index >= m_tunnels.size()) {
        setError("隧道索引无效");
        return;
    }
    const auto removed = m_tunnels.takeAt(index);
    appendLog("正在关闭隧道：" + removed.name);
    emit tunnelsChanged();

    if (m_runtime.isRunning()) {
        m_runtime.stop();
        m_frpcRunning = false;
        emit stateChanged();
        emit tunnelsChanged();
    }
    if (m_tunnels.isEmpty()) {
        setStatus("隧道已全部停止");
        return;
    }
    requestBootstrapForTunnels(m_tunnels, "正在重启剩余隧道");
}

void AppController::stopAllTunnels()
{
    m_runtime.stop();
    m_frpcRunning = false;
    emit stateChanged();
    emit tunnelsChanged();
}

void AppController::copyRemoteEndpoint(int index)
{
    if (index < 0 || index >= m_tunnels.size()) {
        setError("隧道索引无效");
        return;
    }
    const QString endpoint = remoteEndpoint(m_tunnels.at(index));
    if (endpoint.isEmpty()) {
        setError("当前没有可复制的远程地址");
        return;
    }
    QGuiApplication::clipboard()->setText(endpoint);
    setStatus("已复制远程地址：" + endpoint);
    appendLog("已复制远程地址：" + endpoint);
}

void AppController::stopFrpc()
{
    stopAllTunnels();
}

void AppController::quitClient()
{
    if (m_allowClose || !m_connected) {
        QCoreApplication::quit();
        return;
    }
    requestLogout(LogoutAction::QuitApplication, "正在退出客户端");
}

void AppController::clearLogs()
{
    m_logText.clear();
    m_logLineCount = 0;
    emit logsChanged();
}

void AppController::setBusy(bool value, const QString &message)
{
    m_busy = value;
    if (!message.isEmpty()) {
        m_statusMessage = message;
    }
    if (value) {
        m_errorMessage.clear();
    }
    emit stateChanged();
}

void AppController::setError(const QString &message)
{
    m_errorMessage = message.trimmed().isEmpty() ? "操作失败" : message.trimmed();
    m_statusMessage = m_errorMessage;
    appendLog("错误：" + m_errorMessage);
    emit stateChanged();
}

void AppController::setStatus(const QString &message)
{
    m_statusMessage = message;
    emit stateChanged();
}

void AppController::appendLog(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (!m_logText.isEmpty()) {
        m_logText += "\n";
    }
    m_logText += trimmed;
    m_logLineCount += trimmed.count('\n') + 1;

    qsizetype removeThrough = 0;
    int linesToRemove = qMax(0, m_logLineCount - kMaxLogLines);
    for (int i = 0; i < linesToRemove; ++i) {
        const qsizetype newline = m_logText.indexOf('\n', removeThrough);
        if (newline < 0) {
            removeThrough = m_logText.size();
            break;
        }
        removeThrough = newline + 1;
    }
    if (m_logText.size() - removeThrough > kMaxLogCharacters) {
        const qsizetype minimumCut = m_logText.size() - kMaxLogCharacters;
        const qsizetype newline = m_logText.indexOf('\n', qMax(removeThrough, minimumCut));
        removeThrough = newline >= 0 ? newline + 1 : minimumCut;
    }
    if (removeThrough > 0) {
        m_logText.remove(0, removeThrough);
        m_logLineCount = m_logText.isEmpty() ? 0 : m_logText.count('\n') + 1;
    }
    emit logsChanged();
}

bool AppController::validateTunnel(const TunnelDraft &draft, QString *errorMessage) const
{
    if (!m_connected) {
        *errorMessage = "请先连接服务器获取权限";
        return false;
    }
    if (draft.name.isEmpty() || draft.localIp.isEmpty()) {
        *errorMessage = "请填写隧道名称和本地地址";
        return false;
    }
    if (!m_policy.policy.allowedProtocols.contains(draft.type)) {
        *errorMessage = "服务端未授权该穿透协议";
        return false;
    }
    if (draft.localPort < 1 || draft.localPort > 65535) {
        *errorMessage = "本地端口无效";
        return false;
    }
    if (draft.remotePort < m_policy.policy.portStart || draft.remotePort > m_policy.policy.portEnd) {
        *errorMessage = "服务端端口不在授权范围内";
        return false;
    }
    return true;
}

bool AppController::validateTunnelList(const QList<TunnelDraft> &tunnels, QString *errorMessage) const
{
    if (tunnels.isEmpty()) {
        *errorMessage = "请先添加至少一个隧道";
        return false;
    }
    if (tunnels.size() > tunnelLimit()) {
        *errorMessage = QString("隧道数量 %1 超过服务端限制 %2").arg(tunnels.size()).arg(tunnelLimit());
        return false;
    }
    QSet<QString> names;
    QSet<int> remotePorts;
    for (const auto &tunnel : tunnels) {
        if (!validateTunnel(tunnel, errorMessage)) {
            return false;
        }
        if (names.contains(tunnel.name)) {
            *errorMessage = "隧道名称不能重复";
            return false;
        }
        if (remotePorts.contains(tunnel.remotePort)) {
            *errorMessage = "服务端端口不能重复";
            return false;
        }
        names.insert(tunnel.name);
        remotePorts.insert(tunnel.remotePort);
    }
    return true;
}

QString AppController::remoteEndpoint(const TunnelDraft &tunnel) const
{
    if (m_policy.frpServerAddr.trimmed().isEmpty() || tunnel.remotePort <= 0) {
        return {};
    }
    return QString("%1:%2").arg(m_policy.frpServerAddr.trimmed()).arg(tunnel.remotePort);
}

void AppController::requestBootstrapForTunnels(const QList<TunnelDraft> &tunnels, const QString &statusMessage)
{
    m_profile = ClientProfile{m_profile.apiBaseUrl, m_profile.accessToken, m_profile.clientId, m_profile.frpcPath, m_profile.runtimeDir};
    saveProfile();

    QString error;
    if (!validateTunnelList(tunnels, &error)) {
        setError(error);
        return;
    }
    if (!QFileInfo::exists(m_profile.frpcPath)) {
        setError("找不到 frpc 程序：" + m_profile.frpcPath);
        return;
    }

    setBusy(true, statusMessage);
    appendLog(QString("正在请求服务端下发 %1 个隧道配置...").arg(tunnels.size()));
    m_api.bootstrap(m_profile.apiBaseUrl, m_profile.accessToken, m_profile.clientId, tunnels);
}

void AppController::applyPolicy(const ResourcePolicyResponse &response)
{
    m_policy = response;
    m_connected = true;
    m_allowClose = false;
    m_quitInProgress = false;
    m_statusMessage = "鉴权通过，已获取服务端授权";
    appendLog(QString("鉴权通过：%1，frps=%2").arg(m_policy.user, frpEndpoint()));
    startHeartbeat();
    emit stateChanged();
    emit policyChanged();
    emit tunnelsChanged();
}

void AppController::startHeartbeat()
{
    if (!m_heartbeatTimer.isActive()) {
        m_heartbeatTimer.start();
    }
    sendHeartbeat();
}

void AppController::stopHeartbeat()
{
    if (m_heartbeatTimer.isActive()) {
        m_heartbeatTimer.stop();
    }
}

void AppController::sendHeartbeat()
{
    if (!m_connected || m_profile.apiBaseUrl.trimmed().isEmpty() || m_profile.accessToken.trimmed().isEmpty() || m_profile.clientId.trimmed().isEmpty()) {
        return;
    }
    m_api.heartbeat(m_profile.apiBaseUrl, m_profile.accessToken, m_profile.clientId, m_runtime.isRunning());
}

void AppController::handleHeartbeatResponse(const HeartbeatResponse &response)
{
    if (!response.ok) {
        const QString reason = response.reason.isEmpty() ? "服务端拒绝了客户端心跳，请重新鉴权" : response.reason;
        requestLogout(LogoutAction::ReturnToAuth, reason);
        return;
    }
    for (const auto &command : response.commands) {
        executeClientCommand(command);
    }
}

void AppController::executeClientCommand(const ClientCommand &command)
{
    const QString name = command.command.trimmed();
    const QString message = command.message.trimmed().isEmpty() ? "服务端检测到违规行为，请规范操作" : command.message.trimmed();
    if (name == "stop_frpc") {
        appendLog("收到服务端命令：关闭 frpc");
        m_runtime.stop();
        m_frpcRunning = false;
        setStatus(message);
        emit stateChanged();
        emit tunnelsChanged();
        return;
    }
    if (name == "show_warning") {
        appendLog("收到服务端弹窗提醒：" + message);
        emit remoteMessageRequested(message);
        return;
    }
    if (name == "reauth") {
        appendLog("收到服务端命令：重新鉴权");
        requestLogout(LogoutAction::ReturnToAuth, message);
        return;
    }
    appendLog("收到未知服务端命令：" + name);
}

void AppController::returnToAuthScreen(const QString &message)
{
    stopHeartbeat();
    m_runtime.stop();
    m_frpcRunning = false;
    m_connected = false;
    m_busy = false;
    m_quitInProgress = false;
    m_allowClose = false;
    m_policy = ResourcePolicyResponse{};
    m_statusMessage = message.trimmed().isEmpty() ? "服务端要求重新鉴权" : message.trimmed();
    m_errorMessage.clear();
    emit stateChanged();
    emit policyChanged();
    emit tunnelsChanged();
}

void AppController::requestLogout(LogoutAction action, const QString &message)
{
    if (m_logoutAction != LogoutAction::None) {
        return;
    }
    m_logoutAction = action;
    m_logoutMessage = message.trimmed();
    m_quitInProgress = action == LogoutAction::QuitApplication;
    m_allowClose = false;
    setBusy(true, "正在通知服务端下线");
    appendLog("正在通知服务端下线...");

    stopHeartbeat();
    const bool wasFrpcRunning = m_runtime.isRunning();
    if (wasFrpcRunning) {
        m_runtime.stop();
        m_frpcRunning = false;
        emit tunnelsChanged();
    }
    emit stateChanged();

    if (!m_connected || m_profile.apiBaseUrl.trimmed().isEmpty() || m_profile.accessToken.trimmed().isEmpty() || m_profile.clientId.trimmed().isEmpty()) {
        finishLogout();
        return;
    }

    m_api.logout(m_profile.apiBaseUrl, m_profile.accessToken, m_profile.clientId, wasFrpcRunning);
    QTimer::singleShot(3000, this, [this]() {
        if (m_logoutAction == LogoutAction::None) {
            return;
        }
        appendLog("下线请求等待超时，继续清理本地状态。");
        finishLogout();
    });
}

void AppController::finishLogout()
{
    if (m_logoutAction == LogoutAction::None) {
        return;
    }
    const LogoutAction action = m_logoutAction;
    const QString message = m_logoutMessage;
    m_logoutAction = LogoutAction::None;
    m_logoutMessage.clear();
    m_busy = false;

    if (action == LogoutAction::ReturnToAuth) {
        returnToAuthScreen(message.isEmpty() ? "服务端要求重新鉴权" : message);
        return;
    }

    m_connected = false;
    m_frpcRunning = false;
    m_policy = ResourcePolicyResponse{};
    m_quitInProgress = false;
    m_allowClose = true;
    emit stateChanged();
    emit policyChanged();
    emit tunnelsChanged();
    QCoreApplication::quit();
}
