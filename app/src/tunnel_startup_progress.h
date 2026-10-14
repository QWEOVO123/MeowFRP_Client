#pragma once

#include <QRegularExpression>
#include <QSet>
#include <QString>

// Progress is driven by FRPC events, never timers or process-start notifications.
// It holds no certificates or credentials. Steps: 2=trust, 3=login, 4=proxies, 5=ready.
class TunnelStartupProgress {
public:
    int step = 0;
    QString message;
    bool failed = false;
    bool tlsDisabled = false;

    void begin(int proxyCount, bool managedTrust) {
        expected = qMax(0, proxyCount);
        managed = managedTrust;
        started.clear();
        failed = false;
        tlsDisabled = false;
        step = managed ? 2 : 3;
        message = managed ? "正在获取节点的 TLS 信任根…" : "正在连接 FRP 服务端…";
    }

    void stop() { step = 0; message.clear(); failed = false; tlsDisabled = false; started.clear(); }

    bool observe(const QString &line) {
        if (step == 0) return false;
        const bool reconnect = line.contains("try to connect to server...");
        // Without TCP mux, normal visitor traffic creates new physical work
        // connections and fetches trust again. That is not a new startup.
        if (step == 5 && !reconnect) return false;
        if (step >= 4 && !reconnect && (line.contains("[MEOW_TLS]") || line.contains("HTTPS confirmed node FRP TLS state:"))) return false;
        if (reconnect) {
            begin(expected, managed);
        } else if (line.contains("[MEOW_TLS] trust-fetch begin")) {
            step = 2; failed = false; tlsDisabled = false;
            message = "正在通过 HTTPS 下载 TLS 信任根…";
        } else if (line.contains("[MEOW_TLS] trust-fetch complete") || line.contains("HTTPS confirmed node FRP TLS state:")) {
            tlsDisabled = line.contains("enabled=false");
            step = 3; failed = false;
            message = tlsDisabled ? "节点已关闭 TLS，正在登录 FRP…" : "信任根已获取，正在进行 TLS 握手和 FRP 登录…";
        } else if (line.contains("[MEOW_TLS] trust-fetch failed")) {
            step = 2; failed = true;
            message = "TLS 信任根获取或验证失败，等待重试；详情见日志";
        } else if (line.contains("connect to server error:") || line.contains("login to the server failed:")) {
            failed = true;
            message = "FRP 连接失败，详情见日志";
        } else if (line.contains("login to server success, get run id")) {
            failed = false;
            step = expected == 0 ? 5 : 4;
            message = expected == 0 ? "FRP 登录成功（没有待建立的隧道）" : QString("FRP 登录成功，正在建立隧道（0/%1）…").arg(expected);
        } else if (line.contains("start proxy error:")) {
            step = 4; failed = true;
            message = "部分隧道建立失败，详情见日志";
        } else {
            static const QRegularExpression proxy(QStringLiteral(R"(\[([^\[\]\r\n]+)\] start proxy success(?:\s|$))"));
            const auto match = proxy.match(line);
            if (!match.hasMatch()) return false;
            started.insert(match.captured(1));
            if (expected > 0 && started.size() >= expected) {
                step = 5; failed = false;
                message = QString("隧道已建立（%1/%1）").arg(expected);
            } else {
                step = 4;
                message = QString("正在建立隧道（%1/%2）…").arg(started.size()).arg(expected);
            }
        }
        return true;
    }

private:
    int expected = 0;
    bool managed = false;
    QSet<QString> started;
};
