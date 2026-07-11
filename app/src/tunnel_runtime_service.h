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

public slots:
    void start(const ClientProfile &profile, const BootstrapResponse &bootstrap);
    void stop();

signals:
    void statusChanged(const QString &status);
    void logLine(const QString &line);
    void failed(const QString &message);

private:
    QString writeConfig(const ClientProfile &profile, const BootstrapResponse &bootstrap, QString *errorMessage);
    void startNow(const ClientProfile &profile, const BootstrapResponse &bootstrap);
    void requestStop(bool restartAfterStop);

    QProcess m_process;
    QTimer m_forceKillTimer;
    QString m_configPath;
    bool m_stoppingIntentionally = false;
    bool m_hasPendingStart = false;
    ClientProfile m_pendingProfile;
    BootstrapResponse m_pendingBootstrap;
};
