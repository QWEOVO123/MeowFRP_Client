# MeowFRP Client

[中文说明](./README_CN.md)

MeowFRP Client is the Qt desktop client for **MeowFRP Server**. It discovers Controller and Edge nodes, authenticates directly with the selected node over HTTPS, retrieves the user's resource policy and FRP lease, and manages local `frpc` tunnel processes.

Companion server project: [`MeowFRP_Server`](https://github.com/QWEOVO123/MeowFRP_Server)

## Features

- Modern Chinese desktop interface built with Qt Quick and QML
- Automatic device ID derived from the Windows MachineGuid and protected with SHA-256
- Persistent API address and access-token settings
- Public Controller node directory with explicit Controller/Edge selection
- Direct long-lived-token authentication to the selected Edge, without proxying client traffic through the Controller
- Server-controlled port ranges, tunnel count limits, and protocol permissions
- Multiple TCP or UDP tunnels in one client session
- FRP configuration and 24-hour runtime lease retrieval over HTTPS
- Local `frpc` lifecycle management and real-time logs
- Copyable public tunnel endpoints
- DPI policy and blocked-traffic status display
- Ten-second HTTPS heartbeat for server-side client presence and commands
- Remote commands for stopping FRP, displaying a warning, or requiring reauthentication, with execution ACKs returned over HTTPS
- Explicit logout notification on normal exit or reauthentication
- Existing FRP tunnels remain active during an Edge/Controller outage; new logins are rejected and connected users receive a readable warning dialog

## How It Works

1. The user enters the Controller API base URL. The client downloads the unauthenticated node directory from `/api/v1/public/nodes`.
2. The user selects the Controller or an available Edge and enters their long-lived access token.
3. The client sends the token and device ID directly to the selected node and requests `/api/v1/client/resource-policy`.
4. The selected node returns its FRP endpoint, allowed protocols, port range, tunnel limit, and DPI status.
5. The user creates tunnels within those node-defined limits and submits them to `/api/v1/client/bootstrap`.
6. The selected node returns a generated `frpc` configuration and a runtime lease that is valid for 24 hours by default.
7. The client writes the configuration to its runtime directory and starts `frpc`.
8. While authenticated, the client sends a heartbeat every ten seconds, executes queued commands, and acknowledges successful execution through the selected node's HTTPS API.

The long-lived HTTPS access token is never used directly as the FRP authentication token. If an Edge loses its Controller connection, existing FRP tunnels continue to run, while new policy/bootstrap requests fail with `edge_controller_disconnected` and the client displays the outage warning once.

## Requirements

- Windows 10 or later
- Qt 6 with Core, Gui, Qml, Quick, QuickControls2, and Network modules
- CMake 3.16 or later
- A C++17 compiler supported by Qt
- `frpc.exe` from a compatible FRP release
- A running `MeowFRP_server` instance

## Build

Open the repository in Qt Creator as a CMake project, or build it from a shell:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\mingw_64
cmake --build build --config Release -j 6
```

When using MinGW from a plain PowerShell session, add the Qt and compiler tools to `PATH` first:

```powershell
$env:Path = 'C:\Qt\Tools\mingw1310_64\bin;C:\Qt\6.11.1\mingw_64\bin;' + $env:Path
```

## Run

1. Start or deploy a MeowFRP Controller and configure the public API URL for each selectable node in its web panel.
2. Create a regular user in the Controller panel and copy the generated HTTPS API token.
3. Place `frpc.exe` where the client can locate it, or select its path in the client settings.
4. Start MeowFRP Client and enter the complete Controller API base URL. The client does not add an `/api` prefix automatically.
5. Refresh the node directory, select a node, enter the user token, and sign in. The token is sent directly to the selected node.
6. Add tunnels within the permissions returned by that node and start FRP.

The API field is empty on first launch. For local development, a typical API base URL is `http://127.0.0.1:8080/api`. Include any reverse-proxy path such as `/api` yourself.

## Project Structure

```text
app/src/              C++ application, API, profile, and runtime services
app/assets/           Application icons and Windows resources
qml/Main.qml          Qt Quick user interface
docs/architecture.md  Client architecture and API contracts
packaging/             Windows launcher source
tools/                 Development utilities
```

Runtime configuration and generated FRP files are stored outside the source tree and are excluded from Git.

## Security Notes

- Treat the user's HTTPS API token as a secret.
- Production deployments should expose the control API only through HTTPS.
- The local token is currently persisted for automatic sign-in; protect the Windows user profile accordingly.
- Server-issued FRP leases are temporary and are revoked immediately after logout or heartbeat timeout.

## License

See [LICENSE](./LICENSE).
