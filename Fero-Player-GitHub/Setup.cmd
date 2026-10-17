@echo off
rem Downloads compilers, Qt and libmpv into .deps (one time, about 1.5 GB).
cd /d "%~dp0"
echo Downloading the build toolchain. This takes a while; the window closes by itself.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\bootstrap.ps1" > "%~dp0setup-log.txt" 2>&1
>> "%~dp0setup-log.txt" echo setup exit code: %errorlevel%
echo Finished. See setup-log.txt
timeout /t 5 >nul
