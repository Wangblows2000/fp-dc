@echo off
rem Freedom Planet -> Dreamcast disc image (fpdc.cdi, written next to this file), from your copy of the game.
rem Drag the Assets.dat of your Freedom Planet folder onto this file (or double-click it: an Assets.dat in this folder or up
rem to three folders above it is found by itself, else it asks). See BUILDING.md.
rem The first run downloads about 125 MB of tools into build\cache (nothing is installed); a build takes 20-40 minutes.
setlocal
set "FPB_TARGET=dc"
set "FPB_WHAT=the Dreamcast disc image (fpdc.cdi in %~dp0)"
set "FPB_SCRIPT=%~nx0"
call "%~dp0tools\builder\bootstrap.cmd" %*
set "FPB_RC=%ERRORLEVEL%"
if not "%FPB_NOPAUSE%"=="1" pause
exit /b %FPB_RC%
