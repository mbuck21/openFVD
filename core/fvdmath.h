/*
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
*/

#ifndef FVDMATH_H
#define FVDMATH_H

// Maths functions that must give the same last bit as the 2016 release
// binary (bin/win64/FVD.exe). They do not use the C runtime's float versions,
// which differ between runtimes; they do use its double-precision log, log1p,
// sqrt and asin, and were proven against the 2016 machine code on Windows'
// current runtime only. On another runtime or platform, run tests/fvdmath
// before trusting them (docs/research/n2-matching-the-2016-maths.md).
//
// The 2016 binary linked its float maths statically from the mingw-w64
// runtime of the time; a current build gets them from the C runtime it runs
// on (today Windows' Universal C Runtime), which rounds some results
// differently. Where such a difference survives into the track, the rebuilt
// program writes different files from the 2016 one. The functions here
// compute exactly what the 2016 binary computed, using only double-precision
// arithmetic, so they do not depend on the runtime's float functions or on the
// x87 control word. docs/research/n2-matching-the-2016-maths.md has the
// evidence (each one was compared with the 2016 binary's own machine code on
// every float argument) and docs/decisions/0008 the decision.
//
// Only functions with that proof belong here. Call them where the 2016
// result matters; do not replace other maths calls with them.

namespace fvdmath {

// asinh of a float, as the 2016 binary's asinhf computed it (mingw-w64's
// formula log1p(|x| + x*x / (sqrt(x*x + 1) + 1)) with x*x in float and the
// rest in double). Used by subfunc::applyTension() for negative tension.
float asinh2016(float x);

// asin of a float, as the 2016 binary's asinf computed it: identical to the
// double-precision asin rounded to float on every argument. Used by
// section::calcDirFromLast().
float asin2016(float x);

}

#endif // FVDMATH_H
