# MeowFRP Client

[中文说明](./README_CN.md) · [Companion server](https://github.com/QWEOVO123/MeowFRP_Server)

This is the Windows desktop client for MeowFRP. Enter your controller URL and user token, choose an authorized node, and specify the local service you want to expose. The server supplies the configuration; the client runs FRPC, shows its logs and handles server commands.

The interface uses Qt Quick/QML over C++ authentication, HTTP session and process-management services. A running process, a successful FRP login and a registered tunnel are separate states; the logs help you tell them apart.

## Everyday features

- Blue login cards, authorized-node selection, and a sidebar tunnel/log/settings workspace.
- Windows system, light and dark themes with immediate switching.
- Native Mica, dark title bars and rounded corners when supported, with an opaque fallback.
- Server-enforced nodes, ports, protocols and tunnel limits; TCP/UDP tunnel configuration.
- Effective DPI policy display, copyable endpoints and FRPC output.
- HTTPS heartbeats, remote notices/stop/reauthentication commands and execution ACKs.
- A login-page debug switch for detailed HTTP, session, command and process diagnostics.

Mica is a Windows background material, not a promise of arbitrary live Gaussian blur behind every window. System transparency and high-contrast settings are respected.

## Before you start

You need an initialized MeowFRP controller, an administrator-generated regular-user token, and assigned node/resource permissions.

Keep the complete portable client directory: executable, Qt DLLs, QML/plugins and matching `frpc.exe`. Copying only the executable may prevent startup.

Use FRPC built from the matching server's adapted source. The current FRP version identifier is 0.69.1; arbitrary older binaries are not guaranteed drop-in replacements.

## Sign in and create a tunnel

1. Enter the complete controller API URL, such as `https://frp.example.com/api`. No real endpoint is preset; include the supplied `/api` or other base path.
2. Enter the user token, not the web administrator password.
3. Select “Sign in and choose node”. Authentication happens at the controller before the authorized directory is returned.
4. Choose an online node. The client requests that node's effective policy directly.
5. Add a local address/port, remote port and protocol, then start tunnels.
6. Confirm `login to server success` and `start proxy success` in FRPC output.

For `127.0.0.1:25565` mapped to remote port 25565, visitors connect to the node's public address on port 25565. A local service must actually listen on the target, and the node's firewall/security group must allow the remote port.

**“FRPC process started” and HTTP heartbeat 200 do not prove tunnel success.** Repeated EOF is a transport/login problem to investigate; a local-service refusal after successful registration points to the local target.

## Technical flow

```text
Client ── HTTPS + user token ── Controller: identity and authorized nodes
   │
   ├── HTTPS ── Selected node: policy, bootstrap, heartbeat, command ACK
   └── FRPC ─── Selected node FRPS: registration and traffic

Visitor → node tunnel port → FRP work connection → local target
```

Controller authentication uses `POST /api/v1/client/login`. Node selection is followed by `resource-policy` and `bootstrap`. The returned TOML contains a temporary runtime token distinct from the long-lived API token. Leases normally last 24 hours; actual limits and validity are server-controlled.

Client heartbeats normally run every ten seconds. `QNetworkAccessManager` handles HTTPS and `QProcess` owns FRPC. Request generations prevent stale responses from becoming results for a new session.

Device IDs are prefixed SHA-256 digests derived from Windows MachineGuid or fallback machine identifiers. The raw MachineGuid is not sent as the device ID; this is device identification, not tamper-proof hardware authentication.

### Faults and stopping

A transient HTTPS failure does not directly stop a running FRPC. Current server fault handling refuses new authentication/proxies, warns clients with existing registered proxies, and can require reauthentication once all proxies close.

This does not preserve tunnels through power loss or transport failure. Logout, explicit administrator actions, invalid leases and FRP failures can still interrupt them.

**Stopping one tunnel currently rebuilds the remaining FRPC configuration.** Other tunnels may briefly reconnect; independent per-proxy stopping has not been implemented. Rebuilding also cannot bypass fault admission gates.

Process shutdown uses `terminate()` followed by forced termination after roughly three seconds. Stop-source diagnostics distinguish UI actions, configuration replacement, server commands and logout.

## Windows appearance

Windows 10/11 operation depends on the selected Qt/toolchain support. Windows 11 22H2+ attempts Mica when available; older/unsupported environments fall back to opaque backgrounds.

`AppearanceController` observes Qt theme changes and Windows setting messages, uses official DWM attributes, and saves theme preferences through `QSettings`. System transparency and high contrast take precedence. The minimum window is 880×640, with scrollable forms/lists, short animations and native window controls.

## Build from source

Requirements: Qt 6.8+ Core, Gui, Qml, Quick, QuickControls2 and Network; CMake 3.16+; a C++17 compiler. Qt Test enables the optional UI test target. Adjust these example Qt 6.11.1/MinGW paths to your installation.

From the repository root in PowerShell:

```powershell
$qtRoot = 'C:\Qt\6.11.1\mingw_64'
$env:Path = 'C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;' + $qtRoot + '\bin;' + $env:Path
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build-release -G Ninja "-DCMAKE_PREFIX_PATH=$qtRoot" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build-release --parallel 6
```

The source target produces `build-release/frp-control-client.exe`; delivered packages may name it `MeowFRP_Client.exe`.

Deploy Qt dependencies:

```powershell
& "$qtRoot\bin\windeployqt.exe" --release --qmldir .\qml .\build-release\frp-control-client.exe
```

Build the matching FRPC from `third_party/frp` in the companion server repository. In a Windows x86-64 shell:

```powershell
$env:CGO_ENABLED = '0'
$env:GOOS = 'windows'
$env:GOARCH = 'amd64'
go build -trimpath -tags frpc,noweb -o frpc.exe ./cmd/frpc
```

Place it beside the client executable. Discovery prefers adjacent `frpc.exe`, then the parent directory, falling back to the saved path when bundled files are absent. Check `[PROCESS] launch program=...` if you suspect an older binary is being used.

Optional UI checks:

```powershell
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build-release --output-on-failure
```

These cover theme/page behavior, scrolling, navigation and preview isolation, not end-to-end tunnels to live nodes.

## Debugging and local data

Enable Debug mode on the login page before signing in. The log page offers copying and the active file path. Debug mode sets the generated FRPC log level to `debug`.

| Marker | What it helps diagnose |
| --- | --- |
| `[ACTION]`, `[AUTH]`, `[SESSION]` | Action source, authentication and session changes |
| `[HTTP #id]` | Endpoint, status, timing and redacted summaries |
| `[HEARTBEAT]`, `[COMMAND]` | Heartbeats, deduplication and command ACKs |
| `[PROCESS]`, `[CONFIG]`, `[LEASE]` | PID, config path, stop source and lease release |
| `[FRPC stdout/stderr]`, `[QML]` | Native FRPC output and UI warnings |

HTTP summaries use a field allowlist and known credentials are redacted. Full TOML and application traffic are not printed. Logs can still reveal node addresses, paths, tunnel names and server messages; review before sharing.

- Current profiles save endpoint, device ID, paths, node choice and debug preferences, **not the user token**. The token stays in process memory.
- Generated `lease_*.toml` contains temporary credentials, normally under `%APPDATA%\frp-control\frp-control-client\runtime\`.
- Debug files normally use `%LOCALAPPDATA%\frp-control\frp-control-client\logs\`; the displayed path is authoritative.
- Files rotate around 10 MiB with one `.previous` segment. Files from different sessions are not all automatically deleted.
- The UI retains roughly 10,000 lines / 2 MiB. Clearing the view does not erase disk logs.

Not saving a token in the current version does not securely erase files created by older versions. Protect the Windows profile and never publish runtime TOML, captures or sensitive logs.

## Preview without a server

```powershell
.\build-release\frp-control-client.exe --ui-preview dashboard --ui-theme dark
```

Pages: `login`, `nodes`, `dashboard`, `logs`, `settings`. Themes: `system`, `light`, `dark`. The independent `UiPreviewController` uses demo data without real profile/token access, HTTP, FRPC or persisted theme changes.

Preview-only `--ui-screenshot <file>` and `--ui-report <file>` produce QML screenshots and theme/material diagnostics. They validate UI rendering, not tunnel connectivity.

## Source map

```text
app/src/app_controller.*          Authentication, nodes, tunnels and commands
app/src/control_api_client.*      HTTPS, request generations and diagnostics
app/src/tunnel_runtime_service.*  FRPC, configurations, stopping and leases
app/src/profile_service.*        Preferences and device identity
app/src/appearance_controller.*   Windows theme and DWM material
app/src/log_safety.h              Sensitive-data redaction
app/src/ui_preview_controller.h   Isolated, offline UI preview
qml/Main.qml                     Qt Quick interface
tests/ui_test.cpp                UI regression tests
```

Further notes: [debug logging](./docs/DEBUG_LOGGING_CN.md), [modern UI and compatibility](./docs/MODERN_UI_CN.md). Identity synchronization, DPI enforcement and fault admission belong to the server.

## License

Client: [GNU AGPL v3](./LICENSE). The matching FRPC retains FRP's Apache 2.0 license; Qt and other dependencies have their own terms. Keep the applicable notices with distributed packages.
