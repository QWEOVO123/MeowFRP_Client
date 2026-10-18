param(
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [Parameter(Mandatory=$true)][string]$FrpcPath,
    [Parameter(Mandatory=$true)][string]$FrpLicensePath,
    [string]$BuildDir = 'build-release-20261016',
    [string]$QtRoot = 'C:\Qt\6.11.1\mingw_64',
    [switch]$IncludeOffscreen
)
$ErrorActionPreference = 'Stop'
$taskRepo = Split-Path -Parent $PSScriptRoot
if (![IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $taskRepo $BuildDir }
$taskDestination = [IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $taskDestination) { throw 'Output already exists; choose a new folder. Nothing was overwritten.' }
foreach ($taskRequired in @((Join-Path $BuildDir 'frp-control-client.exe'), $FrpcPath, $FrpLicensePath, (Join-Path $QtRoot 'bin\windeployqt.exe'))) {
    if (!(Test-Path -LiteralPath $taskRequired -PathType Leaf)) { throw "Required input missing: $taskRequired" }
}
New-Item -ItemType Directory -Path $taskDestination | Out-Null
Copy-Item -LiteralPath (Join-Path $BuildDir 'frp-control-client.exe') -Destination (Join-Path $taskDestination 'MeowFRP_Client.exe')
Copy-Item -LiteralPath $FrpcPath -Destination (Join-Path $taskDestination 'frpc.exe')
# Main.qml imports Controls.Basic directly: the scanner need not deploy every style.
# Keep software OpenGL fallback for older GPUs; remove tooling, unused generic
# plugins, and unused language packs, not Windows TLS/graphics functionality.
$taskOldPath = $env:PATH
try {
    $taskQtTop = [IO.Path]::GetFullPath((Join-Path $QtRoot '..\..'))
    $env:PATH = (Join-Path $QtRoot 'bin') + ';' + (Join-Path $taskQtTop 'Tools\mingw1310_64\bin') + ';' + $env:PATH
    & (Join-Path $QtRoot 'bin\windeployqt.exe') --release --verbose 0 --compiler-runtime --translations zh_CN,en --skip-plugin-types qmltooling,generic --qmldir (Join-Path $taskRepo 'qml') --dir $taskDestination (Join-Path $taskDestination 'MeowFRP_Client.exe')
    if ($LASTEXITCODE -ne 0) { throw "Qt deployment failed ($LASTEXITCODE)" }
    if ($IncludeOffscreen) {
        Copy-Item -LiteralPath (Join-Path $QtRoot 'plugins\platforms\qoffscreen.dll') -Destination (Join-Path $taskDestination 'platforms\qoffscreen.dll')
    }
    Copy-Item -LiteralPath (Join-Path $taskRepo 'app\assets\qt.conf') -Destination (Join-Path $taskDestination 'qt.conf')
    Copy-Item -LiteralPath (Join-Path $taskRepo 'LICENSE') -Destination (Join-Path $taskDestination 'LICENSE')
    Copy-Item -LiteralPath $FrpLicensePath -Destination (Join-Path $taskDestination 'FRP-LICENSE')
    $taskLicenses = Join-Path $taskDestination 'LICENSES\Qt'
    New-Item -ItemType Directory -Path $taskLicenses | Out-Null
    foreach ($taskNotice in @('COPYING.txt','Copyright.txt','LICENSE','LICENSE.FDL','LICENSE.GPL3-EXCEPT')) {
        $taskNoticePath = Join-Path (Join-Path $taskQtTop 'Licenses') $taskNotice
        if (Test-Path -LiteralPath $taskNoticePath) { Copy-Item -LiteralPath $taskNoticePath -Destination $taskLicenses }
    }
    $taskQtVersion = Split-Path -Leaf (Split-Path -Parent $QtRoot)
    $taskLGPL = Join-Path $taskQtTop "Docs\Qt-$taskQtVersion\qtdoc\lgpl.html"
    if (Test-Path -LiteralPath $taskLGPL) { Copy-Item -LiteralPath $taskLGPL -Destination (Join-Path $taskLicenses 'LGPL-3.0.html') }
} finally { $env:PATH = $taskOldPath }
$taskFiles = @(Get-ChildItem -LiteralPath $taskDestination -Recurse -File)
[PSCustomObject]@{folder=$taskDestination; files=$taskFiles.Count; size_mib=[Math]::Round(($taskFiles | Measure-Object Length -Sum).Sum/1MB,2)}
