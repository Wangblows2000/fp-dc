@echo off
rem The Windows side of the build_*.bat scripts: a portable Python 3.13 (downloaded once from python.org into build\cache,
rem SHA-256 checked; nothing is installed), then tools\builder\fpbuild.py, which fetches the other pinned tools the same way.
rem Called by build_*.bat with FPB_TARGET, FPB_WHAT and FPB_SCRIPT set: call tools\builder\bootstrap.cmd <dropped files>
rem Needs 64-bit Windows 10 (1803) or 11: curl.exe, tar.exe and certutil.exe are part of it.
rem FPDC_PREBUILT=<folder>: downloads are taken from that folder when they are there (also: the top folder, prebuilt\).
rem FPB_LOCAL=1: no download of Python: python.exe from the PATH, with Pillow and lz4 installed (developers).
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul 2>nul
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "CACHE=%ROOT%\build\cache"
set "PYNAME=python-3.13.14-embed-amd64.zip"
set "PYSHA=90b4e5b9898b72d744650524bff92377c367f44bd5fbd09e3148656c080ad907"
set "PYURL=https://www.python.org/ftp/python/3.13.14/python-3.13.14-embed-amd64.zip"
set "PYDIR=%CACHE%\python-3.13.14"
echo Freedom Planet builder: %FPB_WHAT%
echo.
if "%FPB_LOCAL%"=="1" goto runlocal
if /i "%PROCESSOR_ARCHITECTURE%"=="x86" if not defined PROCESSOR_ARCHITEW6432 goto not64
if exist "%PYDIR%\python.exe" goto run

set "ZIP="
if defined FPDC_PREBUILT if exist "%FPDC_PREBUILT%\%PYNAME%" set "ZIP=%FPDC_PREBUILT%\%PYNAME%"
if not defined ZIP if exist "%ROOT%\%PYNAME%" set "ZIP=%ROOT%\%PYNAME%"
if not defined ZIP if exist "%ROOT%\prebuilt\%PYNAME%" set "ZIP=%ROOT%\prebuilt\%PYNAME%"
if not defined ZIP if exist "%CACHE%\downloads\%PYNAME%" set "ZIP=%CACHE%\downloads\%PYNAME%"
if defined ZIP goto verify
if not exist "%CACHE%\downloads" mkdir "%CACHE%\downloads"
if not exist "%CACHE%\downloads" goto nowrite
echo First run: downloading Python 3.13 (11 MB) from python.org into build\cache ...
where curl.exe >nul 2>nul
if errorlevel 1 goto pwsh_get
curl.exe -fL --retry 3 -o "%CACHE%\downloads\%PYNAME%.part" "%PYURL%"
if errorlevel 1 goto dlfail
goto got
:pwsh_get
powershell -NoProfile -Command "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -UseBasicParsing -Uri $env:PYURL -OutFile ($env:CACHE + '\downloads\' + $env:PYNAME + '.part')"
if errorlevel 1 goto dlfail
:got
move /y "%CACHE%\downloads\%PYNAME%.part" "%CACHE%\downloads\%PYNAME%" >nul
set "ZIP=%CACHE%\downloads\%PYNAME%"

:verify
certutil.exe -hashfile "%ZIP%" SHA256 | findstr /i /c:"%PYSHA%" >nul
if errorlevel 1 goto badsum
if exist "%PYDIR%.part" rd /s /q "%PYDIR%.part"
mkdir "%PYDIR%.part"
where tar.exe >nul 2>nul
if errorlevel 1 goto pwsh_unzip
tar.exe -xf "%ZIP%" -C "%PYDIR%.part"
if errorlevel 1 goto unpackfail
goto unpacked
:pwsh_unzip
set "FPB_ZIP=%ZIP%"
set "FPB_DEST=%PYDIR%.part"
powershell -NoProfile -Command "Expand-Archive -LiteralPath $env:FPB_ZIP -DestinationPath $env:FPB_DEST -Force"
if errorlevel 1 goto unpackfail
:unpacked
if not exist "%PYDIR%.part\python.exe" goto unpackfail
rem without its ._pth file the embeddable Python sets sys.path as a normal install does (script folder, Lib\site-packages)
del "%PYDIR%.part\python313._pth"
move "%PYDIR%.part" "%PYDIR%" >nul
if errorlevel 1 goto unpackfail

:run
set "FPB_MANAGED=1"
set "FPB_RESULT=%CACHE%\result.txt"
set "PYTHONUTF8=1"
set "PYTHONDONTWRITEBYTECODE=1"
rem (stdin from NUL: the build asks nothing, and a console without a valid input handle stops Python at start)
"%PYDIR%\python.exe" "%ROOT%\tools\builder\fpbuild.py" %FPB_TARGET% %* <nul
if errorlevel 1 goto failed
if not exist "%FPB_RESULT%" goto failed
goto finished
:runlocal
set "PYTHONUTF8=1"
if not exist "%CACHE%" mkdir "%CACHE%"
set "FPB_RESULT=%CACHE%\result.txt"
python.exe "%ROOT%\tools\builder\fpbuild.py" %FPB_TARGET% %* --local-tools <nul
if errorlevel 1 goto failed
if not exist "%FPB_RESULT%" goto failed
:finished
echo.
echo Finished: %FPB_WHAT%
exit /b 0

:failed
echo.
echo The build stopped: see the ERROR line above. The full log is build\builder\build.log; BUILDING.md has a
echo troubleshooting section. Run %FPB_SCRIPT% again after fixing it: finished steps are not redone.
exit /b 1
:usage
echo Drag the Assets.dat of your Freedom Planet folder onto %FPB_SCRIPT%.
echo It builds %FPB_WHAT%.
echo From a command prompt:  %FPB_SCRIPT% "C:\path\to\Freedom Planet\Assets.dat"
exit /b 2
:not64
echo ERROR: this builder needs 64-bit Windows.
exit /b 1
:nowrite
echo ERROR: cannot create the folder %CACHE%\downloads (is this folder read-only?). Copy the project to a folder you can write to.
exit /b 1
:dlfail
echo ERROR: the download of %PYURL% failed. Check the internet connection and run again,
echo or download that file yourself and put it next to %FPB_SCRIPT%.
exit /b 1
:badsum
echo ERROR: %ZIP% is not the expected file (SHA-256 %PYSHA%). Delete it and run again.
exit /b 1
:unpackfail
echo ERROR: could not unpack %ZIP% into %PYDIR%.
exit /b 1
