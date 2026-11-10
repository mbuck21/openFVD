@echo off
rem    FVD++, an advanced coaster design tool for NoLimits
rem    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
rem    This file: Copyright (C) 2026, the contributors of github.com/mbuck21/openFVD
rem
rem    This program is free software: you can redistribute it and/or modify
rem    it under the terms of the GNU General Public License as published by
rem    the Free Software Foundation, either version 3 of the License, or
rem    (at your option) any later version.
rem
rem    This program is distributed in the hope that it will be useful,
rem    but WITHOUT ANY WARRANTY; without even the implied warranty of
rem    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
rem    GNU General Public License for more details.
rem
rem    You should have received a copy of the GNU General Public License
rem    along with this program. If not, see <http://www.gnu.org/licenses/>.
rem
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
