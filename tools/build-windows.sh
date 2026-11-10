#!/bin/bash
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
#    This file: Copyright (C) 2026, the contributors of github.com/mbuck21/openFVD
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program. If not, see <http://www.gnu.org/licenses/>.
#
# Build FVD++ on Windows with the MSYS2 UCRT64 toolchain (docs/building.md).
# Run from the MSYS2 UCRT64 shell, or through tools\build-windows.cmd.
# Extra arguments go to qmake, for example: CONFIG+=with3ds
# Result: build/release/FVD.exe, and a copy that runs on its own in build/dist/.
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ "${MSYSTEM:-}" != "UCRT64" ]; then
    echo "Run this from the MSYS2 UCRT64 shell (MSYSTEM is '${MSYSTEM:-unset}')." >&2
    exit 1
fi
for tool in qmake-qt5 mingw32-make g++ windeployqt-qt5; do
    if ! command -v "$tool" >/dev/null; then
        echo "Missing $tool. Install the packages listed in docs/building.md." >&2
        exit 1
    fi
done

mkdir -p "$repo/build"
cd "$repo/build"
qmake-qt5 ../fvd.pro CONFIG+=release "$@"
mingw32-make -j"$(nproc)"

# A folder that runs without MSYS2 on the PATH: the program, the Qt plugins
# and every DLL they load from the MSYS2 tree.
dist="$repo/build/dist"
rm -rf "$dist"
mkdir -p "$dist"
cp release/FVD.exe "$dist/"
windeployqt-qt5 --release --no-translations --no-system-d3d-compiler --no-angle --no-opengl-sw "$dist/FVD.exe" >/dev/null

copy_deps() {
    ldd "$1" | awk '/\/ucrt64\/bin\// {print $3}' | while read -r dll; do
        [ -e "$dist/$(basename "$dll")" ] || cp "$dll" "$dist/"
    done
}
copy_deps "$dist/FVD.exe"
for plugin in "$dist"/*/*.dll; do
    copy_deps "$plugin"
done

echo "Built:           $repo/build/release/FVD.exe"
echo "Standalone copy: $repo/build/dist/FVD.exe"
