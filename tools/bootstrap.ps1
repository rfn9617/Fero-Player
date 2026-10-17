# Downloads the build toolchain and engine into native\.deps (once, ~1.5 GB).
# Windows PowerShell 5.1 compatible. Safe to run again: finished steps are skipped.
param([switch]$Force)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$Root = Split-Path -Parent $PSScriptRoot
$Deps = Join-Path $Root '.deps'
$QtVersion = '6.8.3'
New-Item -ItemType Directory -Force $Deps | Out-Null

function Step($text) { Write-Host ''; Write-Host "==> $text" }

function Get-File([string[]]$Urls, [string]$Out) {
    if ((Test-Path $Out) -and -not $Force) { return }
    foreach ($url in $Urls) {
        try {
            Write-Host "Downloading $url"
            Invoke-WebRequest -Uri $url -OutFile "$Out.part" -UseBasicParsing -Headers @{ 'User-Agent' = 'FeroPlayer-build' }
            Move-Item -Force "$Out.part" $Out
            return
        } catch {
            Write-Host "  failed: $($_.Exception.Message)"
        }
    }
    throw "Could not download $Out"
}

function Invoke-Checked([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE" }
}

Add-Type -Namespace Fero -Name Cpu -MemberDefinition '[DllImport("kernel32.dll")] public static extern bool IsProcessorFeaturePresent(uint feature);'
$Avx2 = [Fero.Cpu]::IsProcessorFeaturePresent(40)

Step 'Tools: 7-Zip extractor and Qt installer'
Get-File @('https://www.7-zip.org/a/7zr.exe') (Join-Path $Deps '7zr.exe')
Get-File @('https://github.com/miurahr/aqtinstall/releases/latest/download/aqt_x64.exe',
           'https://github.com/miurahr/aqtinstall/releases/latest/download/aqt.exe') (Join-Path $Deps 'aqt.exe')
$Aqt = Join-Path $Deps 'aqt.exe'
$QtRoot = Join-Path $Deps 'Qt'

Step "Qt $QtVersion (MinGW, only QtBase and QtSvg)"
if (-not (Test-Path (Join-Path $QtRoot "$QtVersion\mingw_64\bin\Qt6Widgets.dll")) -or $Force) {
    Invoke-Checked $Aqt @('install-qt', 'windows', 'desktop', $QtVersion, 'win64_mingw',
                          '--outputdir', $QtRoot, '--archives', 'qtbase', 'qtsvg')
}

Step 'Compiler (MinGW 13.1), CMake, Ninja'
if (-not (Test-Path (Join-Path $QtRoot 'Tools\mingw1310_64\bin\g++.exe')) -or $Force) {
    Invoke-Checked $Aqt @('install-tool', 'windows', 'desktop', 'tools_mingw1310', 'qt.tools.win64_mingw1310', '--outputdir', $QtRoot)
}
if (-not (Test-Path (Join-Path $QtRoot 'Tools\CMake_64\bin\cmake.exe')) -or $Force) {
    Invoke-Checked $Aqt @('install-tool', 'windows', 'desktop', 'tools_cmake', 'qt.tools.cmake', '--outputdir', $QtRoot)
}
if (-not (Test-Path (Join-Path $QtRoot 'Tools\Ninja\ninja.exe')) -or $Force) {
    Invoke-Checked $Aqt @('install-tool', 'windows', 'desktop', 'tools_ninja', 'qt.tools.ninja', '--outputdir', $QtRoot)
}

Step "libmpv (AVX2: $Avx2)"
$Mpv = Join-Path $Deps 'mpv'
if (-not (Test-Path (Join-Path $Mpv 'libmpv-2.dll')) -or $Force) {
    $release = Invoke-RestMethod 'https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/latest' -Headers @{ 'User-Agent' = 'FeroPlayer-build' }
    $pattern = if ($Avx2) { '^mpv-dev-x86_64-v3-\d{8}-git-[0-9a-f]+\.7z$' } else { '^mpv-dev-x86_64-\d{8}-git-[0-9a-f]+\.7z$' }
    $asset = $release.assets | Where-Object { $_.name -match $pattern } | Select-Object -First 1
    if (-not $asset) { throw "No libmpv development package in release $($release.tag_name)" }
    $archive = Join-Path $Deps $asset.name
    Get-File @($asset.browser_download_url) $archive
    if (Test-Path $Mpv) { Remove-Item -Recurse -Force $Mpv }
    Invoke-Checked (Join-Path $Deps '7zr.exe') @('x', $archive, "-o$Mpv", '-y')
    Set-Content -Path (Join-Path $Mpv 'SOURCE.txt') -Value "$($asset.name)`r`n$($asset.browser_download_url)`r`nrelease $($release.tag_name)"
}

$info = [ordered]@{
    qt = $QtVersion; avx2 = $Avx2
    mpv = (Get-Content (Join-Path $Mpv 'SOURCE.txt') -ErrorAction SilentlyContinue | Select-Object -First 1)
    gcc = (& (Join-Path $QtRoot 'Tools\mingw1310_64\bin\g++.exe') --version | Select-Object -First 1)
    cmake = (& (Join-Path $QtRoot 'Tools\CMake_64\bin\cmake.exe') --version | Select-Object -First 1)
}
$info | ConvertTo-Json | Set-Content (Join-Path $Deps 'toolchain.json')
Step 'Toolchain ready'
$info | Format-List | Out-String | Write-Host
