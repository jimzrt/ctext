@echo off
setlocal EnableExtensions
for %%I in ("%~dp0..") do set ROOT=%%~fI
set SOLUTION=%ROOT%\ctext.sln
set OUTPUT=Z:\windows-builder\artifacts
set MSBUILD=C:\BuildTools\VisualStudio\MSBuild\Current\Bin\MSBuild.exe
set NUGET=C:\BuildTools\nuget.exe
if not exist "%MSBUILD%" (echo MSBuild is not installed yet. Reboot the VM or rerun OEM setup.& exit /b 1)
if not exist "%OUTPUT%" mkdir "%OUTPUT%"
"%NUGET%" restore "%SOLUTION%" || exit /b 1

rem Intermediates stay local, so MSBuild can use its normal per-file
rem timestamps and trackers without shared-drive races.
if not exist "C:\CTExtBuild" mkdir "C:\CTExtBuild"
"%MSBUILD%" "%SOLUTION%" /m:1 /t:Build /p:Configuration=Release /p:Platform=x86 /p:CTExtIntRoot=C:\CTExtBuild\ || exit /b 1
copy /y "%ROOT%\build\Release\ctext.dll" "%OUTPUT%\ctext.dll" >nul || exit /b 1
copy /y "%ROOT%\build\Release\winmm.dll" "%OUTPUT%\winmm.dll" >nul || exit /b 1
copy /y "%ROOT%\build\Release\ctext.json" "%OUTPUT%\ctext.json" >nul || exit /b 1
copy /y "%ROOT%\build\Release\ChronoType.ttf" "%OUTPUT%\ChronoType.ttf" >nul || exit /b 1
xcopy /e /i /y "%ROOT%\ctext\assets" "%OUTPUT%\assets" >nul || exit /b 1
echo Package written to Z:\windows-builder\artifacts
dir /b "%OUTPUT%"
