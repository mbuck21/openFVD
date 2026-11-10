#ifndef TRACKLISTENER_H
#define TRACKLISTENER_H

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

#include <QString>

class track;

// What a track tells whoever shows it. The track calls these synchronously,
// at the moment its geometry changes; a track without a listener (mListener
// is NULL) calls nothing.
class trackListener
{
public:
    virtual ~trackListener() {}

    // The track's nodes changed from node fromNode to the end.
    virtual void trackGeometryChanged(track* which, int fromNode) = 0;

    // A short status text, to be shown for timeoutMs milliseconds.
    virtual void trackStatusMessage(const QString& text, int timeoutMs) = 0;
};

#endif // TRACKLISTENER_H
