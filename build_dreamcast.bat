@echo off
rem Freedom Planet -> Dreamcast disc image (output\fpdc.cdi), from your copy of the game.
rem Drag the Assets.dat of your Freedom Planet folder onto this file. See BUILDING.md.
rem The first run downloads about 125 MB of tools into build\cache (nothing is installed); a build takes 20-40 minutes.
setlocal
set "FPB_TARGET=dc"
set "FPB_WHAT=the Dreamcast disc image (output\fpdc.cdi)"
set "FPB_SCRIPT=%~nx0"
call "%~dp0tools\builder\bootstrap.cmd" %*
set "FPB_RC=%ERRORLEVEL%"
if not "%FPB_NOPAUSE%"=="1" pause
exit /b %FPB_RC%
