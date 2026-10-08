#pragma once

#include "models.h"
#include "profile_service.h"

#include <QObject>
#include <QProcess>
#include <QTimer>

class TunnelRuntimeService : public QObject {
    Q_OBJECT

public:
    explicit TunnelRuntimeService(QObject *parent = nullptr);

    bool isRunning() const;
    QString currentConfigPath() const;
    void setDebugMode(bool enabled) { m_debugMode = enabled; }

public slots:
    void start(const ClientProfile &profile, const BootstrapResponse &bootstrap);
    void stop(const QString &source);

signals:
    void diagnostic(const QString &line);
    void statusChanged(const QString &status);
    void logLine(const QString &line);
    void failed(const QString &message);
    void leaseEnded(const QString &apiUrl, const QString &token, const QString &clientId, const QString &leaseId);

private:
    QString writeConfig(const ClientProfile &profile, const BootstrapResponse &bootstrap, QString *errorMessage);
    void startNow(const ClientProfile &profile, const BootstrapResponse &bootstrap);
    void requestStop(bool restartAfterStop, const QString &source);
    void releaseActiveLease();
    void readLogLines(QByteArray &buffer, const QByteArray &data, bool flush = false);

    QProcess m_process;
    QTimer m_forceKillTimer;
    QString m_configPath;
    QString m_activeLeaseId;
    QByteArray m_stdoutBuffer;
    QByteArray m_stderrBuffer;
    ClientProfile m_activeProfile;
    bool m_stoppingIntentionally = false;
    bool m_hasPendingStart = false;
    ClientProfile m_pendingProfile;
    BootstrapResponse m_pendingBootstrap;
    bool m_debugMode = false;
    QString m_stopSource;
    void debugLog(const QString &line);
};
