# MeowFRP 客户端

[English](./README.md)

MeowFRP 客户端是 **MeowFRP Server** 的 Qt 桌面客户端。它通过 HTTPS 与服务端完成身份认证，获取用户资源策略和短期 FRP 凭证，并负责管理本地 `frpc` 隧道进程。

配套服务端项目：`MeowFRP_server`

## 主要功能

- 使用 Qt Quick 和 QML 构建的现代化中文界面
- 根据 Windows MachineGuid 生成设备标识，并使用 SHA-256 处理
- 自动记忆 API 地址和用户访问令牌
- 根据服务端返回结果限制端口范围、隧道数量和可用协议
- 单次会话支持创建和管理多个 TCP 或 UDP 隧道
- 通过 HTTPS 获取短期 FRP 令牌和最终配置文件
- 管理本地 `frpc` 的启动、停止和实时日志
- 一键复制远程隧道地址
- 显示当前 DPI 状态以及被阻断的流量类型
- 每十秒发送一次 HTTPS 心跳，用于维持在线状态并接收服务端命令
- 支持服务端远程停止 FRP、显示违规提示和要求重新鉴权
- 正常退出或重新鉴权时主动通知服务端下线

## 工作流程

1. 用户输入完整的 MeowFRP 服务端 API 基础地址和访问令牌。
2. 客户端请求 `/api/v1/client/resource-policy` 获取资源策略。
3. 服务端返回 FRP 地址、允许协议、端口范围、隧道数量限制和 DPI 状态。
4. 用户在服务端规定的权限范围内创建隧道。
5. 客户端把隧道列表提交到 `/api/v1/client/bootstrap`。
6. 服务端生成包含短期运行令牌的 `frpc` 配置并返回。
7. 客户端把配置写入运行目录，然后启动 `frpc`。
8. 鉴权有效期间，客户端每十秒发送一次心跳并执行服务端排队下发的命令。

用户长期使用的 HTTPS 访问令牌不会被直接用作 FRP 认证令牌。

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

1. 启动或部署 `MeowFRP_server`。
2. 在服务端 Web 面板中创建普通用户，并复制自动生成的 HTTPS API 令牌。
3. 把 `frpc.exe` 放到客户端可以找到的位置，或者在客户端设置中选择其路径。
4. 启动 MeowFRP 客户端，输入完整的服务端 API 基础地址和用户令牌。客户端不会自动添加 `/api` 前缀。
5. 鉴权成功后，在服务端返回的权限范围内添加隧道并启动 FRP。

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
- 服务端签发的 FRP 凭证是临时凭证，不应重复使用。

## 许可证

请参阅 [LICENSE](./LICENSE)。
