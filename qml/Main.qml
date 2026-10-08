pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: root
    objectName: "clientWindow"
    visible: true
    width: 1080; height: 740; minimumWidth: 880; minimumHeight: 640
    title: "MeowFRP" + (uiPreviewMode ? " · 界面预览" : "")
    color: appearance.backdropActive ? "transparent" : canvas
    font.family: "Microsoft YaHei UI"; font.pixelSize: 13
    property int pageIndex: uiPreviewPage === "logs" ? 1 : uiPreviewPage === "settings" ? 2 : 0
    readonly property bool dark: appearance.dark
    readonly property bool hc: appearance.highContrast
    readonly property color canvas: hc ? (dark ? "#000000" : "#ffffff") : dark ? "#101725" : "#edf3fc"
    readonly property color ink: dark ? "#edf3ff" : "#1b2c48"
    readonly property color muted: dark ? "#a8b8d0" : "#5d6d85"
    readonly property color line: hc ? ink : dark ? "#344258" : "#d7e2f1"
    readonly property color panel: hc ? canvas : dark ? "#1b2639" : "#ffffff"
    readonly property color fieldColor: hc ? canvas : dark ? "#121d2e" : "#f7faff"
    readonly property color hoverColor: dark ? "#2b3c57" : "#e9f1ff"
    readonly property color primary: dark ? "#80b5ff" : "#2668d8"
    readonly property color primaryFill: dark ? "#397bed" : "#2668d8"
    readonly property color primarySoft: dark ? "#253c60" : "#e8f0ff"
    readonly property color danger: dark ? "#ff9ca5" : "#ba3449"
    readonly property color dangerSoft: dark ? "#412937" : "#fff0f2"
    readonly property color success: dark ? "#7cddbe" : "#187d65"
    readonly property color successSoft: dark ? "#203c38" : "#e9f8f1"
    property string remoteWarningMessage: ""
    palette.window: panel; palette.windowText: ink; palette.base: fieldColor; palette.alternateBase: hoverColor
    palette.text: ink; palette.button: panel; palette.buttonText: ink
    palette.highlight: primaryFill; palette.highlightedText: "#ffffff"; palette.mid: line; palette.dark: line; palette.light: panel
    onClosing: function(event) {
        if (appController.connected && !appController.allowClose) {
            event.accepted = false
            if (!appController.quitInProgress) appController.quitClient()
        }
    }
    Component.onCompleted: if (uiPreviewMode && uiPreviewPage === "nodes") nodeSelectionDialog.open()
    Connections {
        target: appController
        function onRemoteMessageRequested(message) { root.remoteWarningMessage = message; remoteWarningDialog.open() }
        function onNodeSelectionRequested() { nodeSelectionDialog.open() }
    }
    // Native background is tinted, while content and text stay fully opaque.
    Rectangle { anchors.fill: parent; color: root.canvas; opacity: appearance.backdropActive ? (root.dark ? 0.76 : 0.65) : 1 }

    component LabelText: Text { color: root.ink; font.pixelSize: 13; wrapMode: Text.WordWrap; renderType: Text.QtRendering }
    component AppIcon: Text {
        property string glyph: "\uE8A5"
        text: glyph === "\uE8A5" ? "\uE71B" : glyph; color: root.primary; font.family: "Segoe MDL2 Assets"; font.pixelSize: 20
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        Layout.preferredWidth: 24; Layout.preferredHeight: 24; Accessible.ignored: true
    }
    component AppButton: Button {
        id: control
        property bool emphasized: false
        property bool destructive: false
        implicitHeight: 42; implicitWidth: Math.max(90, label.implicitWidth + 32)
        leftPadding: 16; rightPadding: 16; hoverEnabled: true; opacity: enabled ? 1 : 0.48
        Accessible.name: text
        contentItem: LabelText {
            id: label
            text: control.text
            color: control.emphasized ? "#ffffff" : control.destructive ? root.danger : root.ink
            font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            wrapMode: Text.NoWrap; elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 10
            color: control.emphasized ? (control.down ? Qt.darker(root.primaryFill, 1.14) : control.hovered ? Qt.lighter(root.primaryFill, 1.1) : root.primaryFill) : control.destructive ? root.dangerSoft : control.hovered || control.down ? root.hoverColor : root.panel
            border.color: control.activeFocus ? root.primary : root.line
            border.width: control.activeFocus ? 2 : control.emphasized ? 0 : 1
            Behavior on color { ColorAnimation { duration: root.hc ? 0 : 120 } }
        }
    }
    component AppField: TextField {
        id: control
        implicitHeight: 44; selectByMouse: true; color: root.ink
        selectionColor: root.primaryFill; selectedTextColor: "#ffffff"; placeholderTextColor: root.muted
        leftPadding: 14; rightPadding: 14
        background: Rectangle {
            radius: 10; color: root.fieldColor
            border.width: control.activeFocus ? 2 : 1; border.color: control.activeFocus ? root.primary : root.line
            Behavior on border.color { ColorAnimation { duration: root.hc ? 0 : 120 } }
        }
    }
    component AppSwitch: Switch {
        id: control
        spacing: 10; hoverEnabled: true; Accessible.name: text
        indicator: Rectangle {
            width: 42; height: 24; radius: 12; x: control.leftPadding; y: (control.height - height) / 2
            color: control.checked ? root.primaryFill : root.hoverColor
            border.color: control.activeFocus ? root.primary : root.line; border.width: control.activeFocus ? 2 : 1
            Rectangle {
                width: 18; height: 18; radius: 9; y: 3; x: control.checked ? 21 : 3
                color: control.checked ? "#ffffff" : root.muted
                Behavior on x { NumberAnimation { duration: root.hc ? 0 : 150; easing.type: Easing.OutCubic } }
            }
        }
        contentItem: LabelText { text: control.text; verticalAlignment: Text.AlignVCenter; leftPadding: control.indicator.width + control.spacing }
    }
    component AppCombo: ComboBox {
        id: control
        implicitHeight: 44; leftPadding: 14
        indicator: AppIcon { glyph: "\uE70D"; font.pixelSize: 12; width: 16; height: 16; x: control.width - 28; y: (control.height - height) / 2; color: root.muted }
        contentItem: LabelText { text: control.displayText; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight; rightPadding: 28 }
        background: Rectangle { radius: 10; color: root.fieldColor; border.color: control.activeFocus ? root.primary : root.line; border.width: control.activeFocus ? 2 : 1 }
        delegate: ItemDelegate { required property var modelData; required property int index; width: control.width; text: modelData; highlighted: control.highlightedIndex === index; palette.text: root.ink }
        popup: Popup {
            y: control.height + 5; width: control.width; implicitHeight: Math.min(240, contentItem.implicitHeight + 12); padding: 6
            background: Rectangle { radius: 10; color: root.panel; border.color: root.line }
            contentItem: ListView { clip: true; implicitHeight: contentHeight; model: control.popup.visible ? control.delegateModel : null; currentIndex: control.highlightedIndex; ScrollIndicator.vertical: ScrollIndicator {} }
        }
    }
    component AppSpin: SpinBox {
        id: spin
        implicitHeight: 44; implicitWidth: 150
        leftPadding: 30; rightPadding: 30
        editable: true
        textFromValue: function(value) { return value.toString() }
        valueFromText: function(text) { return Number(text) }
        background: Rectangle { radius: 10; color: root.fieldColor; border.color: spin.activeFocus ? root.primary : root.line; border.width: spin.activeFocus ? 2 : 1 }
        contentItem: TextInput {
            text: spin.displayText; color: root.ink; font: spin.font
            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            readOnly: !spin.editable; validator: spin.validator; inputMethodHints: Qt.ImhDigitsOnly
            selectByMouse: true; selectionColor: root.primaryFill; selectedTextColor: "#ffffff"; clip: true
        }
        down.indicator: Item {
            x: 0; height: spin.height; implicitWidth: 30
            LabelText { anchors.centerIn: parent; text: "−"; font.pixelSize: 20; color: spin.enabled && spin.value > spin.from ? root.muted : root.line }
        }
        up.indicator: Item {
            x: spin.width - width; height: spin.height; implicitWidth: 30
            LabelText { anchors.centerIn: parent; text: "+"; font.pixelSize: 20; color: spin.enabled && spin.value < spin.to ? root.muted : root.line }
        }
    }
    component Pill: Rectangle {
        id: pill
        property string text: ""
        property color fg: root.primary
        property color fill: root.primarySoft
        implicitWidth: pillLabel.implicitWidth + 22; implicitHeight: 28; radius: 14; color: fill
        LabelText { id: pillLabel; anchors.centerIn: parent; text: pill.text; color: pill.fg; font.pixelSize: 11; font.weight: Font.DemiBold }
    }
    component Surface: Rectangle { radius: 16; color: root.panel; border.color: root.line; border.width: 1 }
    component StatCard: Surface {
        id: stat
        property string label: ""
        property string value: ""
        implicitHeight: 100
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 18; spacing: 8
            LabelText { text: stat.label; color: root.muted; font.pixelSize: 12 }
            LabelText { text: stat.value; Layout.fillWidth: true; font.pixelSize: 19; font.weight: Font.DemiBold; wrapMode: Text.NoWrap; elide: Text.ElideRight }
        }
    }
    component NavButton: Button {
        id: nav
        property bool selected: false
        property string glyph: "\uE8A5"
        implicitHeight: 46; hoverEnabled: true
        contentItem: RowLayout {
            spacing: 12
            AppIcon { glyph: nav.glyph; color: nav.selected ? root.primary : root.muted }
            LabelText { text: nav.text; color: nav.selected ? root.primary : root.ink; font.weight: nav.selected ? Font.DemiBold : Font.Normal; Layout.fillWidth: true }
        }
        background: Rectangle {
            radius: 10; color: nav.selected ? root.primarySoft : nav.hovered ? root.hoverColor : "transparent"
            border.color: nav.activeFocus ? root.primary : "transparent"
            Rectangle { visible: nav.selected; width: 3; height: 20; radius: 2; color: root.primary; anchors.verticalCenter: parent.verticalCenter }
            Behavior on color { ColorAnimation { duration: root.hc ? 0 : 140 } }
        }
    }
    component ThemedDialog: Dialog {
        id: themedDialog
        modal: true; anchors.centerIn: parent; padding: 24
        background: Surface { radius: 20 }
        header: LabelText { text: themedDialog.title; padding: 24; bottomPadding: 6; font.pixelSize: 21; font.weight: Font.DemiBold }
        Overlay.modal: Rectangle { color: root.dark ? "#9a030916" : "#66192747" }
        enter: Transition { ParallelAnimation { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: root.hc ? 0 : 160 } NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: root.hc ? 0 : 180; easing.type: Easing.OutCubic } } }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: root.hc ? 0 : 100 } }
    }

    Loader { anchors.fill: parent; sourceComponent: appController.connected ? dashboardPage : connectPage }
    ThemedDialog {
        id: remoteWarningDialog
        title: "服务端提醒"; width: Math.min(root.width - 64, 500); standardButtons: Dialog.Ok
        contentItem: LabelText { text: root.remoteWarningMessage; font.pixelSize: 15; wrapMode: Text.WordWrap }
    }
    ThemedDialog {
        id: nodeSelectionDialog
        objectName: "nodeSelectionDialog"
        title: "选择你的节点"; closePolicy: Popup.CloseOnEscape
        width: Math.min(root.width - 72, 620); height: Math.min(root.height - 72, 500)
        contentItem: ColumnLayout {
            spacing: 18
            LabelText { Layout.fillWidth: true; text: "Token 验证成功。仅显示中心为你授权的节点，选择后自动登录。"; color: root.muted }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; contentWidth: availableWidth; clip: true
                Column {
                    width: parent.width; spacing: 10
                    Repeater {
                        model: appController.nodeList
                        delegate: Button {
                            id: nodeButton
                            required property var modelData
                            width: parent.width; height: 86; enabled: modelData.online && !appController.busy; hoverEnabled: true; opacity: enabled ? 1 : 0.5
                            Accessible.name: (modelData.tag || "未命名节点") + (modelData.online ? " 在线" : " 离线")
                            background: Rectangle { radius: 12; color: nodeButton.hovered ? root.primarySoft : root.fieldColor; border.color: nodeButton.activeFocus || nodeButton.hovered ? root.primary : root.line }
                            contentItem: RowLayout {
                                spacing: 16
                                AppIcon { glyph: "\uE968"; font.pixelSize: 24 }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 6
                                    LabelText { text: nodeButton.modelData.tag || "未命名节点"; font.pixelSize: 15; font.weight: Font.DemiBold }
                                    LabelText { text: nodeButton.modelData.api_url; color: root.muted; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.NoWrap; elide: Text.ElideRight }
                                }
                                Pill { text: (nodeButton.modelData.node_type === "controller" ? "中心" : "边缘") + " · " + (nodeButton.modelData.online ? "在线" : "离线") }
                            }
                            onClicked: { appController.selectNode(modelData.index); nodeSelectionDialog.close() }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: connectPage
        Item {
            RowLayout {
                anchors.fill: parent; anchors.margins: 40; spacing: 42
                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16
                    RowLayout {
                        spacing: 12
                        Image { Layout.preferredWidth: 42; Layout.preferredHeight: 42; source: "qrc:/assets/app_icon_256.png"; fillMode: Image.PreserveAspectFit }
                        LabelText { text: "MeowFRP"; font.pixelSize: 22; font.weight: Font.Bold }
                    }
                    Item { Layout.fillHeight: true }
                    Pill { text: "YOUR PRIVATE GATEWAY"; fg: root.primary }
                    LabelText { text: "连接，不设限。"; font.pixelSize: 35; font.weight: Font.Bold; Layout.fillWidth: true }
                    LabelText { text: "把本地服务，\n带到你需要的地方。"; font.pixelSize: 22; color: root.muted; lineHeight: 1.35; Layout.fillWidth: true }
                    Item { Layout.preferredHeight: 16 }
                    RowLayout { AppIcon { glyph: "\uE72E" } LabelText { text: "先鉴权，再选择授权节点"; color: root.muted } }
                    RowLayout { AppIcon { glyph: "\uE968" } LabelText { text: "集中管理，多节点接入"; color: root.muted } }
                    RowLayout { AppIcon { glyph: "\uE713" } LabelText { text: "跟随系统主题 · 自适应窗口材质"; color: root.muted } }
                    Item { Layout.fillHeight: true }
                    LabelText { text: "MEOWFRP CLIENT / 0.1"; color: root.muted; font.pixelSize: 11; font.letterSpacing: 1.5 }
                }
                Surface {
                    Layout.preferredWidth: 460; Layout.fillHeight: true; radius: 22
                    ScrollView {
                        anchors.fill: parent; anchors.margins: 30; clip: true; contentWidth: availableWidth
                        ColumnLayout {
                            width: parent.width; spacing: 12
                            RowLayout {
                                LabelText { text: "欢迎回来"; font.pixelSize: 27; font.weight: Font.Bold; Layout.fillWidth: true }
                                AppCombo { Layout.preferredWidth: 116; implicitHeight: 36; model: ["跟随系统", "浅色", "深色"]; currentIndex: appearance.themeMode === "light" ? 1 : appearance.themeMode === "dark" ? 2 : 0; onActivated: appearance.themeMode = ["system", "light", "dark"][currentIndex]; Accessible.name: "外观主题" }
                            }
                            LabelText { text: "使用系统生成的 Token 安全登录。"; color: root.muted; Layout.fillWidth: true }
                            Item { Layout.preferredHeight: 8 }
                            LabelText { text: "中心节点 API 地址"; font.weight: Font.DemiBold }
                            AppField { id: apiInput; Layout.fillWidth: true; text: appController.apiBaseUrl; placeholderText: "https://center.example.com/api"; enabled: !appController.busy; onTextEdited: appController.apiBaseUrl = text; onEditingFinished: appController.apiBaseUrl = text; Accessible.name: "中心节点 API 地址" }
                            LabelText { text: "用户 Token"; font.weight: Font.DemiBold }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 8
                                AppField {
                                    id: tokenInput
                                    objectName: "loginToken"
                                    Layout.fillWidth: true; placeholderText: "粘贴系统生成的 ak_… Token"
                                    echoMode: tokenReveal.checked ? TextInput.Normal : TextInput.Password
                                    enabled: !appController.busy; Accessible.name: "用户 Token"
                                    onAccepted: if (!appController.busy && text.trim().length > 0) { appController.apiBaseUrl = apiInput.text; appController.connectToServer(text) }
                                }
                                AppButton { id: tokenReveal; text: checked ? "隐藏" : "显示"; checkable: true; implicitWidth: 64; enabled: !appController.busy; Accessible.name: "显示或隐藏 Token" }
                            }
                            LabelText { text: "设备标识"; color: root.muted; font.pixelSize: 12 }
                            AppField { Layout.fillWidth: true; text: appController.clientId; readOnly: true; font.pixelSize: 11; Accessible.name: "设备标识" }
                            RowLayout {
                                Layout.fillWidth: true
                                AppSwitch { text: "调试模式"; checked: appController.debugMode; enabled: !appController.busy; onToggled: appController.debugMode = checked }
                                Item { Layout.fillWidth: true }
                                LabelText { text: "详细日志 · 凭据脱敏"; color: root.muted; font.pixelSize: 11 }
                            }
                            Rectangle {
                                Layout.fillWidth: true; implicitHeight: Math.max(52, authStatus.implicitHeight + 24); radius: 10
                                color: appController.errorMessage.length ? root.dangerSoft : root.primarySoft
                                LabelText { id: authStatus; anchors.fill: parent; anchors.margins: 12; text: appController.statusMessage; color: appController.errorMessage.length ? root.danger : root.primary; verticalAlignment: Text.AlignVCenter }
                            }
                            AppButton { Layout.fillWidth: true; implicitHeight: 46; text: appController.busy ? "正在验证身份…" : "登录并选择节点  →"; emphasized: true; enabled: !appController.busy && tokenInput.text.trim().length > 0 && apiInput.text.trim().length > 0; onClicked: { appController.apiBaseUrl = apiInput.text; appController.connectToServer(tokenInput.text) } }
                            AppButton { Layout.fillWidth: true; text: "保存连接地址"; enabled: !appController.busy; onClicked: appController.saveProfile() }
                            LabelText { Layout.fillWidth: true; text: "Token 只用于鉴权，不会作为 FRP 隧道运行凭据。"; color: root.muted; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: dashboardPage
        RowLayout {
            anchors.fill: parent; anchors.margins: 20; spacing: 22
            Surface {
                Layout.preferredWidth: 202; Layout.fillHeight: true
                color: root.hc ? root.panel : root.dark ? "#cd1b2639" : "#d9ffffff"
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 10
                    RowLayout { Image { Layout.preferredWidth: 28; Layout.preferredHeight: 28; source: "qrc:/assets/app_icon_256.png"; fillMode: Image.PreserveAspectFit } LabelText { text: "MeowFRP"; font.pixelSize: 20; font.weight: Font.Bold } }
                    LabelText { text: appController.userName || "已鉴权用户"; color: root.muted; Layout.fillWidth: true; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                    Pill { text: appController.frpcRunning ? "frpc 进程运行中" : "控制面已连接"; fg: root.success; fill: root.successSoft }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: root.line; Layout.topMargin: 12; Layout.bottomMargin: 12 }
                    NavButton { objectName: "navTunnels"; Layout.fillWidth: true; text: "隧道工作台"; glyph: "\uE8A5"; selected: root.pageIndex === 0; onClicked: root.pageIndex = 0 }
                    NavButton { objectName: "navLogs"; Layout.fillWidth: true; text: "运行日志"; glyph: "\uE8A0"; selected: root.pageIndex === 1; onClicked: root.pageIndex = 1 }
                    NavButton { objectName: "navSettings"; Layout.fillWidth: true; text: "客户端设置"; glyph: "\uE713"; selected: root.pageIndex === 2; onClicked: root.pageIndex = 2 }
                    Item { Layout.fillHeight: true }
                    LabelText { text: "设备 / " + appController.clientId; Layout.fillWidth: true; color: root.muted; font.pixelSize: 10; wrapMode: Text.NoWrap; elide: Text.ElideMiddle }
                    AppButton { Layout.fillWidth: true; text: "重新鉴权 / 切换节点"; enabled: !appController.busy; onClicked: appController.connectToServer() }
                    AppButton { Layout.fillWidth: true; text: appController.quitInProgress ? "正在退出…" : "退出客户端"; destructive: true; enabled: !appController.busy || appController.quitInProgress; onClicked: appController.quitClient() }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; spacing: 18
                RowLayout {
                    Layout.fillWidth: true
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 7
                        LabelText { text: root.pageIndex === 0 ? "隧道工作台" : root.pageIndex === 1 ? "运行日志" : "客户端设置"; font.pixelSize: 27; font.weight: Font.Bold }
                        LabelText { Layout.fillWidth: true; text: appController.statusMessage; color: appController.errorMessage.length ? root.danger : root.muted; maximumLineCount: 2; elide: Text.ElideRight }
                    }
                    Pill { text: "" + appController.tunnelCount + " / " + appController.tunnelLimit + " 隧道" }
                }
                Loader { Layout.fillWidth: true; Layout.fillHeight: true; sourceComponent: root.pageIndex === 0 ? tunnelPage : root.pageIndex === 1 ? logsPage : settingsPage }
            }
        }
    }

    Component {
        id: tunnelPage
        ScrollView {
            id: tunnelScroll
            objectName: "tunnelScroll"
            clip: true; contentWidth: availableWidth
            ColumnLayout {
                width: tunnelScroll.availableWidth; spacing: 16
                RowLayout {
                    Layout.fillWidth: true; spacing: 12
                    StatCard { Layout.fillWidth: true; label: "当前节点 FRP 地址"; value: appController.frpEndpoint }
                    StatCard { Layout.fillWidth: true; label: "可用端口池"; value: appController.portStart + " – " + appController.portEnd }
                    StatCard { Layout.fillWidth: true; label: "授权协议"; value: appController.allowedProtocolsText.toUpperCase() }
                }
                Surface {
                    Layout.fillWidth: true; implicitHeight: permissionRow.implicitHeight + 32
                    RowLayout {
                        id: permissionRow
                        anchors.fill: parent; anchors.margins: 16; spacing: 16
                        AppIcon { glyph: "\uE72E"; color: root.success }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 5
                            LabelText { text: "服务端授权 · 最多 " + appController.maxPorts + " 个端口"; font.weight: Font.DemiBold }
                            LabelText { text: "DPI " + (appController.dpiEnabled ? appController.dpiMode : "未启用") + "  ·  拒绝：" + appController.dpiBlockedText + "  ·  检测器：" + appController.dpiDetectorsText; color: root.muted; font.pixelSize: 11; Layout.fillWidth: true }
                        }
                        Pill { text: appController.dpiEnabled ? "DPI 已启用" : "DPI 未启用" }
                    }
                }
                Surface {
                    Layout.fillWidth: true; implicitHeight: addLayout.implicitHeight + 40
                    ColumnLayout {
                        id: addLayout
                        anchors.fill: parent; anchors.margins: 20; spacing: 16
                        LabelText { text: "添加隧道"; font.pixelSize: 17; font.weight: Font.DemiBold }
                        GridLayout {
                            Layout.fillWidth: true; columns: 2; uniformCellWidths: true; columnSpacing: 16; rowSpacing: 12
                            ColumnLayout { Layout.fillWidth: true; LabelText { text: "隧道名称"; color: root.muted } AppField { id: proxyName; Layout.fillWidth: true; text: "ssh"; Accessible.name: "隧道名称" } }
                            ColumnLayout { Layout.fillWidth: true; LabelText { text: "穿透协议"; color: root.muted } AppCombo { id: protocol; Layout.fillWidth: true; model: appController.allowedProtocols; Accessible.name: "穿透协议" } }
                            ColumnLayout { Layout.fillWidth: true; LabelText { text: "本地地址"; color: root.muted } AppField { id: localIp; Layout.fillWidth: true; text: "127.0.0.1"; Accessible.name: "本地地址" } }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 12
                                ColumnLayout { Layout.fillWidth: true; LabelText { text: "本地端口"; color: root.muted } AppSpin { id: localPort; objectName: "localPort"; Layout.fillWidth: true; from: 1; to: 65535; value: 22; Accessible.name: "本地端口" } }
                                ColumnLayout { Layout.fillWidth: true; LabelText { text: "服务端端口"; color: root.muted } AppSpin { id: remotePort; objectName: "remotePort"; Layout.fillWidth: true; from: Math.max(1, appController.portStart); to: Math.max(from, appController.portEnd); value: from; Accessible.name: "服务端端口" } }
                            }
                        }
                        RowLayout {
                            AppButton { text: appController.busy ? "处理中…" : "+ 加入列表"; emphasized: true; enabled: !appController.busy && appController.tunnelCount < appController.tunnelLimit && protocol.currentIndex >= 0; onClicked: appController.addTunnel(proxyName.text, protocol.currentText, localIp.text, localPort.value, remotePort.value) }
                            AppButton { text: appController.frpcRunning ? "应用并重启" : "启动列表"; enabled: !appController.busy && appController.tunnelCount > 0; onClicked: appController.startTunnels() }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }
                Surface {
                    Layout.fillWidth: true; implicitHeight: tunnelsLayout.implicitHeight + 40
                    ColumnLayout {
                        id: tunnelsLayout
                        anchors.fill: parent; anchors.margins: 20; spacing: 12
                        RowLayout {
                            LabelText { text: "我的隧道"; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.fillWidth: true }
                            AppButton { text: appController.frpcRunning ? "重启全部" : "启动全部"; implicitHeight: 36; enabled: !appController.busy && appController.tunnelCount > 0; onClicked: appController.startTunnels() }
                            AppButton { text: "停止全部"; implicitHeight: 36; destructive: true; enabled: appController.frpcRunning && !appController.busy; onClicked: appController.stopAllTunnels() }
                        }
                        LabelText { visible: appController.tunnelCount === 0; Layout.fillWidth: true; Layout.topMargin: 24; Layout.bottomMargin: 24; text: "还没有隧道。在上方添加本地服务，开始你的第一次连接。"; color: root.muted; horizontalAlignment: Text.AlignHCenter }
                        Repeater {
                            model: appController.tunnelList
                            delegate: Rectangle {
                                id: tunnelRow
                                required property var modelData
                                Layout.fillWidth: true; implicitHeight: 94; radius: 12; color: root.fieldColor; border.color: root.line
                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 14; spacing: 12
                                    AppIcon { glyph: "\uE8A5" }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 7
                                        RowLayout { LabelText { text: tunnelRow.modelData.name; font.weight: Font.DemiBold } Pill { text: tunnelRow.modelData.type.toUpperCase(); implicitHeight: 22 } }
                                        LabelText { Layout.fillWidth: true; text: tunnelRow.modelData.local_ip + ":" + tunnelRow.modelData.local_port + "  →  " + tunnelRow.modelData.remote_endpoint; color: root.muted; font.pixelSize: 11; wrapMode: Text.NoWrap; elide: Text.ElideMiddle }
                                        LabelText { text: tunnelRow.modelData.status; color: root.success; font.pixelSize: 10 }
                                    }
                                    AppButton { text: "复制地址"; implicitHeight: 34; enabled: !appController.busy; onClicked: appController.copyRemoteEndpoint(tunnelRow.modelData.index) }
                                    AppButton { text: appController.frpcRunning ? "关闭" : "删除"; implicitHeight: 34; destructive: true; enabled: !appController.busy; onClicked: appController.frpcRunning ? appController.stopTunnel(tunnelRow.modelData.index) : appController.removeTunnel(tunnelRow.modelData.index) }
                                }
                            }
                        }
                        LabelText { visible: appController.frpcRunning; Layout.fillWidth: true; text: "提示：当前关闭单条隧道会重新配置 frpc，其余隧道可能短暂重连。"; font.pixelSize: 11; color: root.muted }
                    }
                }
            }
        }
    }

    Component {
        id: logsPage
        ColumnLayout {
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                LabelText { text: appController.debugMode ? "调试模式 · " + appController.debugLogPath : "普通模式 · 可在登录界面开启详细调试日志"; Layout.fillWidth: true; color: root.muted; font.pixelSize: 11; maximumLineCount: 2; elide: Text.ElideMiddle }
                AppButton { text: "复制日志"; implicitHeight: 36; onClicked: appController.copyLogs() }
                AppButton { text: "清空"; implicitHeight: 36; onClicked: appController.clearLogs() }
            }
            Surface {
                Layout.fillWidth: true; Layout.fillHeight: true; color: root.dark ? "#101a2a" : root.fieldColor
                ScrollView {
                    anchors.fill: parent; anchors.margins: 6; clip: true
                    TextArea {
                        readOnly: true; text: appController.logText; wrapMode: TextEdit.Wrap; selectByMouse: true
                        color: root.dark ? "#c5d8f4" : root.ink; selectionColor: root.primaryFill; selectedTextColor: "#ffffff"
                        font.family: "Consolas"; font.pixelSize: 12; padding: 14; background: Item {}
                    }
                }
            }
        }
    }

    Component {
        id: settingsPage
        ScrollView {
            id: settingsScroll
            objectName: "settingsScroll"
            clip: true; contentWidth: availableWidth
            ColumnLayout {
                width: settingsScroll.availableWidth; spacing: 16
                Surface {
                    Layout.fillWidth: true; implicitHeight: appearanceLayout.implicitHeight + 40
                    ColumnLayout {
                        id: appearanceLayout
                        anchors.fill: parent; anchors.margins: 20; spacing: 14
                        LabelText { text: "外观与体验"; font.pixelSize: 17; font.weight: Font.DemiBold }
                        RowLayout {
                            LabelText { text: "外观主题"; Layout.fillWidth: true }
                            AppCombo { Layout.preferredWidth: 190; model: ["跟随 Windows 系统", "浅色模式", "深色模式"]; currentIndex: appearance.themeMode === "light" ? 1 : appearance.themeMode === "dark" ? 2 : 0; onActivated: appearance.themeMode = ["system", "light", "dark"][currentIndex]; Accessible.name: "外观主题" }
                        }
                        RowLayout { LabelText { text: "透明窗口材质"; Layout.fillWidth: true } AppSwitch { checked: appearance.glassEnabled; enabled: !appearance.highContrast; onToggled: appearance.glassEnabled = checked; Accessible.name: "透明窗口材质" } }
                        LabelText { text: appearance.materialDescription; Layout.fillWidth: true; color: root.muted; font.pixelSize: 12 }
                        LabelText { text: "主题与材质修改即时生效；Windows 关闭透明效果、高对比度模式或不支持 Mica 时使用实色背景。文字、输入框和日志始终保持清晰。"; Layout.fillWidth: true; color: root.muted; font.pixelSize: 11 }
                    }
                }
                Surface {
                    Layout.fillWidth: true; implicitHeight: configLayout.implicitHeight + 40
                    ColumnLayout {
                        id: configLayout
                        anchors.fill: parent; anchors.margins: 20; spacing: 12
                        LabelText { text: "连接与运行"; font.pixelSize: 17; font.weight: Font.DemiBold }
                        LabelText { text: "中心 API 地址"; color: root.muted }
                        AppField { Layout.fillWidth: true; text: appController.apiBaseUrl; enabled: !appController.busy; onTextEdited: appController.apiBaseUrl = text; onEditingFinished: appController.apiBaseUrl = text; Accessible.name: "中心 API 地址" }
                        LabelText { text: "当前账号 / 设备标识"; color: root.muted }
                        AppField { Layout.fillWidth: true; text: appController.userName + " / " + appController.clientId; readOnly: true }
                        LabelText { text: "frpc 程序路径"; color: root.muted }
                        AppField { Layout.fillWidth: true; text: appController.frpcPath; enabled: !appController.busy; onTextEdited: appController.frpcPath = text; onEditingFinished: appController.frpcPath = text; Accessible.name: "frpc 程序路径" }
                        LabelText { text: "配置与运行目录"; color: root.muted }
                        AppField { Layout.fillWidth: true; text: appController.runtimeDir; enabled: !appController.busy; onTextEdited: appController.runtimeDir = text; onEditingFinished: appController.runtimeDir = text; Accessible.name: "配置与运行目录" }
                        RowLayout { AppButton { text: "保存设置"; emphasized: true; enabled: !appController.busy; onClicked: appController.saveProfile() } AppButton { text: "重新连接"; enabled: !appController.busy; onClicked: appController.connectToServer() } }
                    }
                }
            }
        }
    }
    Rectangle {
        visible: uiPreviewMode
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 8
        width: previewLabel.implicitWidth + 20; height: 26; radius: 8; color: root.primarySoft
        LabelText { id: previewLabel; anchors.centerIn: parent; text: "界面预览 · 演示数据 · 不连接服务器"; color: root.primary; font.pixelSize: 10 }
    }
}
