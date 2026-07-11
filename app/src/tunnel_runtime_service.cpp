#include "tunnel_runtime_service.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

TunnelRuntimeService::TunnelRuntimeService(QObject *parent)
    : QObject(parent)
{
    m_forceKillTimer.setSingleShot(true);
    m_forceKillTimer.setInterval(3000);
    connect(&m_forceKillTimer, &QTimer::timeout, this, [this]() {
        if (isRunning()) {
            emit statusChanged("frpc 停止超时，正在强制结束...");
            m_process.kill();
        }
    });
    connect(&m_process, &QProcess::started, this, [this]() {
        emit statusChanged("frpc 运行中。");
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString text = QString::fromLocal8Bit(m_process.readAllStandardOutput());
        for (const auto &line : text.split('\n', Qt::SkipEmptyParts)) {
            emit logLine(line.trimmed());
        }
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        const QString text = QString::fromLocal8Bit(m_process.readAllStandardError());
        for (const auto &line : text.split('\n', Qt::SkipEmptyParts)) {
            emit logLine(line.trimmed());
        }
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        m_forceKillTimer.stop();
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
        emit statusChanged(QString("frpc 已停止，退出码=%1，状态=%2").arg(exitCode).arg(exitStatus == QProcess::NormalExit ? "正常退出" : "异常退出"));
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (m_stoppingIntentionally && error == QProcess::Crashed) {
            return;
        }
        emit failed(m_process.errorString());
    });
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
    if (isRunning()) {
        m_pendingProfile = profile;
        m_pendingBootstrap = bootstrap;
        m_hasPendingStart = true;
        requestStop(true);
        return;
    }
    startNow(profile, bootstrap);
}

void TunnelRuntimeService::startNow(const ClientProfile &profile, const BootstrapResponse &bootstrap)
{
    if (!QFileInfo::exists(profile.frpcPath)) {
        emit failed("找不到 frpc 程序：" + profile.frpcPath);
        return;
    }

    QString errorMessage;
    m_configPath = writeConfig(profile, bootstrap, &errorMessage);
    if (m_configPath.isEmpty()) {
        emit failed(errorMessage);
        return;
    }

    emit statusChanged("正在启动 frpc...");
    emit logLine("配置文件：" + m_configPath);
    m_process.setProgram(profile.frpcPath);
    m_process.setArguments(QStringList{"-c", m_configPath});
    m_process.start();
}

void TunnelRuntimeService::stop()
{
    requestStop(false);
}

void TunnelRuntimeService::requestStop(bool restartAfterStop)
{
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
    emit statusChanged(restartAfterStop ? "正在重启 frpc..." : "正在停止 frpc...");
    m_stoppingIntentionally = true;
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
    const QString path = dir.filePath(lease + ".toml");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return {};
    }
    file.write(bootstrap.frpcConfig.toUtf8());
    return path;
}
