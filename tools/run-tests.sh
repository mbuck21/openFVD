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
# Build and run every automated test under tests/ (see tests/README.md).
# Run from the MSYS2 UCRT64 shell, or through tools\run-tests.cmd.
# Each test is built into build/tests/<name>/ and is run with the repository
# folder as its first argument. Ends with PASS or FAIL and a
# matching exit code.
set -uo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ "${MSYSTEM:-}" != "UCRT64" ]; then
    echo "Run this from the MSYS2 UCRT64 shell (MSYSTEM is '${MSYSTEM:-unset}')." >&2
    exit 1
fi
for tool in qmake-qt5 mingw32-make g++; do
    if ! command -v "$tool" >/dev/null; then
        echo "Missing $tool. Install the packages listed in docs/building.md." >&2
        exit 1
    fi
done

failed=0
count=0
for pro in "$repo"/tests/*/*.pro; do
    [ -e "$pro" ] || continue
    name="$(basename "$(dirname "$pro")")"
    out="$repo/build/tests/$name"
    count=$((count + 1))
    echo "== $name"
    mkdir -p "$out"
    if ! ( cd "$out" && qmake-qt5 "$pro" CONFIG+=release >/dev/null && mingw32-make -j"$(nproc)" >/dev/null ); then
        echo "FAIL: $name did not build"
        failed=$((failed + 1))
        continue
    fi
    exe="$(find "$out" -name "$name.exe" -o -name "$name" -type f | head -n 1)"
    if [ -z "$exe" ]; then
        echo "FAIL: $name produced no program"
        failed=$((failed + 1))
        continue
    fi
    if ! ( cd "$out" && PATH="$out:$PATH" "$exe" "$(cygpath -m "$repo")" ); then
        failed=$((failed + 1))
    fi
done

echo "== $count test(s), $failed failed"
if [ "$count" -eq 0 ] || [ "$failed" -ne 0 ]; then
    echo FAIL
    exit 1
fi
echo PASS
