#pragma once

#include <QObject>
#include <QString>

struct ClientProfile {
    QString apiBaseUrl;
    QString accessToken;
    QString clientId;
    QString frpcPath;
    QString runtimeDir;
};

class ProfileService : public QObject {
    Q_OBJECT

public:
    explicit ProfileService(QObject *parent = nullptr);

    ClientProfile load();
    bool save(const ClientProfile &profile, QString *errorMessage = nullptr);
    QString profilePath() const;
    QString defaultRuntimeDir() const;
    QString ensureClientId() const;

private:
    QString defaultFrpcPath() const;
    QString legacyProfilePath() const;
};
