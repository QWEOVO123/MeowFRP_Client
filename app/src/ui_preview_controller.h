#pragma once
#include <QQmlPropertyMap>
#include <QGuiApplication>
#include <QClipboard>

// Design/QA only: no profile reads/writes, HTTP requests, heartbeat or frpc.
class UiPreviewController : public QQmlPropertyMap {
    Q_OBJECT
public:
    explicit UiPreviewController(const QString &page) : QQmlPropertyMap(this, nullptr) {
        insert("previewMode", true); insert("previewPage", page);
        insert("connected", page != "login" && page != "nodes"); insert("busy", false);
        insert("frpcRunning", true); insert("quitInProgress", false); insert("allowClose", true);
        insert("apiBaseUrl", "https://center.example.com/api"); insert("clientId", "DEMO-DEVICE-7A91");
        insert("frpcPath", "frpc.exe"); insert("runtimeDir", "runtime"); insert("userName", "demo_alice");
        insert("tokenName", "个人访问令牌"); insert("frpEndpoint", "203.0.113.10:7000");
        insert("portStart", 10000); insert("portEnd", 30000); insert("maxPorts", 5); insert("tunnelLimit", 5);
        insert("allowedProtocols", QVariantList{"tcp", "udp"}); insert("allowedProtocolsText", "TCP / UDP");
        insert("dpiEnabled", true); insert("dpiMode", "monitor"); insert("dpiBlockedText", "无"); insert("dpiDetectorsText", "HTTP · TLS · QUIC");
        insert("debugMode", false); insert("debugLogPath", "演示模式，不写入日志文件"); insert("errorMessage", "");
        insert("statusMessage", page == "login" ? "准备就绪，登录后选择有权访问的节点" : "已连接上海节点 · 运行状态请以 frpc 日志为准");
        insert("logText", "[演示] 中心鉴权成功，已获取授权节点列表。\n[演示] 上海节点登录成功，资源策略已下发。\n[演示] frpc login to server success\n[演示] [ssh] start proxy success\n[演示] [minecraft] start proxy success\n[演示] 心跳正常 · 当前界面不连接任何服务器");
        QVariantList tunnels;
        for (int i=0; i<2; ++i) tunnels << QVariantMap{{"index",i},{"name",i==0?"ssh":"minecraft"},{"type","tcp"},{"local_ip","127.0.0.1"},{"local_port",i==0?22:25565},{"remote_endpoint",i==0?"203.0.113.10:10280":"203.0.113.10:25565"},{"status","frpc 运行中"}};
        insert("tunnelList", tunnels); insert("tunnelCount", tunnels.size());
        QVariantList nodes;
        for (int i=0; i<3; ++i) nodes << QVariantMap{{"index",i},{"tag",i==0?"上海 · 低延迟节点":i==1?"成都 · 边缘节点":"香港 · 备用节点"},{"node_type",i==0?"controller":"edge"},{"api_url",QString("https://edge%1.example.com/api").arg(i+1)},{"online",i!=2}};
        insert("nodeList", nodes);
    }
    Q_INVOKABLE void connectToServer(const QString & = {}) { emit nodeSelectionRequested(); }
    Q_INVOKABLE void selectNode(int) { insert("connected",true); emit stateChanged(); }
    Q_INVOKABLE void saveProfile() { insert("statusMessage","演示模式：不保存实际配置"); }
    Q_INVOKABLE void addTunnel(const QString &,const QString &,const QString &,int,int) { insert("statusMessage","演示模式：不创建实际隧道"); }
    Q_INVOKABLE void startTunnels() { insert("statusMessage","演示模式：不启动 frpc"); }
    Q_INVOKABLE void stopTunnel(int) { insert("statusMessage","演示模式：不操作 frpc"); }
    Q_INVOKABLE void removeTunnel(int) { insert("statusMessage","演示模式：不修改隧道"); }
    Q_INVOKABLE void stopAllTunnels() { insert("statusMessage","演示模式：不操作 frpc"); }
    Q_INVOKABLE void copyRemoteEndpoint(int i) { qGuiApp->clipboard()->setText(value("tunnelList").toList().value(i).toMap().value("remote_endpoint").toString()); }
    Q_INVOKABLE void copyLogs() { qGuiApp->clipboard()->setText(value("logText").toString()); }
    Q_INVOKABLE void clearLogs() { insert("logText", ""); }
    Q_INVOKABLE void quitClient() { qGuiApp->quit(); }
signals:
    void stateChanged();
    void remoteMessageRequested(const QString &message);
    void nodeSelectionRequested();
};
