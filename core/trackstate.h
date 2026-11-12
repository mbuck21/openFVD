#ifndef TRACKSTATE_H
#define TRACKSTATE_H

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

#include <QByteArray>
#include <QList>
#include <QString>

class track;

// Everything needed to put a track back exactly as it was: a value, copied
// cheaply (its byte arrays are implicitly shared). The base of undo
// (docs/decisions/0011-undo-by-snapshot-commands-on-top.md).
//
// It holds the bytes track::saveTrack() writes, in three parts so that
// successive states share what did not change, plus the live values those
// bytes do not hold. A new field of the model must be saved, added here as a
// live value, or derived from the others; otherwise a restore loses it.
//
// Not held: the control points of freeform transitions (subfunc::pointList).
// They are not saved either, and no window can make a freeform transition.
class trackState
{
public:
    trackState();

    // The saved bytes, cut where saveTrack() writes them: header + every
    // section + smoothing is byte for byte what saveTrack() writes.
    QByteArray header;              // "TRC" to the number of sections (track::saveTrackHeader())
    QList<QByteArray> sections;     // one per section (section::saveSection())
    QByteArray smoothing;           // the smoothing entries and "EOT" (track::saveTrackSmoothing())

    // Live values the saved bytes do not hold.
    QByteArray anchorNode;          // *track::anchorNode as it is in memory (raw bytes of the mnode)
    QByteArray firstNode;           // lSections[0]->lNodes[0]: a copy of the anchor, made with the
                                    // section and again by every anchor edit (track::updateFromAnchor()),
                                    // but roll smoothing that starts at the first node turns it;
                                    // empty with no sections. A restore computes from the anchor
                                    // (updateFromAnchor()), as a load does, and keeps that when it
                                    // gives this node back; else it computes from this node
    QList<QByteArray> bezierPoints; // per section: every bezier_t of a Bezier section as raw bytes,
                                    // else empty. The file holds only some fields; fVel (a point's
                                    // speed, set by the NoLimits 1 importer, zeroed by a load) and the
                                    // fields secbezier::updateSection() reads before or without
                                    // writing them (the last point's length and ptf, fvdRoll) are not
                                    // in it, and the point-list importer leaves flags unset that a
                                    // load would turn into 0 or 1
    QList<QString> unsavedNames;    // per section: sName of a NoLimits CSV section (its saveSection()
                                    // writes no name), else a null string
    int smoothedUntil;              // track::smoothedUntil
    bool bAutoSmooth;               // track::bAutoSmooth

    bool isNull() const { return header.isEmpty(); }

    // What track::saveTrack() writes for this state.
    QByteArray savedBytes() const;

    // Every byte this state refers to, shared or not.
    qint64 byteSize() const;

    // The bytes of this state that are not shared with other (the extra
    // memory this state costs when other is kept too).
    qint64 bytesNotSharedWith(const trackState& other) const;

    bool operator==(const trackState& other) const;
    bool operator!=(const trackState& other) const { return !(*this == other); }
};

// The state of t now. When previous is given (normally the state taken
// before the last edit), every part whose bytes equal previous's shares
// previous's data instead of holding a copy. Writes nothing to t.
trackState captureTrackState(track* t, const trackState* previous = NULL);

// Puts state back into t, in place (the same track object, new sections,
// smoothing entries and nodes), and computes the whole track with
// updateTrack(0, 0). Tells t->mListener trackAboutToReset() before and
// trackReset() after, and nothing in between. Keeps t's povPos and
// drawTrack (view state, not design) and its listener.
void restoreTrackState(track* t, const trackState& state);

#endif // TRACKSTATE_H
