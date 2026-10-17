# Builds a custom libmpv-2.dll (mpv + FFmpeg from source) with MSYS2/UCRT64.
# Every FFmpeg decoder, demuxer and parser is kept; only encoders, muxers,
# network protocols, capture devices and scripting are left out.
# One-time job (about 1-2 hours). Result: native\.deps\mpv-custom
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$Root = Split-Path -Parent $PSScriptRoot
$Msys = 'C:\msys64'
$Bash = Join-Path $Msys 'usr\bin\bash.exe'
New-Item -ItemType Directory -Force 'C:\lq' | Out-Null

if (-not (Test-Path $Bash)) {
    $sfx = 'C:\lq\msys2-base.sfx.exe'
    if (-not (Test-Path $sfx)) {
        Write-Host 'Downloading MSYS2'
        Invoke-WebRequest -Uri 'https://github.com/msys2/msys2-installer/releases/download/nightly-x86_64/msys2-base-x86_64-latest.sfx.exe' -OutFile $sfx -UseBasicParsing
    }
    Write-Host 'Extracting MSYS2 to C:\msys64'
    & $sfx '-y' '-oC:\' | Out-Null
    if (-not (Test-Path $Bash)) { throw 'MSYS2 extraction failed' }
}
$env:MSYSTEM = 'UCRT64'
$env:CHERE_INVOKING = '1'
$env:MSYS2_PATH_TYPE = 'minimal'
function Invoke-Bash([string]$Command) {
    & $Bash -lc $Command
    if ($LASTEXITCODE -ne 0) { throw "bash failed ($LASTEXITCODE): $Command" }
}
Invoke-Bash 'true'   # first start initializes the keyring
Invoke-Bash 'pacman -Syuu --noconfirm || true'
Invoke-Bash 'pacman -Syuu --noconfirm'
# Sources: fetched here because Windows trusts this PC's certificate chain
# (MSYS2's curl rejected it: "self-signed certificate in certificate chain").
$FfmpegVersion = '9.0.2'
$MpvVersion = '3186d369f9'   # same mpv revision as the tested stock engine
$env:FFMPEG_VERSION = $FfmpegVersion
$env:MPV_VERSION = $MpvVersion
New-Item -ItemType Directory -Force 'C:\lq\mpv' | Out-Null
$sources = @(
    @{ File = "C:\lq\mpv\ffmpeg-$FfmpegVersion.tar.xz"; Url = "https://ffmpeg.org/releases/ffmpeg-$FfmpegVersion.tar.xz" },
    @{ File = "C:\lq\mpv\mpv-$MpvVersion.tar.gz"; Url = "https://github.com/mpv-player/mpv/archive/$MpvVersion.tar.gz" }
)
foreach ($s in $sources) {
    if (-not (Test-Path $s.File)) {
        Write-Host "Downloading $($s.Url)"
        Invoke-WebRequest -Uri $s.Url -OutFile "$($s.File).part" -UseBasicParsing
        Move-Item -Force "$($s.File).part" $s.File
    }
}
$script = (& $Bash -lc "cygpath -u '$(Join-Path $PSScriptRoot 'build-mpv.sh')'").Trim()
$output = (& $Bash -lc "cygpath -u '$(Join-Path $Root '.deps\mpv-custom')'").Trim()
Invoke-Bash "bash '$script' '$output'"
Write-Host "Custom libmpv ready: $(Join-Path $Root '.deps\mpv-custom')"
