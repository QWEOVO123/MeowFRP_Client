# MeowFRP 客户端

[English](./README.md)

MeowFRP 客户端是 **MeowFRP Server** 的 Qt 桌面客户端。它从中心获取 Controller/Edge 节点目录，通过 HTTPS 直接连接用户选择的节点，获取资源策略和 FRP 运行租约，并负责管理本地 `frpc` 隧道进程。

配套服务端项目：[`MeowFRP_Server`](https://github.com/QWEOVO123/MeowFRP_Server)

## 主要功能

- 使用 Qt Quick 和 QML 构建的现代化中文界面
- 根据 Windows MachineGuid 生成设备标识，并使用 SHA-256 处理
- 自动记忆 API 地址和用户访问令牌
- 从中心公开接口获取节点目录，并允许用户明确选择 Controller 或 Edge
- 长期令牌直接发送到所选 Edge，客户端业务流量不经过中心转发
- 根据服务端返回结果限制端口范围、隧道数量和可用协议
- 单次会话支持创建和管理多个 TCP 或 UDP 隧道
- 通过 HTTPS 获取 FRP 配置和默认 24 小时的运行租约
- 管理本地 `frpc` 的启动、停止和实时日志
- 一键复制远程隧道地址
- 显示当前 DPI 状态以及被阻断的流量类型
- 每十秒发送一次 HTTPS 心跳，用于维持在线状态并接收服务端命令
- 支持服务端远程停止 FRP、显示违规提示和要求重新鉴权，并通过 HTTPS 回传执行 ACK
- 正常退出或重新鉴权时主动通知服务端下线
- Edge 与中心失联时保留已有 FRP 隧道，新登录会被拒绝，在线用户会收到清晰可读的失联提示

## 工作流程

1. 用户输入完整的中心 API 基础地址，客户端通过免鉴权的 `/api/v1/public/nodes` 下载节点目录。
2. 用户选择中心自身或可用 Edge，并输入长期访问令牌。
3. 客户端把令牌和设备标识直接发送到所选节点，并请求 `/api/v1/client/resource-policy`。
4. 所选节点返回自身的 FRP 地址、允许协议、端口范围、隧道数量限制和 DPI 状态。
5. 用户在节点规定的权限范围内创建隧道，并提交到 `/api/v1/client/bootstrap`。
6. 所选节点生成 `frpc` 配置并返回默认有效 24 小时的运行租约。
7. 客户端把配置写入运行目录，然后启动 `frpc`。
8. 鉴权有效期间，客户端每十秒发送一次心跳、执行排队命令，并在执行成功后通过所选节点的 HTTPS API 回传 ACK。

用户长期使用的 HTTPS 访问令牌不会被直接用作 FRP 认证令牌。Edge 与中心失联时，已有 FRP 隧道继续运行；新的资源查询和 bootstrap 会返回 `edge_controller_disconnected`，客户端只弹出一次节点失联提示。

## 构建要求

- Windows 10 或更高版本
- Qt 6，并安装 Core、Gui、Qml、Quick、QuickControls2 和 Network 模块
- CMake 3.16 或更高版本
- Qt 支持的 C++17 编译器
- 与服务端兼容的 `frpc.exe`
- 正在运行的 `MeowFRP_server`

## 编译

可以使用 Qt Creator 直接打开本仓库的 CMake 项目，也可以在命令行编译：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\mingw_64
cmake --build build --config Release -j 6
```

在普通 PowerShell 中使用 MinGW 时，先把 Qt 和编译器工具加入 `PATH`：

```powershell
$env:Path = 'C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;' + $env:Path
```

## 使用方法

1. 启动或部署 MeowFRP Controller，并在中心面板中为每个可选节点配置公开 API URL。
2. 在中心 Web 面板中创建普通用户，并复制自动生成的 HTTPS API 令牌。
3. 把 `frpc.exe` 放到客户端可以找到的位置，或者在客户端设置中选择其路径。
4. 启动 MeowFRP 客户端，输入完整的中心 API 基础地址。客户端不会自动添加 `/api` 前缀。
5. 刷新节点目录、选择目标节点、输入用户令牌并登录；令牌会直接发送给所选节点。
6. 鉴权成功后，在该节点返回的权限范围内添加隧道并启动 FRP。

首次启动时 API 地址输入框为空。本地开发时通常填写 `http://127.0.0.1:8080/api`；如果反向代理使用了 `/api` 等路径，需要由用户在地址中完整填写。

## 项目结构

```text
app/src/              C++ 应用控制、API、配置和运行服务
app/assets/           应用图标和 Windows 资源
qml/Main.qml          Qt Quick 用户界面
docs/architecture.md  客户端架构和 API 协议说明
packaging/             Windows 启动器源码
tools/                 开发辅助工具
```

运行配置和自动生成的 FRP 文件保存在源码目录之外，并已通过 Git 忽略规则排除。

## 安全说明

- 用户的 HTTPS API 令牌属于敏感凭证，请勿泄露。
- 正式部署时应只通过 HTTPS 暴露控制 API。
- 为了自动登录，客户端目前会在本地持久化保存令牌，请妥善保护 Windows 用户目录。
- 服务端签发的 FRP 租约是临时凭证，退出或心跳超时后会立即撤销。

## 许可证

请参阅 [LICENSE](./LICENSE)。
