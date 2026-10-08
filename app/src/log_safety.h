#pragma once

#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

// All UI/file logs pass through this filter. Never log raw HTTP bodies or TOML.
namespace LogSafety {
inline QStringList &secrets()
{
    static QStringList values;
    return values;
}

inline void rememberSecret(const QString &value)
{
    if (value.size() >= 6 && !secrets().contains(value)) secrets().append(value);
}

inline QString url(const QUrl &value)
{
    QUrl safe = value;
    safe.setUserInfo({});
    safe.setQuery(QString());
    safe.setFragment({});
    return safe.toString();
}

inline QString redact(QString value)
{
    for (const auto &secret : secrets()) value.replace(secret, "[REDACTED]");
    static const QRegularExpression tokens(QStringLiteral(R"(\b(?:ak_|rt_)[A-Za-z0-9_-]+)"));
    value.replace(tokens, "[REDACTED]");
    static const QRegularExpression credentials(QStringLiteral(
        R"((\b(?:access_token|token|password|authorization|secret|private_key)\b["']?\s*[:=]\s*)(?:"[^"\r\n]*"|'[^'\r\n]*'|[^\s,;}]+))"),
        QRegularExpression::CaseInsensitiveOption);
    value.replace(credentials, "\\1[REDACTED]");
    static const QRegularExpression urls(QStringLiteral(R"(https?://[^\s"'<>]+)"));
    auto matches = urls.globalMatch(value);
    QList<QPair<QString, QString>> replacements;
    while (matches.hasNext()) {
        const auto raw = matches.next().captured();
        replacements.append({raw, url(QUrl(raw))});
    }
    for (const auto &entry : replacements) value.replace(entry.first, entry.second);
    // Keep untrusted server text on one record; prevent forged log lines.
    value.replace('\r', "\\r");
    value.replace('\n', "\\n");
    value.replace(QChar(0), "\\0");
    return value.left(16384);
}
}
