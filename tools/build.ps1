# Configures, compiles and packages Fero Player into dist\FeroPlayer, then runs
# the self-test. Run Setup.cmd once before.
param([switch]$Test, [switch]$NoZip, [switch]$Installer)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$Root = Split-Path -Parent $PSScriptRoot
$Deps = Join-Path $Root '.deps'
$QtRoot = Join-Path $Deps 'Qt'
$Qt = Join-Path $QtRoot '6.8.3\mingw_64'
$requestFile = Join-Path $PSScriptRoot 'rebuild-request.txt'
$request = if (Test-Path $requestFile) { Get-Content $requestFile -Raw } else { '' }
# Static Qt: built once on request, then preferred automatically.
$QtStatic = Join-Path $Deps 'qt-static'
if ($request -match '\bqtstatic\b' -and -not (Test-Path (Join-Path $QtStatic 'lib\cmake\Qt6Svg'))) {
    try { & (Join-Path $PSScriptRoot 'build-qt-static.ps1') }
    catch { Write-Host "STATIC QT FAILED: $($_.Exception.Message)`n$($_.ScriptStackTrace)" }
}
# Custom libmpv: built once on request, then preferred automatically.
$MpvCustom = Join-Path $Deps 'mpv-custom'
if ($request -match '\bmpv\b' -and -not (Test-Path (Join-Path $MpvCustom 'libmpv-2.dll'))) {
    try { & (Join-Path $PSScriptRoot 'build-mpv.ps1') }
    catch { Write-Host "CUSTOM MPV FAILED: $($_.Exception.Message)`n$($_.ScriptStackTrace)" }
}
$MpvDir = if ((Test-Path (Join-Path $MpvCustom 'libmpv-2.dll')) -and $request -notmatch '\bstockmpv\b') { $MpvCustom } else { Join-Path $Deps 'mpv' }
Write-Host "Engine: $MpvDir"
$Static = (Test-Path (Join-Path $QtStatic 'lib\cmake\Qt6Svg')) -and $request -notmatch '\bshared\b'
if ($Static) { $Qt = $QtStatic }
$Mingw = Join-Path $QtRoot 'Tools\mingw1310_64\bin'
$CMake = Join-Path $QtRoot 'Tools\CMake_64\bin\cmake.exe'
$Ninja = Join-Path $QtRoot 'Tools\Ninja'
$Build = Join-Path $Root 'build'
$Dist = Join-Path $Root 'dist\FeroPlayer'

if ($Static) { $Build = Join-Path $Root 'build-static' }
foreach ($required in @((Join-Path $Qt 'lib\cmake\Qt6\Qt6Config.cmake'), (Join-Path $Mingw 'g++.exe'), $CMake,
                        (Join-Path $MpvDir 'libmpv-2.dll'))) {
    if (-not (Test-Path $required)) { throw "Missing $required. Run Setup.cmd first." }
}
$env:PATH = "$Mingw;$(Join-Path $Qt 'bin');$Ninja;$env:PATH"

function Invoke-Checked([string]$Exe, [string[]]$Arguments) {
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$([IO.Path]::GetFileName($Exe)) failed with exit code $LASTEXITCODE" }
}

