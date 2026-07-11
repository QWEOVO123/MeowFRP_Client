pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    visible: true
    width: 560
    height: 600
    minimumWidth: 520
    minimumHeight: 560
    title: "FRP 控制客户端"
    color: "#f5f7fb"

    property int pageIndex: 0
    property color ink: "#172033"
    property color muted: "#667085"
    property color line: "#d7e0ea"
    property color panel: "#ffffff"
    property color primary: "#2f6f73"
    property color primarySoft: "#e7f1ef"
    property color coral: "#d66b58"
    property color honey: "#d59b35"
    property string remoteWarningMessage: ""

    onClosing: function(closeEvent) {
        if (appController.connected && !appController.allowClose) {
            closeEvent.accepted = false
            if (!appController.quitInProgress) {
                appController.quitClient()
            }
        }
    }

    Connections {
        target: appController
        function onStateChanged() {
            if (appController.connected && root.width < 900) {
                root.width = 980
                root.height = 660
                root.minimumWidth = 860
                root.minimumHeight = 600
            }
        }
        function onRemoteMessageRequested(message) {
            root.remoteWarningMessage = message
            remoteWarningDialog.open()
        }
    }

    component AppButton: Button {
        id: control
        property bool emphasized: false
        property bool destructive: false
        implicitHeight: 40
        padding: 0
        font.pixelSize: 14
        font.weight: Font.DemiBold
        contentItem: Text {
            text: control.text
            color: !control.enabled ? "#98a2b3" : control.emphasized || control.destructive ? "#ffffff" : root.ink
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font: control.font
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 8
            color: !control.enabled ? "#edf1f6" : control.destructive ? root.coral : control.emphasized ? root.primary : "#ffffff"
            border.width: control.emphasized || control.destructive ? 0 : 1
            border.color: root.line
        }
    }

    component AppField: TextField {
        id: field
        implicitHeight: 40
        selectByMouse: true
        color: root.ink
        placeholderTextColor: "#98a2b3"
        font.pixelSize: 14
        leftPadding: 12
        rightPadding: 12
        background: Rectangle {
            radius: 8
            color: "#fbfcfe"
            border.width: 1
            border.color: field.activeFocus ? root.primary : root.line
        }
    }

    component Pill: Rectangle {
        id: pill
        property string text: ""
        property color fill: root.primarySoft
        property color fg: root.primary
        radius: 999
        height: 28
        width: Math.max(70, label.implicitWidth + 22)
        color: fill
        Text {
            id: label
            anchors.centerIn: parent
            text: pill.text
            color: pill.fg
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }

    component StatCard: Rectangle {
        id: card
        property string title: ""
        property string value: ""
        property color accent: root.primary
        radius: 8
        color: root.panel
        border.width: 1
        border.color: root.line
        Text {
            id: statTitle
            x: 14
            y: 13
            width: parent.width - 28
            text: card.title
            color: root.muted
            font.pixelSize: 12
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            x: 14
            y: 42
            width: parent.width - 28
            text: card.value
            color: card.accent
            font.pixelSize: 20
            font.weight: Font.Bold
            elide: Text.ElideRight
        }
    }

    component NavButton: Button {
        id: nav
        property bool selected: false
        implicitHeight: 42
        padding: 0
        contentItem: Text {
            text: nav.text
            color: nav.selected ? "#ffffff" : root.ink
            verticalAlignment: Text.AlignVCenter
            leftPadding: 14
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
        background: Rectangle {
            radius: 8
            color: nav.selected ? root.primary : "#ffffff"
            border.width: nav.selected ? 0 : 1
            border.color: root.line
        }
    }

    Loader {
        anchors.fill: parent
        sourceComponent: appController.connected ? dashboardPage : connectPage
    }

    Dialog {
        id: remoteWarningDialog
        title: "服务端提醒"
        modal: true
        standardButtons: Dialog.Ok
        anchors.centerIn: parent
        width: Math.min(root.width - 64, 420)
        contentItem: Text {
            text: root.remoteWarningMessage
            color: root.ink
            font.pixelSize: 15
            wrapMode: Text.WordWrap
            width: remoteWarningDialog.width - 48
        }
    }

    Component {
        id: connectPage
        Item {
            anchors.fill: parent

            Rectangle {
                width: Math.min(parent.width - 48, 480)
                height: 510
                anchors.centerIn: parent
                radius: 10
                color: root.panel
                border.width: 1
                border.color: root.line

                Text {
                    x: 26
                    y: 26
                    width: parent.width - 52
                    text: "FRP 控制客户端"
                    color: root.ink
                    font.pixelSize: 28
                    font.weight: Font.Bold
                }
                Text {
                    x: 26
                    y: 68
                    width: parent.width - 52
                    text: "填写连接信息后获取服务端授权"
                    color: root.muted
                    font.pixelSize: 14
                }

                Text { x: 26; y: 120; text: "设备 HWID"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                AppField {
                    id: clientIdInput
                    x: 26
                    y: 142
                    width: parent.width - 52
                    text: appController.clientId
                    readOnly: true
                    placeholderText: "自动从设备标识生成"
                }

                Text { x: 26; y: 196; text: "API 地址"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                AppField {
                    id: apiInput
                    x: 26
                    y: 218
                    width: parent.width - 52
                    text: appController.apiBaseUrl
                    placeholderText: "https://relay.qweovo.top"
                    onTextEdited: appController.apiBaseUrl = text
                    onEditingFinished: appController.apiBaseUrl = text
                }

                Text { x: 26; y: 272; text: "用户 Token"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                AppField {
                    id: tokenInput
                    x: 26
                    y: 294
                    width: parent.width - 52
                    text: appController.accessToken
                    placeholderText: "ak_..."
                    echoMode: TextInput.PasswordEchoOnEdit
                    onTextEdited: appController.accessToken = text
                    onEditingFinished: appController.accessToken = text
                }

                Rectangle {
                    x: 26
                    y: 356
                    width: parent.width - 52
                    height: 58
                    radius: 8
                    color: appController.errorMessage.length > 0 ? "#fff2ef" : root.primarySoft
                    border.width: 1
                    border.color: appController.errorMessage.length > 0 ? "#f2b4a9" : "#c6dedb"
                    Text {
                        anchors.fill: parent
                        anchors.margins: 12
                        text: appController.statusMessage
                        color: appController.errorMessage.length > 0 ? "#a64232" : root.primary
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                AppButton {
                    x: 26
                    y: 438
                    width: parent.width - 158
                    text: appController.busy ? "连接中..." : "连接服务器"
                    emphasized: true
                    enabled: !appController.busy
                    onClicked: {
                        appController.apiBaseUrl = apiInput.text
                        appController.accessToken = tokenInput.text
                        appController.connectToServer()
                    }
                }
                AppButton {
                    x: parent.width - 116
                    y: 438
                    width: 90
                    text: "保存"
                    enabled: !appController.busy
                    onClicked: appController.saveProfile()
                }
            }
        }
    }

    Component {
        id: dashboardPage
        Item {
            anchors.fill: parent

            Rectangle {
                id: side
                x: 16
                y: 16
                width: 214
                height: parent.height - 32
                radius: 10
                color: root.panel
                border.width: 1
                border.color: root.line

                Text {
                    x: 16
                    y: 18
                    width: parent.width - 32
                    text: "FRP Client"
                    color: root.ink
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                Text {
                    x: 16
                    y: 52
                    width: parent.width - 32
                    text: appController.clientId
                    color: root.muted
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }
                Pill {
                    x: 16
                    y: 84
                    text: appController.frpcRunning ? "frpc 运行中" : "控制面已连接"
                    fill: appController.frpcRunning ? root.primarySoft : "#fff5df"
                    fg: appController.frpcRunning ? root.primary : root.honey
                }

                Rectangle { x: 16; y: 132; width: parent.width - 32; height: 1; color: root.line }

                NavButton {
                    x: 16
                    y: 154
                    width: parent.width - 32
                    text: "隧道"
                    selected: root.pageIndex === 0
                    onClicked: root.pageIndex = 0
                }
                NavButton {
                    x: 16
                    y: 204
                    width: parent.width - 32
                    text: "日志"
                    selected: root.pageIndex === 1
                    onClicked: root.pageIndex = 1
                }
                NavButton {
                    x: 16
                    y: 254
                    width: parent.width - 32
                    text: "设置"
                    selected: root.pageIndex === 2
                    onClicked: root.pageIndex = 2
                }

                AppButton {
                    x: 16
                    y: parent.height - 108
                    width: parent.width - 32
                    text: "重新鉴权"
                    enabled: !appController.busy
                    onClicked: appController.connectToServer()
                }
                AppButton {
                    x: 16
                    y: parent.height - 58
                    width: parent.width - 32
                    text: appController.quitInProgress ? "正在退出..." : "退出客户端"
                    destructive: true
                    enabled: !appController.busy || appController.quitInProgress
                    onClicked: appController.quitClient()
                }
            }

            Rectangle {
                id: mainPanel
                x: side.x + side.width + 14
                y: 16
                width: parent.width - x - 16
                height: parent.height - 32
                radius: 10
                color: root.panel
                border.width: 1
                border.color: root.line

                Text {
                    id: pageTitle
                    x: 20
                    y: 17
                    width: parent.width - 40
                    text: root.pageIndex === 0 ? "隧道控制" : root.pageIndex === 1 ? "运行日志" : "客户端设置"
                    color: root.ink
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
                Text {
                    x: 20
                    y: 50
                    width: parent.width - 40
                    text: appController.statusMessage
                    color: appController.errorMessage.length > 0 ? root.coral : root.muted
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                Rectangle { x: 20; y: 82; width: parent.width - 40; height: 1; color: root.line }

                Loader {
                    x: 20
                    y: 100
                    width: parent.width - 40
                    height: parent.height - 120
                    sourceComponent: root.pageIndex === 0 ? tunnelPage : root.pageIndex === 1 ? logsPage : settingsPage
                }
            }
        }
    }

    Component {
        id: tunnelPage
        Flickable {
            id: flick
            clip: true
            contentWidth: width
            contentHeight: tunnelColumn.height

            Column {
                id: tunnelColumn
                width: flick.width
                spacing: 14

                Row {
                    width: parent.width
                    height: 88
                    spacing: 12
                    StatCard {
                        width: Math.max(140, (parent.width - 24) / 3)
                        height: parent.height
                        title: "当前用户"
                        value: appController.userName.length > 0 ? appController.userName : "-"
                    }
                    StatCard {
                        width: Math.max(160, (parent.width - 24) / 3)
                        height: parent.height
                        title: "frps 地址"
                        value: appController.frpEndpoint
                        accent: root.coral
                    }
                    StatCard {
                        width: Math.max(150, (parent.width - 24) / 3)
                        height: parent.height
                        title: "可用端口"
                        value: appController.portStart + " - " + appController.portEnd
                        accent: root.honey
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 122
                    radius: 8
                    color: "#fbfcfe"
                    border.width: 1
                    border.color: root.line
                    Text {
                        x: 16
                        y: 14
                        text: "服务端授权"
                        color: root.ink
                        font.pixelSize: 17
                        font.weight: Font.Bold
                    }
                    Pill {
                        x: parent.width - width - 16
                        y: 12
                        text: appController.dpiEnabled ? "DPI 已启用" : "DPI 未启用"
                        fill: appController.dpiEnabled ? "#fff2ef" : root.primarySoft
                        fg: appController.dpiEnabled ? root.coral : root.primary
                    }
                    Text {
                        x: 16
                        y: 52
                        width: parent.width - 32
                        text: "最多端口：" + appController.maxPorts + "    允许协议：" + appController.allowedProtocolsText
                        color: root.muted
                        font.pixelSize: 14
                        elide: Text.ElideRight
                    }
                    Text {
                        x: 16
                        y: 80
                        width: parent.width - 32
                        text: "DPI 模式：" + appController.dpiMode + "    拒绝流量：" + appController.dpiBlockedText + "    检测器：" + appController.dpiDetectorsText
                        color: root.muted
                        font.pixelSize: 13
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 304
                    radius: 8
                    color: "#fbfcfe"
                    border.width: 1
                    border.color: root.line

                    Text {
                        x: 16
                        y: 14
                        text: "添加隧道"
                        color: root.ink
                        font.pixelSize: 17
                        font.weight: Font.Bold
                    }

                    Text { x: 16; y: 58; text: "隧道名称"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                    AppField { id: proxyName; x: 112; y: 48; width: parent.width - 128; text: "ssh" }

                    Text { x: 16; y: 108; text: "穿透协议"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                    ComboBox {
                        id: protocol
                        x: 112
                        y: 98
                        width: parent.width - 128
                        height: 40
                        model: appController.allowedProtocols.length > 0 ? appController.allowedProtocols : ["tcp", "udp"]
                        font.pixelSize: 14
                        background: Rectangle { radius: 8; color: "#ffffff"; border.width: 1; border.color: root.line }
                    }

                    Text { x: 16; y: 158; text: "本地地址"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                    AppField { id: localIp; x: 112; y: 148; width: parent.width - 128; text: "127.0.0.1" }

                    Text { x: 16; y: 208; text: "本地端口"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                    SpinBox { id: localPort; x: 112; y: 198; width: 150; height: 40; from: 1; to: 65535; value: 22; editable: true }

                    Text { x: 286; y: 208; text: "服务端端口"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                    SpinBox {
                        id: remotePort
                        x: 382
                        y: 198
                        width: 150
                        height: 40
                        from: appController.portStart > 0 ? appController.portStart : 1
                        to: appController.portEnd > 0 ? appController.portEnd : 65535
                        value: appController.portStart > 0 ? appController.portStart : 6001
                        editable: true
                    }

                    AppButton {
                        x: 112
                        y: 256
                        width: Math.min(220, parent.width - 300)
                        text: appController.busy ? "处理中..." : "加入列表"
                        emphasized: true
                        enabled: !appController.busy && appController.tunnelCount < appController.tunnelLimit
                        onClicked: appController.addTunnel(proxyName.text, protocol.currentText, localIp.text, localPort.value, remotePort.value)
                    }
                    AppButton {
                        x: Math.min(parent.width - 170, 350)
                        y: 256
                        width: 150
                        text: appController.frpcRunning ? "应用并重启" : "启动列表"
                        enabled: !appController.busy && appController.tunnelCount > 0
                        onClicked: appController.startTunnels()
                    }
                }

                Rectangle {
                    width: parent.width
                    height: Math.max(190, 82 + Math.max(1, appController.tunnelCount) * 52)
                    radius: 8
                    color: "#fbfcfe"
                    border.width: 1
                    border.color: root.line

                    Text {
                        x: 16
                        y: 14
                        text: "隧道列表"
                        color: root.ink
                        font.pixelSize: 17
                        font.weight: Font.Bold
                    }
                    Text {
                        x: 112
                        y: 18
                        width: parent.width - 300
                        text: appController.tunnelCount + " / " + appController.tunnelLimit
                        color: root.muted
                        font.pixelSize: 13
                    }
                    AppButton {
                        x: parent.width - 288
                        y: 10
                        width: 140
                        text: appController.frpcRunning ? "重启全部" : "启动全部"
                        emphasized: true
                        enabled: !appController.busy && appController.tunnelCount > 0
                        onClicked: appController.startTunnels()
                    }
                    AppButton {
                        x: parent.width - 136
                        y: 10
                        width: 120
                        text: "停止全部"
                        destructive: true
                        enabled: appController.frpcRunning
                        onClicked: appController.stopAllTunnels()
                    }

                    Rectangle { x: 16; y: 58; width: parent.width - 32; height: 1; color: root.line }

                    Text {
                        anchors.centerIn: parent
                        visible: appController.tunnelCount === 0
                        text: "还没有隧道，先在上面添加一个。"
                        color: root.muted
                        font.pixelSize: 14
                    }

                    Repeater {
                        model: appController.tunnelList
                        delegate: Rectangle {
                            required property var modelData
                            x: 16
                            y: 70 + modelData.index * 52
                            width: parent.width - 32
                            height: 42
                            radius: 7
                            color: "#ffffff"
                            border.width: 1
                            border.color: root.line

                            Text {
                                x: 12
                                y: 7
                                width: 150
                                text: modelData.name + " · " + modelData.type
                                color: root.ink
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text {
                                x: 172
                                y: 7
                                width: Math.max(120, parent.width - 520)
                                text: modelData.local_ip + ":" + modelData.local_port + "  ->  " + modelData.remote_endpoint
                                color: root.muted
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                            Pill {
                                x: parent.width - 286
                                y: 7
                                text: modelData.status
                                fill: appController.frpcRunning ? root.primarySoft : "#fff5df"
                                fg: appController.frpcRunning ? root.primary : root.honey
                            }
                            AppButton {
                                x: parent.width - 194
                                y: 5
                                width: 78
                                height: 32
                                text: "复制"
                                enabled: !appController.busy
                                onClicked: appController.copyRemoteEndpoint(modelData.index)
                            }
                            AppButton {
                                x: parent.width - 102
                                y: 5
                                width: 88
                                height: 32
                                text: appController.frpcRunning ? "关闭" : "删除"
                                destructive: appController.frpcRunning
                                enabled: !appController.busy
                                onClicked: {
                                    if (appController.frpcRunning)
                                        appController.stopTunnel(modelData.index)
                                    else
                                        appController.removeTunnel(modelData.index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: logsPage
        Item {
            anchors.fill: parent
            AppButton {
                id: clearButton
                x: parent.width - 92
                y: 0
                width: 92
                text: "清空"
                onClicked: appController.clearLogs()
            }
            ScrollView {
                x: 0
                y: 52
                width: parent.width
                height: parent.height - 52
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                background: Rectangle {
                    radius: 8
                    color: "#132225"
                    border.width: 1
                    border.color: "#294145"
                }
                TextArea {
                    width: parent.width
                    readOnly: true
                    text: appController.logText
                    wrapMode: TextEdit.Wrap
                    color: "#d9f3ef"
                    font.family: "Consolas"
                    font.pixelSize: 13
                    selectByMouse: true
                    padding: 12
                    background: Rectangle {
                        color: "transparent"
                    }
                }
            }
        }
    }

    Component {
        id: settingsPage
        Flickable {
            id: settingsFlick
            clip: true
            contentWidth: width
            contentHeight: settingsBox.height

            Rectangle {
                id: settingsBox
                width: settingsFlick.width
                height: 390
                radius: 8
                color: "#fbfcfe"
                border.width: 1
                border.color: root.line

                Text { x: 16; y: 24; text: "API 地址"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                AppField {
                    x: 112
                    y: 14
                    width: parent.width - 128
                    text: appController.apiBaseUrl
                    onTextEdited: appController.apiBaseUrl = text
                    onEditingFinished: appController.apiBaseUrl = text
                }

                Text { x: 16; y: 78; text: "用户 Token"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                AppField {
                    x: 112
                    y: 68
                    width: parent.width - 128
                    text: appController.accessToken
                    echoMode: TextInput.PasswordEchoOnEdit
                    onTextEdited: appController.accessToken = text
                    onEditingFinished: appController.accessToken = text
                }

                Text { x: 16; y: 132; text: "设备 HWID"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                AppField {
                    x: 112
                    y: 122
                    width: parent.width - 128
                    text: appController.clientId
                    readOnly: true
                }

                Text { x: 16; y: 186; text: "frpc 程序"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                AppField {
                    x: 112
                    y: 176
                    width: parent.width - 128
                    text: appController.frpcPath
                    onTextEdited: appController.frpcPath = text
                    onEditingFinished: appController.frpcPath = text
                }

                Text { x: 16; y: 240; text: "运行目录"; color: root.ink; font.pixelSize: 13; font.weight: Font.DemiBold }
                AppField {
                    x: 112
                    y: 230
                    width: parent.width - 128
                    text: appController.runtimeDir
                    onTextEdited: appController.runtimeDir = text
                    onEditingFinished: appController.runtimeDir = text
                }

                AppButton {
                    x: 112
                    y: 308
                    width: 180
                    text: "保存设置"
                    emphasized: true
                    onClicked: appController.saveProfile()
                }
                AppButton {
                    x: 304
                    y: 308
                    width: 130
                    text: "重新连接"
                    enabled: !appController.busy
                    onClicked: appController.connectToServer()
                }
            }
        }
    }
}
