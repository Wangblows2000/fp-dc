@echo off
rem Freedom Planet -> the PC version for Windows (fpdc.exe + SDL2.dll + data, written next to this file), from your copy of the game.
rem Drag the Assets.dat of your Freedom Planet folder onto this file (or double-click it: an Assets.dat in this folder or up
rem to three folders above it is found by itself, else it asks). No compiler is needed. See BUILDING.md.
rem The first run downloads about 125 MB of tools into build\cache (nothing is installed); a build takes 20-40 minutes.
setlocal
set "FPB_TARGET=win32"
set "FPB_WHAT=the Windows PC version (fpdc.exe in %~dp0)"
set "FPB_SCRIPT=%~nx0"
call "%~dp0tools\builder\bootstrap.cmd" %*
set "FPB_RC=%ERRORLEVEL%"
if not "%FPB_NOPAUSE%"=="1" pause
exit /b %FPB_RC%
