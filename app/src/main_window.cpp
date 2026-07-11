#include "main_window.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QStatusBar>
#include <QUuid>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_autoConnectTimer.setSingleShot(true);
    m_autoConnectTimer.setInterval(900);
    connect(&m_autoConnectTimer, &QTimer::timeout, this, &MainWindow::queryPolicy);

    buildUi();
    loadProfileToUi();

    connect(&m_api, &ControlApiClient::resourcePolicyLoaded, this, &MainWindow::handlePolicy);
    connect(&m_api, &ControlApiClient::bootstrapLoaded, this, &MainWindow::handleBootstrap);
    connect(&m_api, &ControlApiClient::requestFailed, this, &MainWindow::showError);
    connect(&m_runtime, &TunnelRuntimeService::logLine, this, &MainWindow::appendLog);
    connect(&m_runtime, &TunnelRuntimeService::failed, this, &MainWindow::showError);
    connect(&m_runtime, &TunnelRuntimeService::statusChanged, this, [this](const QString &status) {
        statusBar()->showMessage(status);
        appendLog(status);
    });

    if (!m_profile.apiBaseUrl.isEmpty() && !m_profile.accessToken.isEmpty()) {
        m_autoConnectTimer.start(300);
    }
}

void MainWindow::buildUi()
{
    setWindowTitle("FRP 控制客户端");
    resize(540, 360);
    setMinimumSize(460, 320);
    setStyleSheet(R"(
        QMainWindow, QWidget {
            background: #f5f7fb;
            color: #1f2937;
            font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
            font-size: 14px;
        }
        QGroupBox {
            background: #ffffff;
            border: 1px solid #dde4ef;
            border-radius: 8px;
            margin-top: 14px;
            padding: 16px 14px 12px 14px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 14px;
            padding: 0 8px;
            color: #2563eb;
        }
        QLineEdit, QSpinBox, QComboBox, QPlainTextEdit {
            background: #fbfdff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 7px 9px;
            selection-background-color: #93c5fd;
        }
        QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
            border-color: #3b82f6;
        }
        QPushButton {
            background: #ffffff;
            border: 1px solid #cbd5e1;
            border-radius: 6px;
            padding: 8px 14px;
            min-height: 22px;
        }
        QPushButton:hover {
            background: #eff6ff;
            border-color: #60a5fa;
        }
        QPushButton:disabled {
            color: #94a3b8;
            background: #f1f5f9;
        }
        QPushButton#primaryButton {
            background: #2563eb;
            border-color: #2563eb;
            color: white;
            font-weight: 600;
        }
        QPushButton#primaryButton:hover {
            background: #1d4ed8;
        }
        QLabel#summaryLabel {
            background: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 6px;
            padding: 10px;
        }
        QLabel#heroTitle {
            color: #0f172a;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#heroSub {
            color: #64748b;
        }
        QScrollArea {
            border: 0;
            background: transparent;
        }
        QStatusBar {
            background: #ffffff;
            border-top: 1px solid #e2e8f0;
        }
    )");

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(18, 14, 18, 14);
    root->setSpacing(12);

    m_stack = new QStackedWidget(central);
    m_stack->addWidget(buildConnectPage(m_stack));
    m_stack->addWidget(buildTunnelPage(m_stack));
    root->addWidget(m_stack, 1);

    setCentralWidget(central);
    m_stack->setCurrentIndex(0);
    m_bootstrapButton->setEnabled(false);
}

