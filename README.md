# MeowFRP Client

[中文说明](./README_CN.md)

MeowFRP Client is the Qt desktop client for **MeowFRP Server**. It authenticates with the server over HTTPS, retrieves the user's resource policy, requests short-lived FRP credentials, and manages local `frpc` tunnel processes.

Companion server project: `MeowFRP_server`

## Features

- Modern Chinese desktop interface built with Qt Quick and QML
- Automatic device ID derived from the Windows MachineGuid and protected with SHA-256
- Persistent API address and access-token settings
- Server-controlled port ranges, tunnel count limits, and protocol permissions
- Multiple TCP or UDP tunnels in one client session
- Short-lived FRP token and configuration retrieval over HTTPS
- Local `frpc` lifecycle management and real-time logs
- Copyable public tunnel endpoints
- DPI policy and blocked-traffic status display
- Ten-second HTTPS heartbeat for server-side client presence and commands
- Remote commands for stopping FRP, displaying a warning, or requiring reauthentication
- Explicit logout notification on normal exit or reauthentication

## How It Works

1. The user enters the MeowFRP Server API URL and their access token.
2. The client requests the resource policy from `/api/v1/client/resource-policy`.
3. The server returns the FRP endpoint, allowed protocols, port range, tunnel limit, and DPI status.
4. The user creates tunnels within those server-defined limits.
5. The client submits the tunnel list to `/api/v1/client/bootstrap`.
6. The server returns a generated `frpc` configuration with a short-lived runtime token.
7. The client writes the configuration to its runtime directory and starts `frpc`.
8. While authenticated, the client sends a heartbeat every ten seconds and processes queued server commands.

The long-lived HTTPS access token is never used directly as the FRP authentication token.

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

1. Start or deploy `MeowFRP_server`.
2. Create a regular user in the server's web panel and copy the generated HTTPS API token.
3. Place `frpc.exe` where the client can locate it, or select its path in the client settings.
4. Start MeowFRP Client and enter the server API URL and user token.
5. After authentication succeeds, add tunnels within the permissions returned by the server and start FRP.

The default public service API used by packaged builds is:

```text
https://relay.qweovo.top
```

For local development, a typical API URL is `http://127.0.0.1:8080`.

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
- Server-issued FRP credentials are temporary and should not be reused.

## License

See [LICENSE](./LICENSE).
