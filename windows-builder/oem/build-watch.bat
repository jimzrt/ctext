@echo off
setlocal EnableExtensions
set ROOT=Z:\windows-builder

rem Keep one watcher alive for instant host-triggered builds. mkdir is atomic,
rem so the watchdog task cannot start a duplicate copy.
if exist "C:\BuildTools\watcher.lock" exit /b 0
mkdir "C:\BuildTools\watcher.lock" >nul 2>&1
if not exist "C:\BuildTools\watcher.lock" exit /b 0

:watch
echo %DATE% %TIME%>"%ROOT%\watcher.heartbeat"
if exist "%ROOT%\build.request" if not exist "%ROOT%\build.running" (
  del /q "%ROOT%\build.request" >nul 2>&1
  echo running>"%ROOT%\build.running"
  echo running>"%ROOT%\build.status"
  call "%ROOT%\build-release.bat" >"%ROOT%\build.log" 2>&1
  if errorlevel 1 (echo failed>"%ROOT%\build.status") else (echo success>"%ROOT%\build.status")
  del /q "%ROOT%\build.running" >nul 2>&1
)
timeout /t 1 /nobreak >nul
goto watch