QWidget *MainWindow::buildConnectPage(QWidget *parent)
{
    auto *page = new QWidget(parent);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(2, 0, 2, 0);
    root->setSpacing(12);

    auto *title = new QLabel("连接 FRP 控制服务器", page);
    title->setObjectName("heroTitle");
    auto *sub = new QLabel("首次启动会自动生成 Client ID。填写 API 地址和用户 Token，鉴权通过后再创建隧道。", page);
    sub->setObjectName("heroSub");
    sub->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(sub);

    auto *connectBox = new QGroupBox("连接信息", page);
    auto *connectLayout = new QFormLayout(connectBox);
    connectLayout->setLabelAlignment(Qt::AlignRight);
    m_clientIdEdit = new QLineEdit(connectBox);
    m_apiUrlEdit = new QLineEdit(connectBox);
    m_tokenEdit = new QLineEdit(connectBox);
    m_tokenEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_clientIdEdit->setPlaceholderText("首次启动自动生成，可手动修改");
    m_apiUrlEdit->setPlaceholderText("例如：http://127.0.0.1:8080，或 127.0.0.1:8080");
    m_tokenEdit->setPlaceholderText("粘贴后台用户详情里的 ak_... HTTPS API Token");
    connectLayout->addRow("Client ID", m_clientIdEdit);
    connectLayout->addRow("API 地址", m_apiUrlEdit);
    connectLayout->addRow("用户 Token", m_tokenEdit);

    auto *connectButtons = new QWidget(connectBox);
    auto *connectButtonLayout = new QHBoxLayout(connectButtons);
    connectButtonLayout->setContentsMargins(0, 0, 0, 0);
    m_queryButton = new QPushButton("连接并获取权限", connectButtons);
    m_queryButton->setObjectName("primaryButton");
    auto *saveButton = new QPushButton("保存配置", connectButtons);
    connectButtonLayout->addWidget(m_queryButton);
    connectButtonLayout->addWidget(saveButton);
    connectButtonLayout->addStretch();
    connectLayout->addRow("", connectButtons);
    root->addWidget(connectBox);

    m_connectionHint = new QLabel("等待填写连接信息。", page);
    m_connectionHint->setObjectName("summaryLabel");
    m_connectionHint->setWordWrap(true);
    root->addWidget(m_connectionHint);
    root->addStretch();

    connect(saveButton, &QPushButton::clicked, this, &MainWindow::saveProfile);
    connect(m_queryButton, &QPushButton::clicked, this, &MainWindow::queryPolicy);
    connect(m_apiUrlEdit, &QLineEdit::textChanged, this, &MainWindow::scheduleAutoConnect);
    connect(m_tokenEdit, &QLineEdit::textChanged, this, &MainWindow::scheduleAutoConnect);
    connect(m_clientIdEdit, &QLineEdit::textChanged, this, &MainWindow::clearLoadedPolicy);
    return page;
}

