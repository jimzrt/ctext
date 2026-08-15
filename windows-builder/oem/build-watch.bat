@echo off
setlocal EnableExtensions
set ROOT=Z:\windows-builder

rem Keep one watcher alive for instant host-triggered builds. A stale lock can
rem survive a VM suspend or task restart, so use the heartbeat to distinguish
rem an active watcher from a dead one before claiming the lock.
if exist "C:\BuildTools\watcher.lock" (
  powershell.exe -NoProfile -Command "$p='Z:\windows-builder\watcher.heartbeat'; if ((Test-Path $p) -and (((Get-Date)-(Get-Item $p).LastWriteTime).TotalSeconds -lt 15)) { exit 0 } else { exit 1 }"
  if not errorlevel 1 exit /b 0
  rmdir /s /q "C:\BuildTools\watcher.lock" >nul 2>&1
)
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
