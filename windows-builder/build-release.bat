@echo off
setlocal EnableExtensions

rem This wrapper keeps the source checkout and MSBuild intermediates on the
rem Windows disk.  Only the request/status files and final package use Z:.
set SOURCE=C:\CTExtSource
set GIT=C:\BuildTools\git\cmd\git.exe
set REPO=https://github.com/jimzrt/ctext.git
if not exist "%GIT%" (
  echo MinGit is not installed. Installing it now...
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File Z:\windows-builder\oem\install-git.ps1
  if errorlevel 1 (echo MinGit installation failed.& exit /b 1)
)

if not exist "%SOURCE%\.git" (
  "%GIT%" clone --recurse-submodules --branch build/ctext --single-branch "%REPO%" "%SOURCE%" || exit /b 1
) else (
  "%GIT%" -C "%SOURCE%" fetch origin build/ctext || exit /b 1
  "%GIT%" -C "%SOURCE%" reset --hard FETCH_HEAD || exit /b 1
)
"%GIT%" -C "%SOURCE%" submodule sync --recursive || exit /b 1
"%GIT%" -C "%SOURCE%" submodule update --init --recursive || exit /b 1

call "%SOURCE%\windows-builder\build-local.bat"
exit /b %errorlevel%