QWidget *MainWindow::buildTunnelPage(QWidget *parent)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *page = new QWidget(scroll);
    auto *root = new QVBoxLayout(page);
    root->setContentsMargins(2, 0, 2, 0);
    root->setSpacing(10);

    auto *topBar = new QWidget(page);
    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel("创建内网穿透隧道", topBar);
    title->setObjectName("heroTitle");
    m_backButton = new QPushButton("返回连接设置", topBar);
    topLayout->addWidget(title, 1);
    topLayout->addWidget(m_backButton);
    root->addWidget(topBar);

    auto *policyBox = new QGroupBox("服务端授权", page);
    auto *policyLayout = new QVBoxLayout(policyBox);
    m_policyLabel = new QLabel("填写 API 地址和用户 Token 后会自动连接服务器。", policyBox);
    m_dpiLabel = new QLabel("DPI 状态将在连接成功后显示。", policyBox);
    m_policyLabel->setObjectName("summaryLabel");
    m_dpiLabel->setObjectName("summaryLabel");
    m_policyLabel->setWordWrap(true);
    m_dpiLabel->setWordWrap(true);
    policyLayout->addWidget(m_policyLabel);
    policyLayout->addWidget(m_dpiLabel);
    root->addWidget(policyBox);

    auto *tunnelBox = new QGroupBox("本地隧道", page);
    auto *tunnelLayout = new QFormLayout(tunnelBox);
    tunnelLayout->setLabelAlignment(Qt::AlignRight);
    m_proxyNameEdit = new QLineEdit("ssh", tunnelBox);
    m_protocolCombo = new QComboBox(tunnelBox);
    m_protocolCombo->addItems({"tcp", "udp"});
    m_localIpEdit = new QLineEdit("127.0.0.1", tunnelBox);
    m_localPortSpin = new QSpinBox(tunnelBox);
    m_localPortSpin->setRange(1, 65535);
    m_localPortSpin->setValue(22);
    m_remotePortSpin = new QSpinBox(tunnelBox);
    m_remotePortSpin->setRange(1, 65535);
    m_remotePortSpin->setValue(6001);
    tunnelLayout->addRow("隧道名称", m_proxyNameEdit);
    tunnelLayout->addRow("穿透协议", m_protocolCombo);
    tunnelLayout->addRow("本地地址", m_localIpEdit);
    tunnelLayout->addRow("本地端口", m_localPortSpin);
    tunnelLayout->addRow("服务端端口", m_remotePortSpin);

    auto *runtimeButtons = new QWidget(tunnelBox);
    auto *runtimeButtonLayout = new QHBoxLayout(runtimeButtons);
    runtimeButtonLayout->setContentsMargins(0, 0, 0, 0);
    m_bootstrapButton = new QPushButton("创建隧道并启动 FRP", runtimeButtons);
    m_bootstrapButton->setObjectName("primaryButton");
    m_stopButton = new QPushButton("停止 FRP", runtimeButtons);
    runtimeButtonLayout->addWidget(m_bootstrapButton);
    runtimeButtonLayout->addWidget(m_stopButton);
    runtimeButtonLayout->addStretch();
    tunnelLayout->addRow("", runtimeButtons);
    root->addWidget(tunnelBox);

    auto *advancedBox = new QGroupBox("高级设置", page);
    auto *advancedLayout = new QFormLayout(advancedBox);
    advancedLayout->setLabelAlignment(Qt::AlignRight);
    m_frpcPathEdit = new QLineEdit(advancedBox);
    m_runtimeDirEdit = new QLineEdit(advancedBox);

    auto *frpcRow = new QWidget(advancedBox);
    auto *frpcLayout = new QHBoxLayout(frpcRow);
    frpcLayout->setContentsMargins(0, 0, 0, 0);
    frpcLayout->addWidget(m_frpcPathEdit, 1);
    auto *browseFrpc = new QPushButton("选择", frpcRow);
    frpcLayout->addWidget(browseFrpc);
    advancedLayout->addRow("frpc 程序", frpcRow);

    auto *runtimeRow = new QWidget(advancedBox);
    auto *runtimeLayout = new QHBoxLayout(runtimeRow);
    runtimeLayout->setContentsMargins(0, 0, 0, 0);
    runtimeLayout->addWidget(m_runtimeDirEdit, 1);
    auto *browseRuntime = new QPushButton("选择", runtimeRow);
    runtimeLayout->addWidget(browseRuntime);
    advancedLayout->addRow("运行目录", runtimeRow);
    root->addWidget(advancedBox);

    auto *logBox = new QGroupBox("运行日志", page);
    auto *logLayout = new QVBoxLayout(logBox);
    m_logEdit = new QPlainTextEdit(logBox);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(1000);
    logLayout->addWidget(m_logEdit);
    root->addWidget(logBox, 1);

    connect(m_bootstrapButton, &QPushButton::clicked, this, &MainWindow::requestBootstrap);
    connect(m_stopButton, &QPushButton::clicked, &m_runtime, &TunnelRuntimeService::stop);
    connect(m_backButton, &QPushButton::clicked, this, [this]() {
        if (m_stack) {
            m_stack->setCurrentIndex(0);
            resize(540, 360);
        }
    });
    connect(browseFrpc, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, "选择 frpc 程序", m_frpcPathEdit->text(), "frpc (frpc.exe frpc);;所有文件 (*)");
        if (!path.isEmpty()) {
            m_frpcPathEdit->setText(path);
        }
    });
    connect(browseRuntime, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(this, "选择运行目录", m_runtimeDirEdit->text());
        if (!path.isEmpty()) {
            m_runtimeDirEdit->setText(path);
        }
    });
    scroll->setWidget(page);
    return scroll;
}

void MainWindow::loadProfileToUi()
{
    m_profile = m_profiles.load();
    m_apiUrlEdit->setText(m_profile.apiBaseUrl);
    m_tokenEdit->setText(m_profile.accessToken);
    m_clientIdEdit->setText(m_profile.clientId);
    m_frpcPathEdit->setText(m_profile.frpcPath);
    m_runtimeDirEdit->setText(m_profile.runtimeDir);
    statusBar()->showMessage("配置文件：" + m_profiles.profilePath());
}

