# MeowFRP Client

[简体中文](./README_CN.md) | [Server repository](https://github.com/QWEOVO123/MeowFRP_Server)

MeowFRP Client is the Windows desktop component of MeowFRP. It authenticates users with the controller, selects authorized nodes, requests server-generated tunnel configurations, and supervises the adapted FRPC process.

The interface uses Qt Quick/QML. C++ services handle HTTPS requests, session state, FRPC lifecycle, local preferences, and diagnostic logging.

## Requirements and scope

- An initialized MeowFRP controller and a generated user token.
- Explicit node access and resource permissions assigned by the administrator.
- A client-reachable HTTPS API for the controller and each selected node.
- A matching server and adapted FRPC build.
- Windows 10/11 within the support range of the selected Qt/compiler toolchain.

Only **TCP + verified TLS 1.3** is supported for FRPC–FRPS transport. Plaintext, KCP, QUIC, and WebSocket transports are not supported by the delivered application. Both TCP and UDP proxy services remain supported over this transport.

The current adapted FRP version identifier is 0.69.1. An upstream or historical `frpc.exe` is not a supported drop-in replacement: the client relies on the adapted HTTPS trust retrieval and runtime authorization flow.

## Architecture and access flow

~~~text
Qt client -- HTTPS/user token --> Controller: identity and permitted node directory
    |
    +-- HTTPS -----------------> Selected node: policy, bootstrap, heartbeat, ACK
    +-- QProcess --------------> Adapted FRPC
                                     |
                                     +-- TCP + TLS 1.3 --> Selected node FRPS

Visitor --> Node proxy port --> FRPS --> FRPC --> Local service
~~~

1. Submit the user token and device ID to the controller's `POST /api/v1/client/login`.
2. Select a node from the authorized directory.
3. Query `GET /api/v1/client/frp-transport`, then request the selected node's effective `resource-policy`.
4. Define local address/port, remote port, and TCP/UDP proxy type.
5. Request `POST /api/v1/client/bootstrap` to validate resources and obtain a temporary runtime lease and TOML.
6. Start FRPC, retrieve node trust, complete TLS and FRP login, and register allocated proxies.
7. Send client heartbeats and acknowledge remote commands.

The user API token and FRP runtime token have separate scopes. Runtime leases normally last 24 hours; server configuration determines actual validity and resource limits. The web administrator password is not a desktop user credential.

`QNetworkAccessManager` handles API requests. Request generations prevent obsolete responses from being accepted after logout or node/session changes. Client heartbeats normally run every 10 seconds.

## TLS trust and key negotiation

### Separate trust domains

| Connection | Trust |
| --- | --- |
| Qt client → HTTPS API | Normal HTTPS certificate validation using platform trust |
| FRPC → HTTPS trust endpoint | Normal HTTPS with system roots; redirects are rejected |
| FRPC → FRPS | An isolated certificate pool containing only the selected node's downloaded CA |
| Edge → controller | Server-side management mTLS; independent of desktop FRP trust |

The ordinary login flow queries transport status but does not download the FRP root. At tunnel startup, Qt passes the selected node's trust API URL to the adapted FRPC using `--trust-api`.

Before each physical FRPC connection or reconnection, FRPC fetches `GET /api/v1/client/frp-trust`. It validates the expected node ID, enabled status, certificate metadata, and current self-signed root, then constructs an empty trust pool containing that root only. It validates the FRPS certificate and `server_name` without accepting other system roots for FRP.

The FRP root is connection-local memory. It is not persisted in client configuration, runtime TOML, temporary certificate files, or the Windows trust store. The client does not configure an FRP TLS session cache; the server disables tickets. TCP multiplexing can reuse an existing TLS connection without a new physical handshake.

The API supplies a public trust anchor. TLS 1.3 performs identity verification and session-key negotiation; private keys and traffic keys are not downloaded from the API. Certificate/API errors and `enabled:false` reject the connection without plaintext fallback. Resetting a node identity can require a new trust fetch and retry.

TLS encrypts the FRPC–FRPS segment. Visitor-to-node and client-to-local-service encryption depend on the application protocol.

### Startup progress

The tunnel-start interface derives progress from API results and FRPC events:

| Stage | Completion signal |
| --- | --- |
| Configuration authorization | Successful bootstrap response |
| TLS trust retrieval | Adapted FRPC trust-fetch event |
| FRP login | `login to server success` |
| Proxy registration | All configured proxies report `start proxy success` |

Process startup and HTTP heartbeat 200 do not prove tunnel availability. Errors remain visible at the corresponding stage. Reconnection starts a new attempt; routine work connections do not reset completed startup progress.

## Usage

1. Enter the complete controller API URL, for example `https://frp.example.com/api`.
2. Enter the generated regular-user token.
3. Authenticate and choose an authorized online node.
4. Add a tunnel with local address/port, remote port, and protocol.
5. Start tunnels and verify FRP login and proxy-registration results.

For a tunnel mapping `127.0.0.1:25565` to remote port `25565`, visitors use the selected node's public address and port 25565. A local service must listen on the target address, and the node's firewall/security group must permit the remote port.

Node visibility does not imply resource availability. Server-side node pools, user policy, token grants, active leases, and fault admission gates still apply.

## Runtime and fault behavior

`QProcess` owns FRPC. Shutdown first calls `terminate()` and forces termination after approximately 3 seconds if needed. Diagnostics record stop sources, including UI actions, configuration replacement, server commands, and logout.

A transient API request failure does not directly stop an established FRPC process. On control-plane faults, server admission gates reject new logins/proxies, warn clients with registered proxies, and can require reauthentication once their registered proxy count reaches zero. Commands are deduplicated and acknowledged.

**Stopping a single tunnel currently rebuilds the remaining FRPC configuration.** Other tunnels may briefly reconnect. Independent per-proxy stopping is not implemented, and rebuilding cannot bypass server fault gates.

Process restart, transport failure, explicit logout, invalid leases, bans, and administrator actions can interrupt tunnels. A running process flag does not establish that a proxy is registered or that its local target is reachable.

## Local state and diagnostics

| Data | Handling |
| --- | --- |
| User token | Current-process memory; not saved in the current profile format |
| Preferences | API URL, device ID, paths, node selection, and debug preferences |
| FRP root | Connection-local memory; fetched again for new physical connections |
| Runtime TOML | Contains temporary runtime credentials; normally under `%APPDATA%\frp-control\frp-control-client\runtime\` |
| Debug logs | Normally `%LOCALAPPDATA%\frp-control\frp-control-client\logs\`; the UI displays the active path |

Device IDs use a prefixed SHA-256 digest derived from Windows MachineGuid or fallback identifiers. The raw MachineGuid is not submitted as the device ID. This is device identification, not tamper-proof hardware authentication.

Enable **Debug mode** on the login page before signing in. It enables detailed diagnostics and sets FRPC logging to `debug`.

| Marker | Diagnostic content |
| --- | --- |
| `[ACTION]`, `[AUTH]`, `[SESSION]` | Action source, authentication, and session transitions |
| `[HTTP #id]` | Request endpoint, status, timing, and redacted summaries |
| `[HEARTBEAT]`, `[COMMAND]` | Heartbeats, command processing, and ACKs |
| `[PROCESS]`, `[CONFIG]`, `[LEASE]` | PID, config paths, stop source, and lease release |
| `[FRPC stdout/stderr]`, `[QML]` | FRPC output and interface diagnostics |

HTTP summaries use a field allowlist and redact recognized credentials. Logs may still expose node addresses, paths, tunnel names, and server messages. Runtime TOML must be treated as credential-bearing data.

Debug files rotate at approximately 10 MiB and keep one `.previous` segment; session files are not all automatically removed. The UI retains approximately 10,000 lines / 2 MiB. Clearing the view does not delete disk logs. Current token handling does not securely erase files left by earlier versions.

## Windows appearance

The client supports system/light/dark themes and observes Qt and Windows theme changes. `AppearanceController` uses DWM attributes for supported title bars, corners, and background material.

Windows 11 22H2+ attempts Mica when supported and permitted by system settings. Other environments use an opaque fallback. System transparency and high-contrast settings take precedence. The minimum window size is 880×640, with scrollable forms and lists.

## Build and package

Requirements: Qt 6.8+ with Core, Gui, Qml, Quick, QuickControls2, and Network; CMake 3.16+; a C++17 compiler. Qt Test is required for the optional UI test target.

Example PowerShell build using Qt 6.11.1/MinGW; adjust paths to the installed kit:

~~~powershell
$qtRoot = 'C:\Qt\6.11.1\mingw_64'
$env:Path = 'C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;' + $qtRoot + '\bin;' + $env:Path
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build-release -G Ninja "-DCMAKE_PREFIX_PATH=$qtRoot" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
& 'C:\Qt\Tools\CMake_64\bin\cmake.exe' --build build-release --parallel 6
~~~

The source target produces `build-release/frp-control-client.exe`.

Build matching FRPC from `third_party/frp` in the server repository:

~~~powershell
$env:CGO_ENABLED = '0'
$env:GOOS = 'windows'
$env:GOARCH = 'amd64'
go build -trimpath -tags frpc,noweb -o frpc.exe ./cmd/frpc
~~~

From the client repository, package into a new directory:

~~~powershell
.\scripts\package-windows.ps1 `
  -BuildDir .\build-release `
  -OutputDir C:\packages\MeowFRP-client `
  -FrpcPath ..\MeowFRP_Server\third_party\frp\frpc.exe `
  -FrpLicensePath ..\MeowFRP_Server\third_party\frp\LICENSE `
  -QtRoot C:\Qt\6.11.1\mingw_64
~~~

The script rejects an existing output directory. It names the distributed executable `MeowFRP_Client.exe`, invokes `windeployqt`, and includes matching FRPC and license notices. Packaging limits unused styles, translations, and tooling while retaining graphics fallback.

Distribute the complete package directory. FRPC discovery prefers the executable directory, then its parent, then the configured path. The `[PROCESS] launch program=...` log identifies the binary actually started.

## Validation and offline preview

~~~powershell
& 'C:\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build-release --output-on-failure
.\build-release\frp-control-client.exe --ui-preview dashboard --ui-theme dark
~~~

Preview pages: `login`, `nodes`, `dashboard`, `logs`, `settings`. Themes: `system`, `light`, `dark`. `UiPreviewController` uses isolated demo data without accessing real profiles/tokens, issuing HTTP requests, starting FRPC, or persisting theme changes.

Preview-only `--ui-screenshot <file>` and `--ui-report <file>` generate screenshots and appearance diagnostics. UI tests and preview do not validate live node authentication, end-to-end tunnels, or production network compatibility.

## Source map

| Path | Responsibility |
| --- | --- |
| `app/src/app_controller.*` | Authentication, node/tunnel state, heartbeats, and commands |
| `app/src/control_api_client.*` | HTTPS API, request generations, and diagnostics |
| `app/src/tunnel_runtime_service.*` | FRPC process, runtime config, trust arguments, and shutdown |
| `app/src/tunnel_startup_progress.h` | Event-driven tunnel startup stages |
| `app/src/profile_service.*` | Preferences and device identity |
| `app/src/appearance_controller.*` | Windows theme and DWM integration |
| `app/src/log_safety.h` | Credential redaction |
| `app/src/ui_preview_controller.h` | Isolated preview model |
| `qml/Main.qml` | Qt Quick interface |
| `scripts/package-windows.ps1` | Windows runtime packaging |
| `tests/ui_test.cpp` | UI regression tests |

The FRPC trust implementation resides in the server repository at `third_party/frp/pkg/transport/memory_trust.go` and `third_party/frp/client/connector.go`. Node identity synchronization and DPI enforcement are server responsibilities.

Additional documentation: [Debug logging](./docs/DEBUG_LOGGING_CN.md), [UI implementation](./docs/MODERN_UI_CN.md).

## License

Client: [GNU AGPL v3](./LICENSE). The adapted FRPC retains FRP's Apache 2.0 license. Qt and other dependencies retain their respective license requirements. Preserve applicable notices in distributed packages.
