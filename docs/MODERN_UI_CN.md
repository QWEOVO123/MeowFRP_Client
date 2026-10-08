# 客户端现代界面（2026-10-07）

保留 Qt Quick 和现有 C++ 鉴权、节点目录、心跳、隧道进程管理逻辑；本轮未修改这些业务组件。

## 外观

- 蓝色系登录卡片、原有猫咪图标、授权节点选择卡片、侧栏式隧道/日志/设置工作台。
- 默认跟随 Windows 应用主题，可在登录页和设置页切换浅色/深色；修改即时生效并保存。
- Windows 11 22H2 及以上尝试通过官方 DWM 接口启用 Mica，并启用深色原生标题栏、圆角。保留原生标题栏的拖动、缩放和系统菜单。
- 尊重系统透明效果开关、高对比度模式。不支持的 Windows 或系统关闭透明效果时自动使用实色背景。Mica 是系统背景材质，不是把整窗文字一起做半透明，也不是保证任意窗口后方都有高斯模糊。
- 半透明背景仅作用于窗口/侧栏，输入框、弹窗和文字保持高可读性。按钮悬停、开关滑动、弹窗进入/退出使用短动画。
- 880×640 最小窗口，表单、列表和设置页可滚动；不把内容固定在不可见区域。

## 功能保留

先以 Token 在中心鉴权，再选择授权节点。保留登录页调试模式、日志复制/清空、端口池限制、增删隧道、启动/停止、地址复制、服务端提示、退出前下线处理。

本轮没有实现独立停止单个 frpc 代理。原有“关闭单条”仍会重新配置 frpc，其他隧道可能短暂重连，界面已明确提示。进程运行不等于代理成功，仍须查看 frpc 日志。

## 隔离预览与测试

`MeowFRP_Client.exe --ui-preview dashboard --ui-theme dark` 可在演示数据下查看界面，支持 login/dashboard/nodes/logs/settings。

预览使用独立 `UiPreviewController`，不创建真实 `AppController`，不读写业务配置、Token、运行日志，不发 HTTP，不启动 frpc。主题偏好也不持久化。

源码构建要求 Qt 6.8+（本轮使用 C:\Qt\6.11.1\mingw_64）。`client-ui-tests` 测试全部页面的主题切换、最小尺寸滚动、导航、节点选择和预览设置隔离。

`--ui-screenshot <文件>` 与 `--ui-report <文件>` 只在预览模式生效；前者用于实际 QML 渲染截图，后者记录平台、主题、窗口材质状态，不含业务凭据。

官方接口参考：[Qt QStyleHints](https://doc.qt.io/qt-6/qstylehints.html)、[Windows DWM 背景材质](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type)。
