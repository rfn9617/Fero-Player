@echo off
rem Builds Fero Player into dist\FeroPlayer and runs the self-test.
cd /d "%~dp0"
echo Building Fero Player. The window closes by itself.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1" -Test > "%~dp0build-log.txt" 2>&1
>> "%~dp0build-log.txt" echo build exit code: %errorlevel%
echo Finished. See build-log.txt
timeout /t 5 >nul
