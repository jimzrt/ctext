@echo off
setlocal
if not exist C:\BuildTools mkdir C:\BuildTools
if not exist C:\BuildTools\vs_BuildTools.exe (
  curl.exe -L --fail --retry 5 -o C:\BuildTools\vs_BuildTools.exe https://aka.ms/vs/17/release/vs_BuildTools.exe
  if errorlevel 1 exit /b 1
)
C:\BuildTools\vs_BuildTools.exe --quiet --wait --norestart --nocache --installPath C:\BuildTools\VisualStudio --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100
if errorlevel 1 exit /b 1
if not exist C:\BuildTools\nuget.exe (
  curl.exe -L --fail --retry 5 -o C:\BuildTools\nuget.exe https://dist.nuget.org/win-x86-commandline/latest/nuget.exe
  if errorlevel 1 exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\OEM\install-git.ps1
if errorlevel 1 exit /b 1

rem Let the Linux host request builds through the shared folder. The scheduled
rem task runs at the interactive user's logon, where Dockur's Z: drive exists.
copy /y C:\OEM\build-watch.bat C:\BuildTools\build-watch.bat >nul
schtasks /Create /SC ONLOGON /TN CTextBuildWatcher /TR "cmd.exe /c C:\BuildTools\build-watch.bat" /RL HIGHEST /F >nul
schtasks /Create /SC MINUTE /MO 1 /TN CTextBuildWatchdog /TR "cmd.exe /c C:\BuildTools\build-watch.bat" /RL HIGHEST /F >nul
schtasks /Run /TN CTextBuildWatcher >nul 2>&1

echo CText build appliance is ready.
echo Run Z:\windows-builder\build-release.bat to build and package CText.
exit /b 0
