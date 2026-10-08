#include "tunnel_runtime_service.h"
#include "log_safety.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>

TunnelRuntimeService::TunnelRuntimeService(QObject *parent)
    : QObject(parent)
{
    m_forceKillTimer.setSingleShot(true);
    m_forceKillTimer.setInterval(3000);
    connect(&m_forceKillTimer, &QTimer::timeout, this, [this]() {
        if (isRunning()) {
            debugLog(QString("[PROCESS] force_kill pid=%1 state=%2 stop_source=%3 timeout_ms=%4")
                .arg(m_process.processId()).arg(static_cast<int>(m_process.state())).arg(m_stopSource)
                .arg(m_forceKillTimer.interval()));
            emit statusChanged("frpc 停止超时，正在强制结束...");
            m_process.kill();
        }
    });
    connect(&m_process, &QProcess::started, this, [this]() {
        debugLog(QString("[PROCESS] started pid=%1 config=%2").arg(m_process.processId()).arg(m_configPath));
        emit statusChanged("frpc 进程已启动，正在连接服务端（隧道是否建立请查看下方日志）。");
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        readLogLines(m_stdoutBuffer,m_process.readAllStandardOutput());
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        readLogLines(m_stderrBuffer,m_process.readAllStandardError());
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        debugLog(QString("[PROCESS] finished exit_code=%1 exit_status=%2 intentional=%3 pending_start=%4 stop_source=%5")
            .arg(exitCode).arg(static_cast<int>(exitStatus)).arg(m_stoppingIntentionally)
            .arg(m_hasPendingStart).arg(m_stopSource));
        m_forceKillTimer.stop();
        readLogLines(m_stdoutBuffer,m_process.readAllStandardOutput(),true);
        readLogLines(m_stderrBuffer,m_process.readAllStandardError(),true);
        releaseActiveLease();
        const bool intentional = m_stoppingIntentionally;
        m_stoppingIntentionally = false;
        if (intentional) {
            if (m_hasPendingStart) {
                const auto profile = m_pendingProfile;
                const auto bootstrap = m_pendingBootstrap;
                m_hasPendingStart = false;
                emit statusChanged("frpc 已停止，正在应用新隧道配置...");
                startNow(profile, bootstrap);
                return;
            }
            emit statusChanged("frpc 已停止。");
            return;
        }
        emit statusChanged(QString("frpc 已停止，退出码=%1，状态=%2").arg(exitCode).arg(exitStatus == QProcess::NormalExit && exitCode == 0 ? "正常结束" : "运行失败，非手动退出；请查看前面的连接错误"));
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        debugLog(QString("[PROCESS] error code=%1 message=%2 intentional=%3 stop_source=%4")
            .arg(static_cast<int>(error)).arg(m_process.errorString()).arg(m_stoppingIntentionally).arg(m_stopSource));
        if (error == QProcess::FailedToStart) releaseActiveLease();
        if (m_stoppingIntentionally && error == QProcess::Crashed) {
            return;
        }
        emit failed(m_process.errorString());
    });
    connect(&m_process, &QProcess::stateChanged, this, [this](QProcess::ProcessState state) {
        debugLog(QString("[PROCESS] state=%1 pid=%2 intentional=%3")
            .arg(static_cast<int>(state)).arg(m_process.processId()).arg(m_stoppingIntentionally));
    });
}

void TunnelRuntimeService::debugLog(const QString &line)
{
    if (m_debugMode) emit diagnostic(line);
}

bool TunnelRuntimeService::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

QString TunnelRuntimeService::currentConfigPath() const
{
    return m_configPath;
}

void TunnelRuntimeService::start(const ClientProfile &profile, const BootstrapResponse &bootstrap)
{
    debugLog(QString("[PROCESS] start requested running=%1 pending=%2 config_bytes=%3 expires=%4")
        .arg(isRunning()).arg(m_hasPendingStart).arg(bootstrap.frpcConfig.toUtf8().size()).arg(bootstrap.expiresAt));
    if (isRunning()) {
        if (m_hasPendingStart) {
            emit leaseEnded(m_pendingProfile.selectedNodeApiUrl, m_pendingProfile.accessToken, m_pendingProfile.clientId, m_pendingBootstrap.leaseId);
        }
        m_pendingProfile = profile;
        m_pendingBootstrap = bootstrap;
        m_hasPendingStart = true;
        requestStop(true, "runtime.apply_new_configuration");
        return;
    }
    startNow(profile, bootstrap);
}

void TunnelRuntimeService::startNow(const ClientProfile &profile, const BootstrapResponse &bootstrap)
{
    m_stdoutBuffer.clear();
    m_stderrBuffer.clear();
    m_stopSource.clear();
    m_activeProfile = profile;
    m_activeLeaseId = bootstrap.leaseId;
    if (!QFileInfo::exists(profile.frpcPath)) {
        releaseActiveLease();
        emit failed("找不到 frpc 程序：" + profile.frpcPath);
        return;
    }

    QString errorMessage;
    m_configPath = writeConfig(profile, bootstrap, &errorMessage);
    if (m_configPath.isEmpty()) {
        releaseActiveLease();
        emit failed(errorMessage);
        return;
    }

    emit statusChanged("正在启动 frpc...");
    emit logLine("配置文件：" + m_configPath);
    m_process.setProgram(profile.frpcPath);
    m_process.setArguments(QStringList{"-c", m_configPath});
    debugLog("[PROCESS] launch program=" + profile.frpcPath + " args=-c " + m_configPath);
    m_process.start();
}

