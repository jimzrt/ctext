@echo off
setlocal EnableExtensions
set ROOT=Z:\
set SOLUTION=%ROOT%ctext.sln
set OUTPUT=%ROOT%windows-builder\artifacts
set MSBUILD=C:\BuildTools\VisualStudio\MSBuild\Current\Bin\MSBuild.exe
set NUGET=C:\BuildTools\nuget.exe
if not exist "%MSBUILD%" (echo MSBuild is not installed yet. Reboot the VM or rerun OEM setup.& exit /b 1)
if not exist "%OUTPUT%" mkdir "%OUTPUT%"
"%NUGET%" restore "%SOLUTION%" || exit /b 1
rem Keep intermediates on the Windows C: drive. This makes normal builds
rem incremental while avoiding MSBuild tracker races on Dockur's shared Z:.
if not exist "C:\CTExtBuild" mkdir "C:\CTExtBuild"
set FINGERPRINT=Z:\windows-builder\source.fingerprint
set LOCAL_FINGERPRINT=C:\CTExtBuild\source.fingerprint
set SOURCE_CHANGED=0
if not exist "%LOCAL_FINGERPRINT%" set SOURCE_CHANGED=1
if "%SOURCE_CHANGED%"=="0" fc /b "%FINGERPRINT%" "%LOCAL_FINGERPRINT%" >nul || set SOURCE_CHANGED=1
if "%SOURCE_CHANGED%"=="1" (
  rmdir /s /q "C:\CTExtBuild\ctext" >nul 2>&1
  rmdir /s /q "C:\CTExtBuild\loader" >nul 2>&1
  copy /y "%FINGERPRINT%" "%LOCAL_FINGERPRINT%" >nul
)
"%MSBUILD%" "%SOLUTION%" /m:1 /t:Build /p:Configuration=Release /p:Platform=x86 /p:CTExtIntRoot=C:\CTExtBuild\ || exit /b 1
copy /y "%ROOT%build\Release\ctext.dll" "%OUTPUT%\ctext.dll" >nul || exit /b 1
copy /y "%ROOT%build\Release\winmm.dll" "%OUTPUT%\winmm.dll" >nul || exit /b 1
copy /y "%ROOT%build\Release\ctext.json" "%OUTPUT%\ctext.json" >nul || exit /b 1
copy /y "%ROOT%build\Release\ChronoType.ttf" "%OUTPUT%\ChronoType.ttf" >nul || exit /b 1
xcopy /e /i /y "%ROOT%ctext\assets" "%OUTPUT%\assets" >nul || exit /b 1
echo Package written to Z:\windows-builder\artifacts
dir /b "%OUTPUT%"
