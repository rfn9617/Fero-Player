# Builds a static, trimmed Qt 6.8.3 (QtBase + QtSvg) for Fero Player with the
# MinGW toolchain from Setup.cmd. One-time job: about 30-60 minutes.
# Result: native\.deps\qt-static (Qt is then linked into FeroPlayer.exe).
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$Root = Split-Path -Parent $PSScriptRoot
$Deps = Join-Path $Root '.deps'
$QtTools = Join-Path $Deps 'Qt\Tools'
$Version = '6.8.3'
$Prefix = Join-Path $Deps 'qt-static'
# Qt's source tree is deep: build under a short path to stay below MAX_PATH.
$Work = 'C:\lq'
try { New-Item -ItemType Directory -Force $Work | Out-Null } catch { $Work = Join-Path $env:TEMP 'lq'; New-Item -ItemType Directory -Force $Work | Out-Null }
$env:PATH = "$(Join-Path $QtTools 'mingw1310_64\bin');$(Join-Path $QtTools 'CMake_64\bin');$(Join-Path $QtTools 'Ninja');$env:PATH"

function Invoke-Checked([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE" }
}

function Get-Source([string]$Module) {
    $zip = Join-Path $Work "$Module-everywhere-src-$Version.zip"
    $dir = Join-Path $Work "$Module-everywhere-src-$Version"
    if (-not (Test-Path $zip)) {
        # 6.8.3 has moved from official_releases to the archive; try both.
        $urls = @("https://download.qt.io/archive/qt/6.8/$Version/submodules/$Module-everywhere-src-$Version.zip",
                  "https://download.qt.io/official_releases/qt/6.8/$Version/submodules/$Module-everywhere-src-$Version.zip")
        foreach ($url in $urls) {
            try {
                Write-Host "Downloading $url"
                Invoke-WebRequest -Uri $url -OutFile "$zip.part" -UseBasicParsing
                Move-Item -Force "$zip.part" $zip
                break
            } catch { Write-Host "  failed: $($_.Exception.Message)" }
        }
        if (-not (Test-Path $zip)) { throw "Could not download the $Module sources" }
    }
    if (-not (Test-Path (Join-Path $dir 'CMakeLists.txt'))) {
        Write-Host "Extracting $Module"
        Invoke-Checked 'tar.exe' @('-xf', $zip, '-C', $Work)
    }
    return $dir
}

$started = Get-Date
if (-not (Test-Path (Join-Path $Prefix 'lib\cmake\Qt6Widgets'))) {
    $source = Get-Source 'qtbase'
    $build = Join-Path $Work 'b-base'
    if (Test-Path $build) { Remove-Item -Recurse -Force $build }
    New-Item -ItemType Directory $build | Out-Null
    Push-Location $build
    try {
        Write-Host '==> Configure QtBase (static, release, Widgets only)'
        Invoke-Checked (Join-Path $source 'configure.bat') @(
            '-prefix', $Prefix, '-static', '-static-runtime', '-release', '-platform', 'win32-g++',
            '-nomake', 'examples', '-nomake', 'tests',
            '-no-opengl', '-no-dbus', '-no-icu',
            '-qt-zlib', '-qt-libpng', '-qt-libjpeg', '-qt-pcre', '-qt-harfbuzz',
            '-no-feature-sql', '-no-feature-network', '-no-feature-testlib',
            '-no-feature-printsupport', '-no-feature-concurrent')
        Write-Host '==> Build QtBase'
        Invoke-Checked 'cmake.exe' @('--build', '.', '--parallel')
        Invoke-Checked 'cmake.exe' @('--install', '.')
    } finally { Pop-Location }
}
if (-not (Test-Path (Join-Path $Prefix 'lib\cmake\Qt6Svg'))) {
    $source = Get-Source 'qtsvg'
    $build = Join-Path $Work 'b-svg'
    if (Test-Path $build) { Remove-Item -Recurse -Force $build }
    New-Item -ItemType Directory $build | Out-Null
    Push-Location $build
    try {
        Write-Host '==> Configure and build QtSvg'
        Invoke-Checked (Join-Path $Prefix 'bin\qt-configure-module.bat') @($source)
        Invoke-Checked 'cmake.exe' @('--build', '.', '--parallel')
        Invoke-Checked 'cmake.exe' @('--install', '.')
    } finally { Pop-Location }
}
Write-Host ("Static Qt ready in {0:N0} min: {1}" -f ((Get-Date) - $started).TotalMinutes, $Prefix)
