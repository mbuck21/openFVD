@echo off
rem Builds FVD++ with the MSYS2 UCRT64 toolchain. See docs\building.md.
rem Extra arguments go to qmake, for example: tools\build-windows.cmd CONFIG+=with3ds
setlocal
set "MSYS2=C:\msys64"
if not exist "%MSYS2%\usr\bin\bash.exe" (
    echo MSYS2 was not found at %MSYS2%. See docs\building.md.
    exit /b 1
)
set MSYSTEM=UCRT64
set CHERE_INVOKING=1
"%MSYS2%\usr\bin\bash.exe" -lc "\"$(cygpath -u '%~dp0build-windows.sh')\" %*"
