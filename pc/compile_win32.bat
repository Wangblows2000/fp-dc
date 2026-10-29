@echo off
rem Developers: compile the Windows PC program alone (fpdc.exe + SDL2.dll, no game data) from source, with MSYS2.
rem To get a ready-to-play build use build_win32.bat in the top folder instead: drag Assets.dat onto it (no compiler needed).
rem Needs MSYS2 (https://www.msys2.org) with, in an MSYS2 shell:  pacman -S --needed make tar curl mingw-w64-i686-gcc
rem (a 64-bit mingw-w64-x86_64-gcc works too and gives a 64-bit fpdc.exe; a mingw-w64 gcc on the Windows PATH is used as well).
rem usage: compile_win32.bat [outdir]       default outdir: the folder you start it from
rem Set MSYS2_ROOT if MSYS2 is not installed in C:\msys64.
setlocal
set "FPC_OUTDIR=%~f1"
if "%~1"=="" set "FPC_OUTDIR=%CD%"
if "%MSYS2_ROOT%"=="" set "MSYS2_ROOT=C:\msys64"
if not exist "%MSYS2_ROOT%\usr\bin\bash.exe" goto nomsys
rem MINGW32 first (its gcc is the 32-bit one); MSYS2_PATH_TYPE=inherit keeps the Windows PATH (a mingw-w64 installed elsewhere)
set "MSYSTEM=MINGW32"
set "CHERE_INVOKING=1"
set "MSYS2_PATH_TYPE=inherit"
cd /d "%~dp0"
"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "./compile_win32.sh"
exit /b %ERRORLEVEL%
:nomsys
echo error: MSYS2 not found in %MSYS2_ROOT% - install it from https://www.msys2.org or set MSYS2_ROOT to its folder.
echo To just play: drag Assets.dat onto build_win32.bat in the top folder of the project (no compiler needed).
exit /b 1
