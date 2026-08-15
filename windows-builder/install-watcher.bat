@echo off
setlocal
if not exist C:\BuildTools mkdir C:\BuildTools
copy /y Z:\windows-builder\oem\build-watch.bat C:\BuildTools\build-watch.bat
schtasks /Create /SC ONLOGON /TN CTextBuildWatcher /TR "cmd.exe /c C:\BuildTools\build-watch.bat" /RL HIGHEST /F
schtasks /Create /SC MINUTE /MO 1 /TN CTextBuildWatchdog /TR "cmd.exe /c C:\BuildTools\build-watch.bat" /RL HIGHEST /F
schtasks /Run /TN CTextBuildWatcher >nul 2>&1
echo Build watcher installed. You can now trigger builds from Linux.
