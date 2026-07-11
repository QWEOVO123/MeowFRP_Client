#include "profile_service.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>

ProfileService::ProfileService(QObject *parent)
    : QObject(parent)
{
}

ClientProfile ProfileService::load()
{
    ClientProfile profile;
    profile.runtimeDir = defaultRuntimeDir();
    profile.clientId = ensureClientId();
    profile.frpcPath = defaultFrpcPath();

    QString loadedPath = profilePath();
    QFile file(loadedPath);
    if (!file.exists() && QFileInfo::exists(legacyProfilePath())) {
        loadedPath = legacyProfilePath();
        file.setFileName(loadedPath);
    }
    if (!file.open(QIODevice::ReadOnly)) {
        save(profile);
        return profile;
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    const auto object = document.object();
    profile.apiBaseUrl = object.value("api_base_url").toString(profile.apiBaseUrl);
    profile.accessToken = object.value("access_token").toString();
    const QString storedClientId = object.value("client_id").toString().trimmed();
    const QString hardwareClientId = ensureClientId();
    profile.clientId = hardwareClientId.trimmed().isEmpty() ? storedClientId : hardwareClientId;
    profile.frpcPath = object.value("frpc_path").toString(profile.frpcPath);
    profile.runtimeDir = object.value("runtime_dir").toString(profile.runtimeDir);

    if (profile.clientId != storedClientId || loadedPath == legacyProfilePath()) {
        save(profile);
    }
    return profile;
}

bool ProfileService::save(const ClientProfile &profile, QString *errorMessage)
{
    QDir dir;
    if (!dir.mkpath(QFileInfo(profilePath()).absolutePath())) {
        if (errorMessage) {
            *errorMessage = "创建配置目录失败。";
        }
        return false;
    }

    QFile file(profilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QJsonObject object{
        {"api_base_url", profile.apiBaseUrl},
        {"access_token", profile.accessToken},
        {"client_id", profile.clientId},
        {"frpc_path", profile.frpcPath},
        {"runtime_dir", profile.runtimeDir},
    };
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}

QString ProfileService::profilePath() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.frp-control-client";
    }
    return QDir(base).filePath("frp-control-client.cfg.json");
}

QString ProfileService::defaultRuntimeDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(base.isEmpty() ? QDir::homePath() + "/.frp-control-client" : base).filePath("runtime");
}

QString ProfileService::ensureClientId() const
{
    QString rawId;
#ifdef Q_OS_WIN
    QSettings registry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography", QSettings::NativeFormat);
    rawId = registry.value("MachineGuid").toString().trimmed();
#endif
    if (rawId.isEmpty()) {
        rawId = QString::fromUtf8(QSysInfo::machineUniqueId()).trimmed();
    }
    if (rawId.isEmpty()) {
        rawId = QSysInfo::machineHostName().trimmed();
    }
    const QByteArray normalized = rawId.trimmed().toLower().toUtf8();
    const QByteArray digest = QCryptographicHash::hash(QByteArray("frp-control-client:") + normalized, QCryptographicHash::Sha256).toHex();
    return "hwid-" + QString::fromLatin1(digest.left(32));
}

QString ProfileService::defaultFrpcPath() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("frpc.exe");
}

QString ProfileService::legacyProfilePath() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath("frp-control-client.cfg.json");
}
