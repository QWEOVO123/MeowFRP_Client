# MeowFRP 客户端

[English](./README.md) | [服务端仓库](https://github.com/QWEOVO123/MeowFRP_Server)

MeowFRP Client 是 MeowFRP 的 Windows 桌面组件，负责向中心鉴权、选择授权节点、请求服务端生成的隧道配置，并管理适配版 FRPC 的运行进程。

界面采用 Qt Quick/QML；C++ 服务负责 HTTPS 请求、会话状态、FRPC 生命周期、本地偏好与诊断日志。

## 依赖与支持范围

- 已初始化的 MeowFRP 中心与系统生成的用户 Token。
- 管理员显式分配的节点访问权限与资源权限。
- 中心及所选节点均具有客户端可访问的 HTTPS API。
- 配套版本的服务端与适配版 FRPC。
- 处于所选 Qt/编译工具链支持范围内的 Windows 10/11。

FRPC–FRPS 传输仅支持 **TCP + 经验证的 TLS 1.3**。交付程序不支持明文、KCP、QUIC 或 WebSocket 传输；TCP、UDP 代理业务均可承载于该传输通道。

当前适配版 FRP 版本标识为 0.69.1。客户端依赖适配后的 HTTPS 信任根获取与运行授权流程，不能将上游或历史 `frpc.exe` 视为可直接替换的组件。

## 架构与接入流程

~~~text
Qt 客户端 -- HTTPS/用户 Token --> 中心：身份校验与授权节点目录
    |
    +-- HTTPS -----------------> 所选节点：策略、bootstrap、心跳、ACK
    +-- QProcess --------------> 适配版 FRPC
                                     |
                                     +-- TCP + TLS 1.3 --> 所选节点 FRPS

访问者 --> 节点穿透端口 --> FRPS --> FRPC --> 本地服务
~~~

1. 向中心 `POST /api/v1/client/login` 提交用户 Token 与设备 ID。
2. 从授权目录选择节点。
3. 查询 `GET /api/v1/client/frp-transport`，随后请求目标节点的有效 `resource-policy`。
4. 配置本地地址/端口、远程端口及 TCP/UDP 代理类型。
5. 请求 `POST /api/v1/client/bootstrap`，校验资源并获取临时运行租约及 TOML。
6. 启动 FRPC、获取节点信任根、完成 TLS 与 FRP 登录，并注册已分配代理。
7. 维持客户端心跳并确认远程命令。

用户 API Token 与 FRP 运行 Token 具有独立作用域。运行租约通常为 24 小时，实际有效期与资源限制由服务端配置决定。Web 管理员密码不是桌面用户登录凭据。

`QNetworkAccessManager` 处理 API 请求；请求代次用于避免退出、切换节点或会话后接受过期响应。客户端心跳通常每 10 秒一次。

## TLS 信任与密钥协商

### 独立信任域

| 连接 | 信任方式 |
| --- | --- |
| Qt 客户端 → HTTPS API | 常规 HTTPS 证书校验，使用平台信任 |
| FRPC → HTTPS 信任端点 | 常规 HTTPS，使用系统根并拒绝重定向 |
| FRPC → FRPS | 独立证书池，仅包含所选节点下载的 CA |
| 边缘 → 中心 | 服务端管理 mTLS，独立于桌面 FRP 信任 |

普通登录流程查询传输状态，不下载 FRP 根证书。启动隧道时，Qt 通过 `--trust-api` 将所选节点的信任 API 地址传给适配版 FRPC。

每次建立或重建 FRPC 底层物理连接前，FRPC 获取 `GET /api/v1/client/frp-trust`，校验预期节点 ID、启用状态、证书元数据与当前有效自签根，再创建仅包含该根的空信任池。FRPS 证书与 `server_name` 使用此信任池校验，不接受其他系统根作为 FRP 信任依据。

FRP 根证书仅存在于连接内存，不持久化到客户端配置、运行 TOML、临时证书文件或 Windows 信任库。客户端不配置 FRP TLS 会话缓存，服务端禁用票据；TCP 复用可以共享已有 TLS 连接，无需新建物理握手。

API 提供公开信任锚，TLS 1.3 负责身份验证与会话密钥协商；私钥与流量密钥不从 API 下载。证书/API 错误或 `enabled:false` 均拒绝连接，不回退明文。节点身份重置后可能需要重新获取信任根并重试。

TLS 加密范围为 FRPC–FRPS；访问者到节点、客户端到本地服务是否加密由业务协议决定。

### 启动进度

隧道启动进度由 API 结果与 FRPC 事件驱动：

| 阶段 | 完成依据 |
| --- | --- |
| 配置鉴权 | bootstrap 成功响应 |
| TLS 信任根获取 | 适配版 FRPC 的信任根获取事件 |
| FRP 登录 | `login to server success` |
| 代理注册 | 全部配置代理返回 `start proxy success` |

进程启动、HTTP 心跳 200 不代表隧道可用。错误保留在对应阶段；重连开始新的尝试，普通工作连接不会重置已完成的启动进度。

## 使用步骤

1. 输入完整中心 API 地址，例如 `https://frp.example.com/api`。
2. 输入系统生成的普通用户 Token。
3. 鉴权并选择已授权的在线节点。
4. 添加隧道，填写本地地址、本地端口、远程端口与协议。
5. 启动隧道，检查 FRP 登录及代理注册结果。

将 `127.0.0.1:25565` 映射至远程 `25565` 时，访问者使用所选节点公网地址的 25565 端口。本地服务必须实际监听目标地址，节点防火墙与安全组也需允许远程端口。

节点可见不等于资源可用。服务端节点端口池、用户策略、Token 授权、有效租约与故障准入仍共同约束请求。

## 运行与故障行为

`QProcess` 管理 FRPC。停止时先调用 `terminate()`，约 3 秒未退出则强制终止。诊断日志记录界面操作、配置替换、服务端命令与退出等停止来源。

短暂 API 请求失败不会直接停止已运行的 FRPC。控制面故障时，服务端拒绝新登录与新代理，向持有已注册代理的客户端发送警告，并在注册代理计数归零后要求重新鉴权。命令执行带去重与确认。

**当前停止单条隧道会重建剩余 FRPC 配置，其他隧道可能短暂重连。** 尚未实现独立停止单代理，配置重建也不能绕过服务端故障准入限制。

进程重启、传输故障、主动退出、租约失效、封禁与管理员操作可能中断隧道。进程运行标记不能证明代理已注册或本地服务可达。

## 本地状态与诊断

| 数据 | 处理方式 |
| --- | --- |
| 用户 Token | 当前进程内存；当前配置格式不保存 Token |
| 偏好配置 | API 地址、设备 ID、程序路径、节点选择与调试偏好 |
| FRP 信任根 | 连接内存；新物理连接重新获取 |
| 运行 TOML | 含临时运行凭据；通常位于 `%APPDATA%\frp-control\frp-control-client\runtime\` |
| 调试日志 | 通常位于 `%LOCALAPPDATA%\frp-control\frp-control-client\logs\`；界面显示实际路径 |

设备 ID 为基于 Windows MachineGuid 或回退标识生成的带前缀 SHA-256 摘要，不直接上报原始 MachineGuid。该机制用于设备识别，不提供不可伪造的硬件认证。

在登录页开启**调试模式**后再登录，可启用详细诊断并将 FRPC 日志设为 `debug`。

| 标记 | 诊断内容 |
| --- | --- |
| `[ACTION]`、`[AUTH]`、`[SESSION]` | 操作来源、鉴权与会话切换 |
| `[HTTP #id]` | 请求端点、状态、耗时与脱敏摘要 |
| `[HEARTBEAT]`、`[COMMAND]` | 心跳、命令处理与 ACK |
| `[PROCESS]`、`[CONFIG]`、`[LEASE]` | PID、配置路径、停止来源与租约释放 |
| `[FRPC stdout/stderr]`、`[QML]` | FRPC 输出与界面诊断 |

HTTP 摘要使用字段白名单并脱敏已识别凭据。日志仍可能包含节点地址、路径、隧道名称及服务端消息；运行 TOML 必须按含凭据文件处理。

调试文件约 10 MiB 后轮转，保留一个 `.previous` 分段，不自动清除所有会话文件。界面最多保留约 10,000 行 / 2 MiB；清空界面不删除磁盘日志。当前 Token 处理方式不会安全擦除旧版本遗留文件。

## Windows 外观

支持系统、浅色与深色主题，监听 Qt 及 Windows 主题变化。`AppearanceController` 使用 DWM 属性调整受支持的标题栏、圆角与背景材质。

Windows 11 22H2 及以上在支持且系统允许时尝试 Mica；其他环境使用实色回退。系统透明效果和高对比度设置优先。最小窗口为 880×640，表单与列表支持滚动。

## 构建与打包

依赖 Qt 6.8+ 的 Core、Gui、Qml、Quick、QuickControls2、Network，CMake 3.16+ 及 C++17 编译器。可选 UI 测试目标需要 Qt Test。

以下为 Qt 6.11.1/MinGW 的 PowerShell 构建示例，路径需匹配已安装工具链：

~~~powershell
$qtRoot = 'C:\Qt\6.11.1\mingw_64'
$env:Path = 'C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;' + $qtRoot + '\bin;' + $env:Path
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build-release -G Ninja "-DCMAKE_PREFIX_PATH=$qtRoot" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build-release --parallel 6
~~~

源码目标生成 `build-release/frp-control-client.exe`。

在服务端仓库的 `third_party/frp` 中构建配套 FRPC：

~~~powershell
$env:CGO_ENABLED = '0'
$env:GOOS = 'windows'
$env:GOARCH = 'amd64'
go build -trimpath -tags frpc,noweb -o frpc.exe ./cmd/frpc
~~~

在客户端仓库打包至新目录：

~~~powershell
.\scripts\package-windows.ps1 `
  -BuildDir .\build-release `
  -OutputDir C:\packages\MeowFRP-client `
  -FrpcPath ..\MeowFRP_Server\third_party\frp\frpc.exe `
  -FrpLicensePath ..\MeowFRP_Server\third_party\frp\LICENSE `
  -QtRoot C:\Qt\6.11.1\mingw_64
~~~

脚本拒绝覆盖已有输出目录，将发布主程序命名为 `MeowFRP_Client.exe`，调用 `windeployqt` 并包含配套 FRPC 与许可文件。打包限制未使用的样式、翻译及调试插件，同时保留图形回退。

应分发完整打包目录。FRPC 查找顺序为主程序同目录、上级目录、配置路径；`[PROCESS] launch program=...` 表示实际运行的程序。

## 验证与离线预览

~~~powershell
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build-release --output-on-failure
.\build-release\frp-control-client.exe --ui-preview dashboard --ui-theme dark
~~~

预览页面包括 `login`、`nodes`、`dashboard`、`logs`、`settings`，主题包括 `system`、`light`、`dark`。`UiPreviewController` 使用隔离演示数据，不访问真实配置/Token、不发 HTTP、不启动 FRPC，也不持久化主题变更。

预览专用参数 `--ui-screenshot <文件>`、`--ui-report <文件>` 输出截图和外观诊断。UI 测试及预览不验证真实节点鉴权、端到端隧道或生产网络兼容性。

## 实现目录

| 路径 | 职责 |
| --- | --- |
| `app/src/app_controller.*` | 鉴权、节点/隧道状态、心跳与命令 |
| `app/src/control_api_client.*` | HTTPS API、请求代次与诊断 |
| `app/src/tunnel_runtime_service.*` | FRPC 进程、运行配置、信任参数与停止 |
| `app/src/tunnel_startup_progress.h` | 事件驱动的隧道启动阶段 |
| `app/src/profile_service.*` | 偏好与设备标识 |
| `app/src/appearance_controller.*` | Windows 主题与 DWM 集成 |
| `app/src/log_safety.h` | 凭据脱敏 |
| `app/src/ui_preview_controller.h` | 隔离预览模型 |
| `qml/Main.qml` | Qt Quick 界面 |
| `scripts/package-windows.ps1` | Windows 运行依赖打包 |
| `tests/ui_test.cpp` | UI 回归测试 |

FRPC 信任实现位于服务端仓库的 `third_party/frp/pkg/transport/memory_trust.go`、`third_party/frp/client/connector.go`。节点身份同步与 DPI 执行由服务端负责。

补充文档：[调试日志](./docs/DEBUG_LOGGING_CN.md)、[界面实现](./docs/MODERN_UI_CN.md)。

## 许可证

客户端采用 [GNU AGPL v3](./LICENSE)。适配版 FRPC 保留 FRP 的 Apache 2.0 许可，Qt 及其他依赖遵循各自许可要求。发布包需保留相应许可文件。