void MainWindow::saveProfile()
{
    m_profile = profileFromUi();
    QString error;
    if (!m_profiles.save(m_profile, &error)) {
        showError(error);
        return;
    }
    if (m_connectionHint && m_stack && m_stack->currentIndex() == 0) {
        m_connectionHint->setText("配置已保存。");
    }
    statusBar()->showMessage("配置已保存。");
}

void MainWindow::queryPolicy()
{
    m_autoConnectTimer.stop();
    m_profile = profileFromUi();
    if (m_profile.clientId.isEmpty()) {
        m_profile.clientId = "qt-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_clientIdEdit->setText(m_profile.clientId);
    }
    if (m_profile.apiBaseUrl.isEmpty() || m_profile.accessToken.isEmpty()) {
        clearLoadedPolicy();
        const QString message = "请先填写 API 地址和用户 Token。";
        if (m_connectionHint) {
            m_connectionHint->setText(message);
        }
        statusBar()->showMessage(message);
        return;
    }
    QString error;
    if (!m_profiles.save(m_profile, &error)) {
        showError(error);
        return;
    }
    m_queryButton->setEnabled(false);
    m_queryButton->setText("连接中...");
    if (m_connectionHint) {
        m_connectionHint->setText("正在连接服务器并获取授权，请稍等。");
    }
    statusBar()->showMessage("正在连接服务器...");
    appendLog("正在连接服务器并获取权限...");
    m_api.queryResourcePolicy(m_profile.apiBaseUrl, m_profile.accessToken, m_profile.clientId);
}

void MainWindow::requestBootstrap()
{
    saveProfile();
    QString error;
    const TunnelDraft draft = tunnelFromUi();
    if (!validateTunnel(draft, &error)) {
        showError(error);
        return;
    }
    appendLog("正在创建本地隧道并请求服务端下发配置...");
    m_api.bootstrap(m_profile.apiBaseUrl, m_profile.accessToken, m_profile.clientId, QList<TunnelDraft>{draft});
}

void MainWindow::scheduleAutoConnect()
{
    clearLoadedPolicy();
    if (!m_apiUrlEdit->text().trimmed().isEmpty() && !m_tokenEdit->text().trimmed().isEmpty()) {
        m_autoConnectTimer.start();
    }
}

void MainWindow::clearLoadedPolicy()
{
    m_hasPolicy = false;
    if (m_queryButton) {
        m_queryButton->setEnabled(true);
        m_queryButton->setText("连接并获取权限");
    }
    if (m_bootstrapButton) {
        m_bootstrapButton->setEnabled(false);
    }
    if (m_connectionHint) {
        m_connectionHint->setText("等待服务器鉴权。填写完整后会自动连接，也可以点击“连接并获取权限”。");
    }
    if (m_policyLabel) {
        m_policyLabel->setText("填写 API 地址和用户 Token 后会自动连接服务器。");
    }
    if (m_dpiLabel) {
        m_dpiLabel->setText("DPI 状态将在连接成功后显示。");
    }
}

void MainWindow::handlePolicy(const ResourcePolicyResponse &response)
{
    m_queryButton->setEnabled(true);
    m_queryButton->setText("连接并获取权限");
    m_policy = response;
    m_hasPolicy = true;
    m_bootstrapButton->setEnabled(true);

    if (m_policy.policy.portStart > 0 && m_policy.policy.portEnd >= m_policy.policy.portStart) {
        m_remotePortSpin->setRange(m_policy.policy.portStart, m_policy.policy.portEnd);
        m_remotePortSpin->setValue(m_policy.policy.portStart);
    }
    m_protocolCombo->clear();
    for (const auto &protocol : m_policy.policy.allowedProtocols) {
        if (protocol == "tcp" || protocol == "udp") {
            m_protocolCombo->addItem(protocol);
        }
    }
    if (m_protocolCombo->count() == 0) {
        m_protocolCombo->addItems({"tcp", "udp"});
        m_bootstrapButton->setEnabled(false);
        appendLog("服务端当前未允许 tcp/udp，客户端暂时不能创建端口隧道。");
    }

    updatePolicySummary();
    if (m_connectionHint) {
        m_connectionHint->setText("鉴权通过，已获取服务端授权。");
    }
    if (m_stack) {
        m_stack->setCurrentIndex(1);
        resize(720, 560);
    }
    statusBar()->showMessage("已获取服务端权限。");
}