function CMakePath([string]$Path) { return $Path.Replace('\', '/') }
# A configure step that failed leaves a cache without build.ninja: start clean.
if ((Test-Path (Join-Path $Build 'CMakeCache.txt')) -and -not (Test-Path (Join-Path $Build 'build.ninja'))) {
    Remove-Item -Recurse -Force $Build
}
Write-Host '==> Configure'
Invoke-Checked $CMake @('-S', (CMakePath $Root), '-B', (CMakePath $Build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
                        "-DCMAKE_PREFIX_PATH=$(CMakePath $Qt)", "-DCMAKE_MAKE_PROGRAM=$(CMakePath (Join-Path $Ninja 'ninja.exe'))",
                        "-DCMAKE_CXX_COMPILER=$(CMakePath (Join-Path $Mingw 'g++.exe'))",
                        "-DCMAKE_RC_COMPILER=$(CMakePath (Join-Path $Mingw 'windres.exe'))",
                        "-DFERO_STATIC=$(if ($Static) { 'ON' } else { 'OFF' })",
                        "-DMPV_DIR=$(CMakePath $MpvDir)")
Write-Host '==> Compile'
Invoke-Checked $CMake @('--build', (CMakePath $Build), '--parallel', '--', '-k', '0')

Write-Host '==> Package'
if (Test-Path $Dist) {
    try { Remove-Item -Recurse -Force $Dist -ErrorAction Stop }
    catch {
        # The player is running from dist\FeroPlayer: package next to it instead.
        Write-Host 'dist\FeroPlayer is in use; packaging into dist\FeroPlayer-next'
        $Dist = Join-Path $Root 'dist\FeroPlayer-next'
        if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
    }
}
New-Item -ItemType Directory -Force $Dist | Out-Null
Copy-Item (Join-Path $Build 'FeroPlayer.exe') $Dist
if (-not $Static) {
Invoke-Checked (Join-Path $Qt 'bin\windeployqt.exe') @('--release', '--no-translations', '--no-system-d3d-compiler',
    '--no-opengl-sw', '--no-quick-import', '--compiler-runtime', '--verbose', '0', (Join-Path $Dist 'FeroPlayer.exe'))
# Keep only the Qt plug-ins Fero uses.
$keep = @('platforms\qwindows.dll', 'imageformats\qsvg.dll',
          'imageformats\qico.dll', 'imageformats\qjpeg.dll', 'iconengines\qsvgicon.dll')
Get-ChildItem $Dist -Recurse -File -Filter *.dll | Where-Object { $_.DirectoryName -ne $Dist } | ForEach-Object {
    $relative = $_.FullName.Substring($Dist.Length + 1)
    if ($keep -notcontains $relative) { Remove-Item $_.FullName }
}
Get-ChildItem $Dist -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending |
    Where-Object { -not (Get-ChildItem $_.FullName -Force) } | Remove-Item
}
# Pulled in by plug-ins Fero does not ship (TLS, network information).
Remove-Item (Join-Path $Dist 'Qt6Network.dll') -ErrorAction SilentlyContinue
Get-ChildItem $MpvDir -Filter *.dll | Copy-Item -Destination $Dist
foreach ($notice in @('LICENSE', 'THIRD-PARTY-NOTICES.md')) {
    $source = Join-Path $Root $notice
    if (Test-Path $source) { Copy-Item $source $Dist }
}
$files = Get-ChildItem $Dist -Recurse -File
$size = ($files | Measure-Object Length -Sum).Sum
$report = [ordered]@{
    files = $files.Count; bytes = $size; mib = [math]::Round($size / 1MB, 1)
    largest = @($files | Sort-Object Length -Descending | Select-Object -First 12 |
        ForEach-Object { '{0,8:N1} MiB  {1}' -f ($_.Length / 1MB), $_.FullName.Substring($Dist.Length + 1) })
}
$report | ConvertTo-Json | Set-Content (Join-Path $Root 'dist\package.json')
Write-Host "Package ($(if ($Static) { 'static Qt' } else { 'shared Qt' })): $($report.files) files, $($report.mib) MiB"

$requestFile = Join-Path $PSScriptRoot 'rebuild-request.txt'
if (-not $Test -and (Test-Path $requestFile) -and ((Get-Content $requestFile -Raw) -match '\btest\b')) { $Test = $true }
if ($Test) {
    Write-Host '==> Self-test'
    $media = Join-Path $Root 'tools\fero-test.webm'
    $output = Join-Path $Root 'dist\self-test'
    # Screenshots are replaced; the test profile (with its shader cache) is kept.
    Get-ChildItem $output -File -ErrorAction SilentlyContinue | Remove-Item
    $variants = @('')
    $extra = Join-Path $PSScriptRoot 'test-variants.txt'
    if (Test-Path $extra) { $variants += @(Get-Content $extra | Where-Object { $_.Trim() }) }
    $index = 0
    foreach ($variant in $variants) {
        $index++
        $target = if ($variant) { Join-Path $output "variant-$index" } else { $output }
        Get-ChildItem $target -File -ErrorAction SilentlyContinue | Remove-Item
        $arguments = @('--self-test', "`"$((Resolve-Path $media).Path)`"", "`"$target`"")
        if ($variant) { $arguments += $variant.Split(' ') }
        $process = Start-Process -FilePath (Join-Path $Dist 'FeroPlayer.exe') -ArgumentList $arguments -PassThru
        if (-not $process.WaitForExit(90000)) { $process.Kill(); Write-Host 'Self-test timed out' }
        Write-Host "Variant: $variant  exit code: $($process.ExitCode)"
        $report = Join-Path $target 'self-test.json'
        if (Test-Path $report) { Get-Content -Encoding UTF8 $report | Write-Host }
    }
}
$Version = '1.0.0'
if ((Get-Content (Join-Path $Root 'CMakeLists.txt') -Raw) -match 'project\(FeroPlayer VERSION ([0-9.]+)') { $Version = $Matches[1] }
if (-not $NoZip) {
    $zip = Join-Path $Root "dist\FeroPlayer-$Version-portable-x64.zip"
    if (Test-Path $zip) { Remove-Item $zip }
    Compress-Archive -Path $Dist -DestinationPath $zip -CompressionLevel Optimal
    Write-Host "ZIP: $([math]::Round((Get-Item $zip).Length / 1MB, 1)) MiB"
}
if ($Installer -or $request -match '\binstaller\b') {
    Write-Host '==> Installer'
    $iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
              "$env:ProgramFiles\Inno Setup 6\ISCC.exe", 'C:\lq\inno\ISCC.exe') | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $iscc) {
        # Inno Setup (free, jrsoftware.org) is installed once for the current user.
        New-Item -ItemType Directory -Force 'C:\lq' | Out-Null
        $setup = 'C:\lq\innosetup.exe'
        Remove-Item $setup -ErrorAction SilentlyContinue
        try {
            $release = Invoke-RestMethod 'https://api.github.com/repos/jrsoftware/issrc/releases/latest' -Headers @{ 'User-Agent' = 'FeroPlayer-build' }
            $asset = $release.assets | Where-Object { $_.name -match '^innosetup-[0-9.]+\.exe$' } | Select-Object -First 1
            Write-Host "Downloading $($asset.name)"
            Invoke-WebRequest -Uri $asset.browser_download_url -OutFile $setup -UseBasicParsing
        } catch { Write-Host "GitHub download failed: $_" }
        $isExe = (Test-Path $setup) -and ([IO.File]::ReadAllBytes($setup)[0..1] -join ',') -eq '77,90'
        if ($isExe) {
            Start-Process -FilePath $setup -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER', '/DIR=C:\lq\inno' -Wait
            $iscc = 'C:\lq\inno\ISCC.exe'
        } else {
            Write-Host 'Trying winget'
            & winget install --id JRSoftware.InnoSetup -e --scope user --silent --accept-package-agreements --accept-source-agreements
            $iscc = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
        }
        if (-not (Test-Path $iscc)) { throw 'Inno Setup could not be installed' }
    }
    & $iscc "/DAppVersion=$Version" "/DSourceDir=$Dist" (Join-Path $PSScriptRoot 'installer.iss')
    if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }
    $setupFile = Join-Path $Root "dist\FeroPlayer-Setup-$Version.exe"
    Write-Host "Installer: $([math]::Round((Get-Item $setupFile).Length / 1MB, 1)) MiB"
}
Write-Host '==> Done'