void TunnelRuntimeService::readLogLines(QByteArray &buffer, const QByteArray &data, bool flush)
{
    buffer += data;
    static const QRegularExpression ansi(QStringLiteral("\x1b\\[[0-9;]*[A-Za-z]"));
    qsizetype newline;
    while ((newline = buffer.indexOf('\n')) >= 0) {
        const auto line = QString::fromUtf8(buffer.first(newline)).remove(ansi).trimmed();
        buffer.remove(0,newline+1);
        if (!line.isEmpty()) {
            emit logLine(m_debugMode ? (QString("[FRPC %1] ").arg(&buffer == &m_stdoutBuffer ? "stdout" : "stderr") + line) : line);
        }
    }
    if ((flush || buffer.size()>128*1024) && !buffer.isEmpty()) {
        const auto line = QString::fromUtf8(buffer).remove(ansi).trimmed();
        buffer.clear();
        if (!line.isEmpty()) {
            emit logLine(m_debugMode ? (QString("[FRPC %1] ").arg(&buffer == &m_stdoutBuffer ? "stdout" : "stderr") + line) : line);
        }
    }
}

void TunnelRuntimeService::releaseActiveLease()
{
    if (m_activeLeaseId.isEmpty()) return;
    debugLog("[LEASE] runtime ended; request release of active lease");
    const QString leaseId = m_activeLeaseId;
    m_activeLeaseId.clear();
    emit leaseEnded(m_activeProfile.selectedNodeApiUrl, m_activeProfile.accessToken, m_activeProfile.clientId, leaseId);
}

void TunnelRuntimeService::stop(const QString &source)
{
    requestStop(false, source);
}

void TunnelRuntimeService::requestStop(bool restartAfterStop, const QString &source)
{
    m_stopSource = source;
    debugLog(QString("[PROCESS] stop request source=%1 restart=%2 running=%3 pid=%4 pending_start=%5")
        .arg(source).arg(restartAfterStop).arg(isRunning()).arg(m_process.processId()).arg(m_hasPendingStart));
    if (!restartAfterStop && m_hasPendingStart) {
        emit leaseEnded(m_pendingProfile.selectedNodeApiUrl, m_pendingProfile.accessToken, m_pendingProfile.clientId, m_pendingBootstrap.leaseId);
        m_hasPendingStart = false;
    }
    if (!isRunning()) {
        if (restartAfterStop && m_hasPendingStart) {
            const auto profile = m_pendingProfile;
            const auto bootstrap = m_pendingBootstrap;
            m_hasPendingStart = false;
            startNow(profile, bootstrap);
        } else {
            emit statusChanged("frpc 当前未运行。");
        }
        return;
    }
    // Always record stop origin, even with debug mode off.
    emit logLine("frpc 停止来源：" + source);
    emit statusChanged(restartAfterStop ? "正在重启 frpc..." : "正在停止 frpc...");
    m_stoppingIntentionally = true;
    debugLog("[PROCESS] calling QProcess::terminate; force-kill fallback in 3000ms");
    m_process.terminate();
    m_forceKillTimer.start();
}

QString TunnelRuntimeService::writeConfig(const ClientProfile &profile, const BootstrapResponse &bootstrap, QString *errorMessage)
{
    QDir dir(profile.runtimeDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        if (errorMessage) {
            *errorMessage = "创建运行目录失败。";
        }
        return {};
    }
    const QString lease = bootstrap.leaseId.isEmpty() ? "latest" : bootstrap.leaseId;
    static const QRegularExpression safeName(QStringLiteral("^[A-Za-z0-9_-]{1,96}$"));
    if (!safeName.match(lease).hasMatch()) {
        if (errorMessage) *errorMessage = "服务端返回的租约标识无效。";
        return {};
    }
    const QString path = dir.filePath(lease + ".toml");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return {};
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QString config = bootstrap.frpcConfig;
    // Register runtime credentials before frpc can emit any log containing them.
    const QRegularExpression credential(QStringLiteral(R"re((?:auth\.token|metadatas\.token)\s*=\s*"([^"\r\n]+)")re"));
    auto matches = credential.globalMatch(config);
    while (matches.hasNext()) LogSafety::rememberSecret(matches.next().captured(1));
    if (m_debugMode) {
        // Generated configuration uses root dotted TOML keys. Only log settings
        // are changed; network/auth/proxy configuration is left untouched.
        const qsizetype section = config.indexOf(QRegularExpression(QStringLiteral("(?m)^\\s*\\[")));
        QString root = section < 0 ? config : config.left(section);
        const QString tail = section < 0 ? QString() : config.mid(section);
        for (const auto &key : QStringList{"level", "to", "disablePrintColor"}) {
            root.remove(QRegularExpression(QString("(?m)^\\s*log\\.%1\\s*=[^\\r\\n]*[\\r\\n]*").arg(key)));
        }
        config = root + "\nlog.level = \"debug\"\nlog.to = \"console\"\nlog.disablePrintColor = true\n" + tail;
        debugLog("[CONFIG] frpc log.level=debug log.to=console; authentication/config contents not printed");
        const QRegularExpression networkSetting(QStringLiteral(
            R"((?m)^(?:serverAddr|serverPort|loginFailExit|transport\.tls\.enable|transport\.tcpMux|transport\.tcpMuxKeepaliveInterval)\s*=[^\r\n]*)"));
        auto settings = networkSetting.globalMatch(root);
        while (settings.hasNext()) debugLog("[CONFIG] " + settings.next().captured());
    }
    const auto content = config.toUtf8();
    if (file.write(content) != content.size() || !file.commit()) {
        if (errorMessage) *errorMessage = "写入 FRP 配置失败：" + file.errorString();
        return {};
    }
    debugLog(QString("[CONFIG] saved path=%1 bytes=%2").arg(path).arg(content.size()));
    return path;
}