void MainWindow::handleBootstrap(const BootstrapResponse &response)
{
    appendLog("服务端已下发配置，租约：" + response.leaseId);
    if (!response.expiresAt.isEmpty()) {
        appendLog("租约过期时间：" + response.expiresAt);
    }
    m_runtime.start(m_profile, response);
}

void MainWindow::appendLog(const QString &line)
{
    if (!line.trimmed().isEmpty() && m_logEdit) {
        m_logEdit->appendPlainText(line);
    }
}

void MainWindow::showError(const QString &message)
{
    const QString text = message.trimmed().isEmpty() ? "操作失败，请检查连接信息。" : message.trimmed();
    if (m_queryButton) {
        m_queryButton->setEnabled(true);
        m_queryButton->setText("连接并获取权限");
    }
    if (m_connectionHint && m_stack && m_stack->currentIndex() == 0) {
        m_connectionHint->setText("连接失败：" + text);
    }
    appendLog("错误：" + text);
    statusBar()->showMessage(text);
}

ClientProfile MainWindow::profileFromUi() const
{
    ClientProfile profile;
    profile.apiBaseUrl = m_apiUrlEdit->text().trimmed();
    profile.accessToken = m_tokenEdit->text().trimmed();
    profile.clientId = m_clientIdEdit->text().trimmed();
    profile.frpcPath = m_frpcPathEdit->text().trimmed();
    profile.runtimeDir = m_runtimeDirEdit->text().trimmed();
    return profile;
}

TunnelDraft MainWindow::tunnelFromUi() const
{
    TunnelDraft draft;
    draft.name = m_proxyNameEdit->text().trimmed();
    draft.type = m_protocolCombo->currentText();
    draft.localIp = m_localIpEdit->text().trimmed();
    draft.localPort = m_localPortSpin->value();
    draft.remotePort = m_remotePortSpin->value();
    return draft;
}

bool MainWindow::validateTunnel(const TunnelDraft &draft, QString *errorMessage) const
{
    if (!m_hasPolicy) {
        *errorMessage = "请先连接服务器获取权限。";
        return false;
    }
    if (draft.name.isEmpty() || draft.localIp.isEmpty()) {
        *errorMessage = "请填写隧道名称和本地地址。";
        return false;
    }
    if (!m_policy.policy.allowedProtocols.contains(draft.type)) {
        *errorMessage = "服务端未授权该穿透协议。";
        return false;
    }
    if (draft.remotePort < m_policy.policy.portStart || draft.remotePort > m_policy.policy.portEnd) {
        *errorMessage = "服务端端口不在授权范围内。";
        return false;
    }
    return true;
}

void MainWindow::updatePolicySummary()
{
    const auto &policy = m_policy.policy;
    m_policyLabel->setText(QString("用户：%1\nfrps：%2:%3\n可用服务端端口：%4 - %5，最多同时开放 %6 个\n允许协议：%7")
                               .arg(m_policy.user)
                               .arg(m_policy.frpServerAddr)
                               .arg(m_policy.frpServerPort)
                               .arg(policy.portStart)
                               .arg(policy.portEnd)
                               .arg(policy.maxPorts)
                               .arg(policy.allowedProtocols.join(", ")));

    const auto &dpi = m_policy.dpi;
    const QString blocked = dpi.blockedTrafficTypes.isEmpty() ? "无" : dpi.blockedTrafficTypes.join(", ");
    const QString detectors = dpi.enabledDetectors.isEmpty() ? "无" : dpi.enabledDetectors.join(", ");
    m_dpiLabel->setText(QString("DPI：%1\n模式：%2\n拒绝通过的流量：%3\n检测器：%4")
                            .arg(dpi.enabled ? "已启用" : "未启用")
                            .arg(dpi.mode)
                            .arg(blocked)
                            .arg(detectors));
}
