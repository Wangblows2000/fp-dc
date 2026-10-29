@echo off
rem Freedom Planet -> the PC version for Windows (output\win32\fpdc.exe + SDL2.dll + data), from your copy of the game.
rem Drag the Assets.dat of your Freedom Planet folder onto this file. See BUILDING.md.
rem The first run downloads about 125 MB of tools into build\cache (nothing is installed); a build takes 20-40 minutes.
setlocal
set "FPB_TARGET=win32"
set "FPB_WHAT=the Windows PC version (output\win32\fpdc.exe)"
set "FPB_SCRIPT=%~nx0"
call "%~dp0tools\builder\bootstrap.cmd" %*
set "FPB_RC=%ERRORLEVEL%"
if not "%FPB_NOPAUSE%"=="1" pause
exit /b %FPB_RC%
