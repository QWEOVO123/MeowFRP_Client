#include "app_controller.h"
#include "log_safety.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QSysInfo>
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
    m_profile.accessToken.clear();
    connect(&m_api, &ControlApiClient::diagnostic, this, &AppController::debugLog);
    connect(&m_runtime, &TunnelRuntimeService::diagnostic, this, &AppController::debugLog);
    connect(&m_api, &ControlApiClient::accountAuthenticated, this, [this](const QString &token) {
        m_profile.accessToken = token;
        emit profileChanged();
    });
    m_heartbeatTimer.setInterval(10000);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &AppController::sendHeartbeat);
    connect(&m_api, &ControlApiClient::resourcePolicyLoaded, this, [this](const ResourcePolicyResponse &response) {
        setBusy(false);
        applyPolicy(response);
    });
    connect(&m_api, &ControlApiClient::nodeDirectoryLoaded, this, [this](const QList<NodeDirectoryEntry> &nodes) {
        setBusy(false, "请选择 FRP 节点");
        m_nodes = nodes;
        debugLog(QString("[AUTH] directory nodes=%1").arg(nodes.size()));
        emit nodeListChanged();
        emit nodeSelectionRequested();
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
        if (message.contains("中心节点失联") || message.contains("controller_disconnected")) {
            emit remoteMessageRequested("边缘节点与中心节点失联，暂时无法建立新的连接。");
        }
    });
    connect(&m_api, &ControlApiClient::heartbeatLoaded, this, &AppController::handleHeartbeatResponse);
    connect(&m_api, &ControlApiClient::heartbeatFailed, this, [this](const QString &message) {
        appendLog("心跳失败：" + message);
        debugLog("[HEARTBEAT] transport/API failure; keeping login and frpc running");
    });
    connect(&m_api, &ControlApiClient::commandAcknowledged, this, [this](qint64 commandId) {
        appendLog(QString("已通过 HTTPS API 确认服务端命令：%1").arg(commandId));
    });
    connect(&m_api, &ControlApiClient::commandAcknowledgeFailed, this, [this](qint64 commandId, const QString &message) {
        appendLog(QString("命令 %1 确认失败，将在下次心跳重试：%2").arg(commandId).arg(message));
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
    connect(&m_runtime, &TunnelRuntimeService::leaseEnded, &m_api, &ControlApiClient::releaseLease);
    connect(&m_api, &ControlApiClient::leaseReleaseFailed, this, [this](const QString &message) {
        appendLog("旧租约释放请求失败（下次启动会替换同一客户端旧租约）：" + message);
    });
    connect(&m_runtime, &TunnelRuntimeService::failed, this, [this](const QString &message) {
        m_frpcRunning = m_runtime.isRunning();
        setError(message);
        emit tunnelsChanged();
    });
    connect(&m_runtime, &TunnelRuntimeService::statusChanged, this, [this](const QString &message) {
        m_frpcRunning = m_runtime.isRunning();
        setStatus(message);
        appendLog(message);
        emit tunnelsChanged();
    });
    setDebugMode(m_profile.debugMode);
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
bool AppController::debugMode() const { return m_debugMode; }
QString AppController::debugLogPath() const { return m_debugLogPath; }

void AppController::openDebugLog()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) base = m_profile.runtimeDir;
    const QString directory = QDir(base).filePath("logs");
    if (!QDir().mkpath(directory)) {
        appendLog("调试日志目录创建失败；详细日志仍会显示在界面。");
        return;
    }
    m_debugLogPath = QDir(directory).filePath(QString("debug-%1-%2-%3.log")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz"))
        .arg(QCoreApplication::applicationPid()).arg(m_logoutGeneration));
    m_debugLogFile.setFileName(m_debugLogPath);
    if (!m_debugLogFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_debugLogPath.clear();
        appendLog("调试日志文件打开失败；详细日志仍会显示在界面。");
        return;
    }
    m_debugLogFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

void AppController::setDebugMode(bool enabled)
{
    if (m_debugMode == enabled) return;
    if (!enabled) appendLog("调试模式已关闭。");
    m_debugMode = enabled;
    m_profile.debugMode = enabled;
    m_api.setDebugMode(enabled);
    m_runtime.setDebugMode(enabled);
    if (enabled) {
        openDebugLog();
        appendLog("调试模式已开启：敏感凭据脱敏；详细日志保存在 " + m_debugLogPath);
        debugLog(QString("[ENV] app=%1 Qt=%2 OS=%3 arch=%4 pid=%5 heartbeat_ms=%6")
            .arg(QString(APP_VERSION), QString(qVersion()), QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture())
            .arg(QCoreApplication::applicationPid()).arg(m_heartbeatTimer.interval()));
        debugLog("[ENV] frpc=" + m_profile.frpcPath + " runtime=" + m_profile.runtimeDir);
    } else {
        m_debugLogFile.close();
    }
    // Only save the debug preference here; do not write an unsaved API edit.
    auto stored = m_profiles.load();
    stored.debugMode = enabled;
    QString error;
    if (!m_profiles.save(stored, &error)) appendLog("调试模式偏好保存失败：" + error);
    emit debugModeChanged();
}

void AppController::debugLog(const QString &line)
{
    if (m_debugMode) appendLog("[DEBUG] " + line);
}

void AppController::traceAction(const QString &source)
{
    debugLog(QString("[ACTION] source=%1 connected=%2 busy=%3 runtime_running=%4 tunnels=%5 logout_action=%6")
        .arg(source).arg(m_connected).arg(m_busy).arg(m_runtime.isRunning()).arg(m_tunnels.size())
        .arg(static_cast<int>(m_logoutAction)));
}
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

QVariantList AppController::nodeList() const
{
    QVariantList items;
    for (int i = 0; i < m_nodes.size(); ++i) {
        QVariantMap item;
        item["index"] = i;
        item["node_id"] = m_nodes.at(i).nodeId;
        item["tag"] = m_nodes.at(i).tag;
        item["api_url"] = m_nodes.at(i).apiUrl;
		item["online"] = m_nodes.at(i).online;
		item["node_type"] = m_nodes.at(i).nodeType;
        items << item;
    }
    return items;
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

void AppController::connectToServer(const QString &accessToken)
{
    LogSafety::rememberSecret(accessToken.trimmed());
    traceAction("ui.login_or_reauthenticate");
	if (m_busy) return;
	if (m_logoutAction != LogoutAction::None) {
		return;
	}
	if (m_connected) {
		requestLogout(LogoutAction::ReturnToAuth, "已断开当前节点，请重新连接并选择节点");
		return;
	}
    m_allowClose = false;
    const QString hardwareClientId = m_profiles.ensureClientId();
    if (m_profile.clientId != hardwareClientId) {
        m_profile.clientId = hardwareClientId;
        emit profileChanged();
    }
    if (m_profile.apiBaseUrl.trimmed().isEmpty() || accessToken.trimmed().isEmpty()) {
        setError("请填写中心 API 地址和系统生成的用户 Token");
        return;
    }
    saveProfile();
    m_nodes.clear();
    m_profile.accessToken.clear();
    emit nodeListChanged();
    setBusy(true, "正在向中心验证 Token 并获取授权节点");
    m_api.queryNodeDirectory(m_profile.apiBaseUrl, accessToken, m_profile.clientId);
}

void AppController::selectNode(int index)
{
    traceAction(QString("ui.select_node index=%1").arg(index));
    if (m_busy || m_profile.accessToken.isEmpty()) return;
    if (index < 0 || index >= m_nodes.size()) {
        setError("请选择有效的边缘节点");
        return;
    }
    const auto &node = m_nodes.at(index);
	if (!node.online) {
		setError("该节点当前离线，请选择在线节点");
		return;
	}
    m_profile.selectedNodeId = node.nodeId;
    m_profile.selectedNodeApiUrl = node.apiUrl;
    saveProfile();
    setBusy(true, "正在连接边缘节点并获取权限");
    appendLog(QString("已选择节点 %1（%2），正在直接鉴权...").arg(node.tag, node.apiUrl));
    m_api.queryResourcePolicy(node.apiUrl, m_profile.accessToken, m_profile.clientId);
}

void AppController::createTunnel(const QString &name, const QString &type, const QString &localIp, int localPort, int remotePort)
{
    traceAction("ui.create_tunnel");
    if (m_busy || m_logoutAction != LogoutAction::None) return;
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
    traceAction("ui.add_tunnel");
    debugLog(QString("[TUNNEL] name=%1 type=%2 local=%3:%4 remote_port=%5")
        .arg(name, type, localIp).arg(localPort).arg(remotePort));
    if (m_busy || m_logoutAction != LogoutAction::None) return;
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
    traceAction(QString("ui.remove_tunnel index=%1").arg(index));
    if (m_busy || m_logoutAction != LogoutAction::None) return;
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
    traceAction("ui.start_or_restart_tunnels");
    requestBootstrapForTunnels(m_tunnels, "正在启动隧道列表");
}

void AppController::stopTunnel(int index)
{
    traceAction(QString("ui.stop_single_tunnel index=%1").arg(index));
    if (m_busy || m_logoutAction != LogoutAction::None) return;
    if (index < 0 || index >= m_tunnels.size()) {
        setError("隧道索引无效");
        return;
    }
    const auto removed = m_tunnels.takeAt(index);
    appendLog("正在关闭隧道：" + removed.name);
    emit tunnelsChanged();

    if (m_runtime.isRunning()) {
        m_runtime.stop("ui.stop_single_tunnel:" + removed.name);
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
    traceAction("ui.stop_all_tunnels");
    if (m_logoutAction != LogoutAction::None) return;
    m_api.invalidateSession();
    setBusy(false);
    m_runtime.stop("ui.stop_all_tunnels");
    m_frpcRunning = false;
    emit stateChanged();
    emit tunnelsChanged();
}

void AppController::copyRemoteEndpoint(int index)
{
    traceAction(QString("ui.copy_remote_endpoint index=%1").arg(index));
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
    traceAction("ui.legacy_stop_frpc");
    stopAllTunnels();
}

void AppController::quitClient()
{
    traceAction("ui.quit_or_window_close");
    if (m_allowClose || !m_connected) {
        QCoreApplication::quit();
        return;
    }
    requestLogout(LogoutAction::QuitApplication, "正在退出客户端");
}

void AppController::clearLogs()
{
    debugLog("[ACTION] ui.clear_visible_logs; file log preserved");
    m_logText.clear();
    m_logLineCount = 0;
    emit logsChanged();
}

void AppController::copyLogs()
{
    QGuiApplication::clipboard()->setText(m_logText);
    setStatus("已复制当前日志（敏感凭据已脱敏）");
}

void AppController::recordUiWarning(const QString &message)
{
    debugLog("[QML] " + message);
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
    const QString trimmed = LogSafety::redact(line.trimmed());
    if (trimmed.isEmpty()) {
        return;
    }
    if (!m_logText.isEmpty()) {
        m_logText += "\n";
    }
    const QString record = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz t") + " " + trimmed;
    m_logText += record;
    if (m_debugMode && m_debugLogFile.isOpen()) {
        // Bound each session to a current 10 MiB file and one previous segment.
        if (m_debugLogFile.size() >= 10 * 1024 * 1024) {
            m_debugLogFile.close();
            const QString previous = m_debugLogPath + ".previous";
            // These are our own diagnostic files, never configuration/runtime files.
            const bool rotated = (!QFile::exists(previous) || QFile::remove(previous))
                && QFile::rename(m_debugLogPath, previous);
            if (rotated && m_debugLogFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
                m_debugLogFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        }
        if (m_debugLogFile.isOpen()) {
            const QByteArray data = record.toUtf8() + '\n';
            if (m_debugLogFile.write(data) != data.size() || !m_debugLogFile.flush()) {
                m_debugLogFile.close();
                m_logText += "\n调试日志写盘失败，后续日志仅显示在界面。";
                ++m_logLineCount;
            }
        } else {
            m_logText += "\n调试日志轮转失败，后续日志仅显示在界面。";
            ++m_logLineCount;
        }
    }
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
    if (draft.name.toUtf8().size()>48) { *errorMessage="隧道名称最多 48 个 UTF-8 字节"; return false; }
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
    if (m_busy || m_logoutAction != LogoutAction::None) return;
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
    m_api.bootstrap(m_profile.selectedNodeApiUrl, m_profile.accessToken, m_profile.clientId, tunnels);
}

void AppController::applyPolicy(const ResourcePolicyResponse &response)
{
    debugLog(QString("[AUTH] policy accepted ports=%1-%2 max_ports=%3 protocols=%4 DPI=%5 mode=%6")
        .arg(response.policy.portStart).arg(response.policy.portEnd).arg(response.policy.maxPorts)
        .arg(response.policy.allowedProtocols.join(',')).arg(response.dpi.enabled).arg(response.dpi.mode));
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
    debugLog(QString("[HEARTBEAT] start interval_ms=%1 immediate=true").arg(m_heartbeatTimer.interval()));
    if (!m_heartbeatTimer.isActive()) {
        m_heartbeatTimer.start();
    }
    sendHeartbeat();
}

void AppController::stopHeartbeat()
{
    debugLog(QString("[HEARTBEAT] stop timer active=%1").arg(m_heartbeatTimer.isActive()));
    if (m_heartbeatTimer.isActive()) {
        m_heartbeatTimer.stop();
    }
}

void AppController::sendHeartbeat()
{
    debugLog(QString("[HEARTBEAT] timer tick connected=%1 runtime_running=%2").arg(m_connected).arg(m_runtime.isRunning()));
    if (!m_connected || m_profile.selectedNodeApiUrl.trimmed().isEmpty() || m_profile.accessToken.trimmed().isEmpty() || m_profile.clientId.trimmed().isEmpty()) {
        return;
    }
    m_api.heartbeat(m_profile.selectedNodeApiUrl, m_profile.accessToken, m_profile.clientId, m_runtime.isRunning());
}

void AppController::handleHeartbeatResponse(const HeartbeatResponse &response)
{
    debugLog(QString("[HEARTBEAT] response ok=%1 commands=%2 reason=%3")
        .arg(response.ok).arg(response.commands.size()).arg(response.reason));
    if (!m_connected || m_logoutAction != LogoutAction::None) return;
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
    debugLog(QString("[COMMAND] id=%1 type=%2 message=%3").arg(command.id).arg(command.command, command.message));
    const QString commandKey = m_profile.selectedNodeApiUrl.trimmed() + ":" + QString::number(command.id);
    if (command.id > 0 && m_handledCommandKeys.contains(commandKey)) {
        debugLog("[COMMAND] already handled; ACK only, no repeated stop");
        acknowledgeClientCommand(command);
        return;
    }
    const QString name = command.command.trimmed();
    const QString message = command.message.trimmed().isEmpty() ? "服务端检测到违规行为，请规范操作" : command.message.trimmed();
    if (name == "stop_frpc") {
        appendLog("收到服务端命令：关闭 frpc");
        m_runtime.stop(QString("server.command.stop_frpc id=%1").arg(command.id));
        m_frpcRunning = false;
        setStatus(message);
        emit stateChanged();
        emit tunnelsChanged();
        if (command.id > 0) m_handledCommandKeys.insert(commandKey);
        acknowledgeClientCommand(command);
        return;
    }
    if (name == "show_warning") {
        appendLog("收到服务端弹窗提醒：" + message);
        emit remoteMessageRequested(message);
        if (command.id > 0) m_handledCommandKeys.insert(commandKey);
        acknowledgeClientCommand(command);
        return;
    }
    if (name == "reauth") {
        appendLog("收到服务端命令：重新鉴权");
        if (command.id > 0) m_handledCommandKeys.insert(commandKey);
        acknowledgeClientCommand(command);
        requestLogout(LogoutAction::ReturnToAuth, message);
        return;
    }
    appendLog("收到未知服务端命令：" + name);
}

void AppController::acknowledgeClientCommand(const ClientCommand &command)
{
    if (command.id <= 0) {
        return;
    }
    m_api.acknowledgeCommand(m_profile.selectedNodeApiUrl, m_profile.accessToken, m_profile.clientId, command.id);
}

void AppController::returnToAuthScreen(const QString &message)
{
    debugLog("[AUTH] return_to_login reason=" + message);
    m_api.invalidateSession();
    m_profile.accessToken.clear();
    m_nodes.clear();
    emit nodeListChanged();
    stopHeartbeat();
    m_runtime.stop("auth.return_to_login:" + message);
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
    debugLog(QString("[LOGOUT] request action=%1 existing_action=%2 reason=%3")
        .arg(static_cast<int>(action)).arg(static_cast<int>(m_logoutAction)).arg(message));
    if (m_logoutAction != LogoutAction::None) {
        return;
    }
    m_logoutAction = action;
    m_api.invalidateSession();
    const auto logoutGeneration = ++m_logoutGeneration;
    m_logoutMessage = message.trimmed();
    m_quitInProgress = action == LogoutAction::QuitApplication;
    m_allowClose = false;
    setBusy(true, "正在通知服务端下线");
    appendLog("正在通知服务端下线...");

    stopHeartbeat();
    const bool wasFrpcRunning = m_runtime.isRunning();
    if (wasFrpcRunning) {
        m_runtime.stop("auth.logout:" + message);
        m_frpcRunning = false;
        emit tunnelsChanged();
    }
    emit stateChanged();

    if (!m_connected || m_profile.selectedNodeApiUrl.trimmed().isEmpty() || m_profile.accessToken.trimmed().isEmpty() || m_profile.clientId.trimmed().isEmpty()) {
        finishLogout();
        return;
    }

    m_api.logout(m_profile.selectedNodeApiUrl, m_profile.accessToken, m_profile.clientId, wasFrpcRunning);
    QTimer::singleShot(3000, this, [this, logoutGeneration]() {
        if (m_logoutAction == LogoutAction::None || logoutGeneration != m_logoutGeneration) {
            return;
        }
        appendLog("下线请求等待超时，继续清理本地状态。");
        finishLogout();
    });
}

void AppController::finishLogout()
{
    debugLog(QString("[LOGOUT] finish action=%1").arg(static_cast<int>(m_logoutAction)));
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
