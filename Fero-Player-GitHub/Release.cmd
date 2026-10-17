@echo off
rem Builds Fero Player, runs the self-test and makes the portable ZIP and the installer in dist.
cd /d "%~dp0"
echo Building the Fero Player release. The window closes by itself.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1" -Test -Installer > "%~dp0build-log.txt" 2>&1
>> "%~dp0build-log.txt" echo build exit code: %errorlevel%
echo Finished. See build-log.txt
timeout /t 5 >nul
