# MeowFRP_client

MeowFRP 的 Qt 桌面客户端，配套服务端项目为 `MeowFRP_server`。

第一版客户端已经按中文桌面客户端流程实现：首次启动自动生成 `client_id`，并写入运行目录下的 `frp-control-client.cfg.json`。用户只需要填写 API 地址和用户 token，客户端会自动连接服务器获取端口数量、协议权限、frps 地址和 DPI 状态。

This client is responsible for:

- storing the user's HTTPS API token locally;
- asking the control server for allowed resources;
- letting the user choose server-side ports inside the allowed range;
- requesting a runtime frpc config from the server;
- starting and supervising the local frpc process.

## Recommended Stack

- UI: Qt 6 Widgets first.
- Language: C++17.
- Build: CMake.
- HTTP: `QNetworkAccessManager`.
- Local storage: encrypted credential storage later, JSON settings first.
- Process control: `QProcess` to launch `frpc`.

Qt Widgets is the recommended first step because this app is more like a small control panel than a visual-heavy product. It is stable, simple to package, and quick to debug on Windows. QML can still be added later if the UI needs animation or a more mobile-like surface.

## First Milestone

Build a minimal but real client:

1. User enters control server API URL and HTTPS API token.
2. Client generates or loads a persistent `client_id`.
3. Client calls `/api/v1/client/resource-policy`.
4. UI shows allowed port range, max port count, allowed protocols, and frps endpoint.
5. User creates one TCP or UDP tunnel.
6. Client calls `/api/v1/client/bootstrap`.
7. Client writes the returned `frpc_config` to a runtime file.
8. Client starts `frpc` with that runtime config and shows status/logs.

## Folder Plan

```text
client/
  CMakeLists.txt
  README.md
  app/
    src/
  docs/
    architecture.md
  third_party/         # optional local frpc/frp helpers if needed
  runtime/             # ignored generated configs/logs in dev
```

## Current Demo Status

The first Qt Widgets implementation is now under `app/src`.

Implemented:

- profile loading/saving;
- first-run client ID generation and `frp-control-client.cfg.json` persistence;
- Chinese Qt Widgets UI with a modern light style;
- API base URL and HTTPS API token as the main required inputs;
- advanced frpc path and runtime directory settings;
- `/api/v1/client/resource-policy`;
- display of port range, allowed protocols, frps endpoint, and DPI status;
- one TCP/UDP tunnel draft;
- `/api/v1/client/bootstrap`;
- writing returned `frpc_config` to a runtime TOML file;
- starting/stopping `frpc` with `QProcess`;
- stdout/stderr log panel.

## Build

Open this folder with Qt Creator as a CMake project, or build from a shell where Qt and CMake are in `PATH`:

```powershell
cd D:\Qt_test\client
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

If Qt is not discoverable automatically, pass `CMAKE_PREFIX_PATH`, for example:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.8.0\msvc2022_64
```

In a plain PowerShell, make sure the Qt tools are in `PATH` first. This machine's Qt Creator kit uses:

```powershell
$env:Path = 'C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;' + $env:Path
```

## Local Test Flow

1. Start `frp-control-server` on `http://127.0.0.1:8080`.
2. Create a normal `user` in the web panel and copy the user's `ak_` HTTPS API token.
3. Run this client.
4. Enter:
   - API base URL: `http://127.0.0.1:8080`
   - HTTPS API token: copied `ak_...`
5. The client automatically queries the server for permissions.
6. Choose a server remote port inside the returned range.
7. Click `创建隧道并启动 FRP`.

The default frpc path is:

```text
D:/Qt_test/frp_0.69.1_windows_amd64/frpc.exe
```

## Current Server API Assumptions

The client talks only to the HTTPS API at first. It does not know the frps address or frps port before the server returns them.

- `POST /api/v1/client/resource-policy`
- `POST /api/v1/client/bootstrap`

The server returns a temporary runtime frp token in the generated config. The user's long-lived HTTPS API token is never passed directly to frps.
