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

#include "fvdmath.h"
#include <cmath>
#include <cfloat>

// Written from the machine code of the 2016 binary (bin/win64/FVD.exe). The
// asinh formula is the one in mingw-w64's mingw-w64-crt/math/asinhf.c (v5.0.0),
// which that file places in the public domain. Each function was checked
// against that machine code on every float bit pattern (2^32 of them); see
// docs/research/n2-matching-the-2016-maths.md. Keep every operation and its
// precision as it is: a float that becomes a double here, or the other way
// round, changes results.

namespace fvdmath {

float asinh2016(float x)
{
    // Infinity and NaN come back unchanged, as in 2016.
    if(!(std::fabs(x) <= FLT_MAX)) {
        return x;
    }
    // The square is rounded to float before anything else; this is what
    // makes the 2016 result differ from the correctly rounded one.
    const float xx = x*x;
    const double z2 = xx;
    const double w = (double)std::fabs(x) + z2/(std::sqrt(z2 + 1.0) + 1.0);
    // The 2016 code used log(1 + w) above this bound and log1p(w) below it
    // (1 - sqrt(2)/2, the range of the x87 instruction it used).
    const double r = (std::fabs(w) >= 0.29289321881345243) ? std::log(w + 1.0) : std::log1p(w);
    const float f = (float)r;
    return x > 0.f ? f : -f;
}

float asin2016(float x)
{
    return (float)std::asin((double)x);
}

}
