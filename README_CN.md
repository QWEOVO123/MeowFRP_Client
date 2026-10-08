# MeowFRP 客户端

[English](./README.md) · [配套服务端](https://github.com/QWEOVO123/MeowFRP_Server)

这是 MeowFRP 的 Windows 桌面客户端。填入中心地址和你的用户 Token，选择一个有权限的节点，再告诉它要把本机哪个服务映射出去。配置由服务器下发，客户端负责启动 FRPC、展示日志和处理服务端命令。

界面使用 Qt Quick / QML，底层保留 C++ 的鉴权、HTTP 会话和进程管理。它不是一个“启动了进程就算成功”的壳：进程状态、FRP 登录和隧道注册是不同阶段，遇到问题可以从日志逐步确认。

## 日常用起来是什么样

- 蓝色系登录卡片、授权节点选择、侧栏式隧道/日志/设置页面。
- 跟随 Windows 浅色/深色主题，也能手动选择浅色或深色。
- 支持时使用 Windows Mica 背景材质、原生深色标题栏和圆角；不支持时自动回退到实色。
- 根据服务器策略限制节点、端口、协议和隧道数量，支持 TCP / UDP。
- 显示有效 DPI 策略，复制远程地址，查看 FRPC 实时日志。
- HTTPS 心跳接收提示、停止和重新鉴权命令，并回传执行 ACK。
- 登录页提供“调试模式”，记录 HTTP、会话、命令及 FRPC 进程的详细状态。

Mica 是系统背景材质，不是任意窗口后方都会出现高斯模糊。程序尊重系统透明效果和高对比度设置；不要为了透明效果牺牲文字可读性。

## 开始之前

你需要一台已配置的 MeowFRP 中心、管理员生成的普通用户 Token，以及管理员给这个用户分配的节点和资源权限。

使用完整版时保留整个客户端目录：主程序、Qt DLL、QML/插件目录和配套 `frpc.exe` 都需要。只复制一个 EXE 到别处，可能无法启动。

推荐使用同一版本服务端源码构建的 FRPC。当前配套 FRP 版本标识为 0.69.1，不承诺任意历史版本二进制都可替换。

## 登录和创建隧道

1. 中心 API 地址填完整 URL，例如 `https://frp.example.com/api`。第一次启动不预设真实服务器；不要省略管理员提供的 `/api` 等路径。
2. 填用户 Token。桌面客户端不使用 Web 管理员账号密码。
3. 点击“登录并选择节点”。客户端先在中心鉴权，中心只返回用户被允许访问的节点。
4. 选择在线节点，客户端直接到这个节点查询资源策略并完成鉴权。
5. 添加隧道，填本地地址、本地端口、远程端口和协议，再启动。
6. 日志出现 `login to server success`、`start proxy success` 后，才算 FRP 登录和代理注册成功。

例如，远程端口 `25565` 指向本地 `127.0.0.1:25565` 时，外部访问的是“节点公网地址:25565”。本机还必须真的有服务监听 25565，节点防火墙和安全组也必须放行远程端口。

**“frpc 进程已启动”和 HTTP 心跳 200 都不等于隧道成功。** 如果日志一直 EOF，先查 FRP 接入链路；如果注册成功后报本地连接被拒绝，再查本机服务。

## 数据怎么走

```text
客户端 ── HTTPS + 用户 Token ── 中心：验证身份，返回授权节点
   │
   ├── HTTPS ── 所选节点：策略、配置、心跳、远程命令 ACK
   └── FRPC ─── 所选节点 FRPS：代理注册和业务转发

外部访问者 → 节点穿透端口 → FRP 工作连接 → 本机目标服务
```

中心登录使用 `POST /api/v1/client/login`；选节点后调用 `resource-policy` 和 `bootstrap`。服务器返回临时租约和 TOML，其中的运行 Token 不同于长期用户 Token。租约默认 24 小时，实际权限和有效期由服务器决定。

正常客户端心跳约每 10 秒一次。`QNetworkAccessManager` 管理 HTTPS 请求，`QProcess` 管理 FRPC；请求带会话代数，切换节点/退出后不会把旧响应当作新会话结果。

设备 ID 由 Windows MachineGuid 等机器标识计算带前缀的 SHA-256 摘要，不直接上报原始 MachineGuid。它用于设备识别，不是硬件级不可伪造的认证凭据。

### 故障和停止行为

短暂 HTTPS 请求失败不会由客户端直接停掉仍在运行的 FRPC。控制面异常时，当前服务端会拒绝新登录和新端口；已注册代理的客户端收到警告，端口全部关闭后再要求重新鉴权。

这不是断电或断网也能保持连接的保证。正常退出、管理员主动停止/封禁、租约失效或 FRP 连接本身故障仍可能中断隧道。

还有一个明确的限制：**目前“关闭单条隧道”会重建剩余 FRPC 配置，其他隧道可能短暂重连。** 尚未实现独立停止单个代理，故障期间也不能靠重建绕过新代理准入限制。

停止进程先调用 `terminate()`，约 3 秒未结束再强制终止。日志会记录停止来源，区分界面操作、配置重启、服务端命令和退出流程。

## Windows 外观支持

需要 Windows 10/11，具体可运行范围也取决于 Qt 和编译工具链。Windows 11 22H2 及以上在系统允许时尝试 Mica；Windows 10 或接口不可用时使用兼容实色背景。

`AppearanceController` 监听 Qt 主题变化和 Windows 设置消息，通过官方 DWM 接口调整标题栏、圆角和背景材质。浅色/深色即时生效，主题偏好通过 `QSettings` 保存。

最小窗口为 880×640，列表和表单可滚动。弹窗和控件使用短动画，保留系统标题栏的拖动、缩放及菜单行为。

## 从源码构建

需要 Qt 6.8+ 的 Core、Gui、Qml、Quick、QuickControls2、Network，CMake 3.16+ 和支持 C++17 的编译器。安装 Qt Test 后还可构建 UI 测试。下面用 Qt 6.11.1 / MinGW 的目录举例，请按实际安装修改。

在仓库根目录运行 PowerShell：

```powershell
$qtRoot = 'C:\Qt\6.11.1\mingw_64'
$env:Path = 'C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;' + $qtRoot + '\bin;' + $env:Path
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build-release -G Ninja "-DCMAKE_PREFIX_PATH=$qtRoot" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build-release --parallel 6
```

生成的源码构建程序是 `build-release/frp-control-client.exe`；发布目录中的 `MeowFRP_Client.exe` 是交付时使用的名称。

部署 Qt 运行依赖：

```powershell
& "$qtRoot\bin\windeployqt.exe" --release --qmldir .\qml .\build-release\frp-control-client.exe
```

FRPC 从配套服务端仓库的 `third_party/frp` 编译。在该目录下，Windows x86-64 环境可运行：

```powershell
$env:CGO_ENABLED = '0'
$env:GOOS = 'windows'
$env:GOARCH = 'amd64'
go build -trimpath -tags frpc,noweb -o frpc.exe ./cmd/frpc
```

把它放到客户端 EXE 旁。客户端优先寻找同目录的 `frpc.exe`，再寻找上级目录；找不到配套文件时才回落到保存的路径。请确认日志中的 `[PROCESS] launch program=...`，避免误用旧 FRPC。

可选 UI 测试：

```powershell
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build-release --output-on-failure
```

UI 测试检查主题、页面、滚动、导航和预览隔离，不是与真实节点建立隧道的端到端验证。

## 调试日志与本地文件

在登录页打开“调试模式”再登录。日志页可复制日志并查看当前文件路径；程序会将本次 FRPC 日志等级设为 `debug`。

| 标记 | 主要内容 |
| --- | --- |
| `[ACTION]` / `[AUTH]` / `[SESSION]` | 操作来源、鉴权阶段、会话切换 |
| `[HTTP #编号]` | 请求路径、状态码、耗时和脱敏摘要 |
| `[HEARTBEAT]` / `[COMMAND]` | 心跳结果、命令、去重和 ACK |
| `[PROCESS]` / `[CONFIG]` / `[LEASE]` | PID、配置路径、停止来源和租约释放 |
| `[FRPC stdout/stderr]` / `[QML]` | 原生 FRPC 日志和界面告警 |

HTTP 摘要使用字段白名单，已识别的 Token、密码和运行凭据会脱敏，不打印完整 TOML 或业务流量。但日志仍可能包含节点地址、文件路径、隧道名称和服务端提示，分享前仍要检查。

- 当前客户端配置保存 API、设备 ID、程序路径、节点选择和调试偏好，**不把用户 Token 写入配置文件**；Token 仅保留在当前进程内存。
- 生成的 `lease_*.toml` 含临时运行凭据，默认在 `%APPDATA%\frp-control\frp-control-client\runtime\`，不要公开分享。
- 调试日志通常在 `%LOCALAPPDATA%\frp-control\frp-control-client\logs\`，实际路径以日志页显示为准。
- 单个调试日志约 10 MiB 后轮转，保留当前和一个 `.previous` 分段；不同会话文件不自动全部清理。
- 界面日志最多保留约 10,000 行 / 2 MiB。“清空”清的是界面，不是磁盘文件。

如果旧版本已经在本地留下敏感配置，这一版“不再保存 Token”不代表旧文件自动被安全擦除。保护 Windows 用户目录，也别把 TOML、日志、抓包提交到仓库。

## 不连服务器也能看界面

```powershell
.\build-release\frp-control-client.exe --ui-preview dashboard --ui-theme dark
```

预览支持 `login`、`nodes`、`dashboard`、`logs`、`settings` 和 `system` / `light` / `dark` 主题。独立 `UiPreviewController` 使用演示数据，不读写真实业务配置和 Token、不发 HTTP、不启动 FRPC，也不保存主题偏好。

预览专用 `--ui-screenshot <文件>`、`--ui-report <文件>` 可输出 QML 截图和主题/材质状态。这些是 UI 检查工具，不是隧道测试工具。

## 项目结构

```text
app/src/app_controller.*          登录、节点、隧道与命令协调
app/src/control_api_client.*      HTTPS API、会话代数和诊断
app/src/tunnel_runtime_service.*  FRPC 进程、配置、停止与租约
app/src/profile_service.*        本地偏好和设备标识
app/src/appearance_controller.*   Windows 主题与 DWM 材质
app/src/log_safety.h              敏感字段与已知凭据脱敏
app/src/ui_preview_controller.h   无网络隔离预览
qml/Main.qml                     Qt Quick 界面
tests/ui_test.cpp                UI 回归测试
```

继续看：[调试日志说明](./docs/DEBUG_LOGGING_CN.md)、[现代界面与兼容边界](./docs/MODERN_UI_CN.md)。节点同步、DPI 执行和故障准入由服务端负责，详情在服务端仓库。

## 许可证

客户端采用 [GNU AGPL v3](./LICENSE)。配套 FRPC 保留 FRP 的 Apache 2.0 许可；Qt 及其他依赖按各自许可证分发。请保留发布包中的相关许可文件。
