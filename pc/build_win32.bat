@echo off
rem Build the 32-bit Windows PC version on Windows, with MSYS2 (https://www.msys2.org).
rem One-time setup, in an MSYS2 shell:  pacman -S --needed make tar curl mingw-w64-i686-gcc
rem usage: pc\build_win32.bat [outdir]        (default outdir: pc\dist\win32)
rem Runs pc/build_win32.sh in the MSYS2 MINGW32 environment (it downloads the SDL2 mingw package once).
rem Set MSYS2_ROOT if MSYS2 is not installed in C:\msys64.
setlocal
if "%MSYS2_ROOT%"=="" set "MSYS2_ROOT=C:\msys64"
if not exist "%MSYS2_ROOT%\usr\bin\bash.exe" (
    echo error: MSYS2 not found in %MSYS2_ROOT% ^(install it from https://www.msys2.org or set MSYS2_ROOT^)
    exit /b 1
)
cd /d "%~dp0"
set MSYSTEM=MINGW32
set CHERE_INVOKING=1
if "%~1"=="" (
    "%MSYS2_ROOT%\usr\bin\bash.exe" -lc "./build_win32.sh"
) else (
    "%MSYS2_ROOT%\usr\bin\bash.exe" -lc "./build_win32.sh \"$(cygpath -u '%~f1')\""
)
exit /b %ERRORLEVEL%
