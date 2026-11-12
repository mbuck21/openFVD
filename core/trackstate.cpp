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

#include "trackstate.h"
#include "track.h"
#include "smoothhandler.h"
#include "exportfuncs.h"
#include "function.h"
#include "subfunction.h"

#include <QSet>
#include <cstring>
#include <sstream>
#include <string>
#include <type_traits>

// An mnode is a plain value of floats, kept as its raw bytes so that two
// states compare bit for bit (NaN included) and come back bit for bit.
static_assert(std::is_trivially_copyable<mnode>::value, "trackState copies mnode as raw bytes");
static_assert(std::is_trivially_copyable<bezier_t>::value, "trackState copies bezier_t as raw bytes");

namespace {

QByteArray nodeBytes(const mnode& node)
{
    return QByteArray(reinterpret_cast<const char*>(&node), sizeof(mnode));
}

void putNode(mnode* node, const QByteArray& bytes)
{
    lenAssert(bytes.size() == (int)sizeof(mnode));
    if(bytes.size() == (int)sizeof(mnode)) {
        memcpy(static_cast<void*>(node), bytes.constData(), sizeof(mnode));
    }
}

// A plain std::iostream over a string buffer. Deliberately not a
// std::stringstream: several save and load functions have a second overload
// taking std::stringstream& that writes the old undo's private format, not
// the file format (secforced's drops fVel and bSpeed). Through this stream
// the std::iostream& overloads, the file format, are always the ones called.
class byteStream
{
public:
    byteStream() : buf(std::ios::in | std::ios::out | std::ios::binary), io(&buf) {}
    explicit byteStream(const QByteArray& bytes)
        : buf(std::string(bytes.constData(), bytes.size()), std::ios::in | std::ios::binary), io(&buf) {}

    std::iostream& stream() { return io; }
    QByteArray bytes() const
    {
        std::string s = buf.str();
        return QByteArray(s.data(), (int)s.size());
    }

private:
    std::stringbuf buf;
    std::iostream io;
};

// fresh when it equals old (then old's data is shared), else fresh.
QByteArray shareIfEqual(const QByteArray& fresh, const QByteArray* old)
{
    if(old != NULL && *old == fresh) return *old;
    return fresh;
}

// fresh, sharing the data of the entry of old at index i, or else at i+shift
// (the same distance from the end), when that one holds the same bytes.
QByteArray shareFromList(const QByteArray& fresh, const QList<QByteArray>* old, int i, int shift)
{
    if(old == NULL || fresh.isEmpty()) return fresh;
    if(i < old->size() && old->at(i) == fresh) return old->at(i);
    if(shift != 0 && i+shift >= 0 && i+shift < old->size() && old->at(i+shift) == fresh) return old->at(i+shift);
    return fresh;
}

// The transition functions a section saves, in the order its saveSection()
// writes them; they are the last bytes of the section. A Bezier section has
// none; a NoLimits CSV section has a roll function but does not save it.
QList<func*> savedFunctions(section* sec)
{
    QList<func*> funcs;
    if(sec->type == straight || sec->type == curved) {
        funcs << sec->rollFunc;
    } else if(sec->type == forced || sec->type == geometric) {
        funcs << sec->rollFunc << sec->normForce << sec->latForce;
    }
    return funcs;
}

// Writes every saved field of every transition piece of sec as it is in the
// section's saved bytes, read straight into the pieces with no arithmetic.
//
// Why a restore does not trust the load path for this: func::loadFunction()
// appends each further piece with appendSubFunction(), whose closing loop
// recomputes the end of every piece after the first as
// (prev->maxArgument + cur->maxArgument) - cur->minArgument. In single
// precision that is not always cur->maxArgument, even when cur->minArgument
// equals prev->maxArgument. Example: a piece from 4.9999986 (0x409FFFFD) to
// 5.999999 (0x40BFFFFE) comes out ending at 5.9999995 (0x40BFFFFF), one last
// digit up, and the piece after it (read afterwards, as saved) still starts
// at 5.999999. A file load keeps doing exactly that (the comparison with the
// 2016 binary depends on it); a restore must give back the model as it was.
void putTransitionsBack(section* sec, const QByteArray& bytes)
{
    QList<func*> funcs = savedFunctions(sec);
    if(funcs.isEmpty()) return;

    // how many bytes the functions take (their size does not depend on the values)
    int size = 0;
    for(int f = 0; f < funcs.size(); ++f) {
        byteStream out;
        funcs[f]->saveFunction(out.stream());
        size += out.bytes().size();
    }
    lenAssert(size <= bytes.size());
    if(size > bytes.size()) return;

    byteStream in(bytes.right(size));
    std::iostream& file = in.stream();
    for(int f = 0; f < funcs.size(); ++f) {
        func* fn = funcs[f];
        std::string tag = readString(&file, 4);
        int count = readInt(&file);
        lenAssert(tag == "FUNC" && count == fn->funcList.size());
        if(tag != "FUNC" || count != fn->funcList.size()) return;
        for(int p = 0; p < count; ++p) {
            fn->funcList[p]->loadSubFunc(file);   // the std::iostream overload: reads only
        }
    }
}

// Silences a track's listener while it lives (the windows are not told about
// the half-built states of a restore) and gives it back afterwards.
class listenerMute
{
public:
    explicit listenerMute(track* t) : mTrack(t), mSaved(t->mListener) { t->mListener = NULL; }
    ~listenerMute() { mTrack->mListener = mSaved; }

private:
    track* mTrack;
    trackListener* mSaved;
};

} // namespace

trackState::trackState()
    : smoothedUntil(0), bAutoSmooth(false)
{
}

QByteArray trackState::savedBytes() const
{
    QByteArray all = header;
    for(int i = 0; i < sections.size(); ++i) {
        all += sections[i];
    }
    all += smoothing;
    return all;
}

qint64 trackState::byteSize() const
{
    qint64 size = header.size() + smoothing.size() + anchorNode.size() + firstNode.size();
    for(int i = 0; i < sections.size(); ++i) {
        size += sections[i].size();
    }
    for(int i = 0; i < bezierPoints.size(); ++i) {
        size += bezierPoints[i].size();
    }
    for(int i = 0; i < unsavedNames.size(); ++i) {
        size += unsavedNames[i].size()*(qint64)sizeof(QChar);
    }
    return size + sizeof(trackState);
}

qint64 trackState::bytesNotSharedWith(const trackState& other) const
{
    QSet<const char*> shared;
    shared.insert(other.header.constData());
    shared.insert(other.smoothing.constData());
    for(int i = 0; i < other.sections.size(); ++i) {
        shared.insert(other.sections[i].constData());
    }

    qint64 size = byteSize();
    if(shared.contains(header.constData())) size -= header.size();
    if(shared.contains(smoothing.constData())) size -= smoothing.size();
    for(int i = 0; i < sections.size(); ++i) {
        if(shared.contains(sections[i].constData())) size -= sections[i].size();
    }
    return size;
}

bool trackState::operator==(const trackState& other) const
{
    return header == other.header
        && sections == other.sections
        && smoothing == other.smoothing
        && anchorNode == other.anchorNode
        && firstNode == other.firstNode
        && bezierPoints == other.bezierPoints
        && unsavedNames == other.unsavedNames
        && smoothedUntil == other.smoothedUntil
        && bAutoSmooth == other.bAutoSmooth;
}

trackState captureTrackState(track* t, const trackState* previous)
{
    trackState state;

    {
        byteStream out;
        t->saveTrackHeader(out.stream());
        state.header = shareIfEqual(out.bytes(), previous ? &previous->header : NULL);
    }

    // A section is compared with the one at the same place in previous and,
    // when sections were added or removed, with the one as far from the end;
    // so an edit at one place leaves the others shared either side of it.
    int shift = previous ? previous->sections.size() - t->lSections.size() : 0;
    for(int i = 0; i < t->lSections.size(); ++i) {
        section* sec = t->lSections[i];
        byteStream out;
        sec->saveSection(out.stream());
        state.sections.append(shareFromList(out.bytes(), previous ? &previous->sections : NULL, i, shift));

        QByteArray points;
        if(sec->type == bezier) {
            for(int b = 0; b < sec->bezList.size(); ++b) {
                points.append(reinterpret_cast<const char*>(sec->bezList[b]), sizeof(bezier_t));
            }
        }
        state.bezierPoints.append(shareFromList(points, previous ? &previous->bezierPoints : NULL, i, shift));

        state.unsavedNames.append(sec->type == nolimitscsv ? sec->sName : QString());
    }

    {
        byteStream out;
        t->saveTrackSmoothing(out.stream());
        state.smoothing = shareIfEqual(out.bytes(), previous ? &previous->smoothing : NULL);
    }

    state.anchorNode = nodeBytes(*t->anchorNode);
    if(!t->lSections.isEmpty()) {
        state.firstNode = nodeBytes(t->lSections[0]->lNodes[0]);
    }
    state.smoothedUntil = t->smoothedUntil;
    state.bAutoSmooth = t->bAutoSmooth;

    return state;
}

// Not a load: a load rewrites values the running program leaves alone
// (smoothHandler::loadSmooth() ends in update(), which takes an entry's name
// and node range from the track; readProject() switches bAutoSmooth on and
// applies the smoothing again). A restore reads the saved bytes with the same
// code a load uses, then puts the live values, the transition pieces and the
// smoothing entries' saved fields back as they were captured.
//
// What the recompute (updateFromAnchor(), or updateTrack(0, 0) from the
// captured first node; see below) then does to the saved fields:
// - a piece's start value (and a "to zero" piece's change) is set again from
//   the section's start node and the piece before, as every recompute does;
// - a straight's length, a curve's angle and a force or geometric section's
//   iTime are set again from the last piece's end, which is as captured;
// - a locked piece is stretched (subfunc::getValue() calls changeLength()) to
//   min + (section end - min), from values that are as captured, so to what
//   the last recompute before the capture gave; pieces after a locked one go
//   through the same sum as above, but every recompute runs it at each node,
//   so a captured state is already where it settles;
// - nothing calls setMaxArgument(); the curve's constructor and the force
//   section's changeLength(1, 0) run before the load overwrites the pieces.
// So a state captured after a recompute comes back with the same saved bytes.
void restoreTrackState(track* t, const trackState& state)
{
    lenAssert(!state.isNull());
    if(state.isNull()) return;

    if(t->mListener != NULL)
        t->mListener->trackAboutToReset(t);

    {
        listenerMute mute(t);

        // view state, kept as it is
        glm::vec2 povPos = t->povPos;
        bool drawTrack = t->drawTrack;

        // Each smoothing entry leaves the list before it is deleted: its
        // destructor tells its listener, which must not find it in the list.
        while(!t->smoothList.isEmpty()) {
            smoothHandler* entry = t->smoothList.takeLast();
            delete entry;
        }
        while(!t->lSections.isEmpty()) {
            section* sec = t->lSections.takeLast();
            delete sec;
        }
        t->activeSection = NULL;
        t->smoothedUntil = 0;
        t->smoothList.append(new smoothHandler(t, -1));   // as track::track() makes it

        byteStream in(state.savedBytes());
        std::iostream& file = in.stream();

        std::string tag = readString(&file, 3);
        lenAssert(tag == "TRC");
        t->loadTrackHeader(file);
        putNode(t->anchorNode, state.anchorNode);

        QString error = t->loadTrackSections(file, false);   // updateTrack(0, 0) below computes them
        lenAssert(error.isEmpty());
        tag = readString(&file, 3);
        lenAssert(tag == "EOT");
        Q_UNUSED(error);

        lenAssert(t->lSections.size() == state.sections.size());
        for(int i = 0; i < t->lSections.size() && i < state.sections.size(); ++i) {
            section* sec = t->lSections[i];
            if(sec->type == bezier) {
                // every field of every point, the saved ones included (a
                // flag the importer left unset would otherwise come back as 0 or 1)
                const QByteArray& points = state.bezierPoints[i];
                lenAssert(points.size() == sec->bezList.size()*(int)sizeof(bezier_t));
                for(int b = 0; b < sec->bezList.size() && (b+1)*(int)sizeof(bezier_t) <= points.size(); ++b) {
                    memcpy(static_cast<void*>(sec->bezList[b]), points.constData() + b*sizeof(bezier_t), sizeof(bezier_t));
                }
            } else if(sec->type == nolimitscsv) {
                sec->sName = state.unsavedNames[i];
            }
            // The transition pieces exactly as captured: the load above can
            // move a piece's end by one last digit (putTransitionsBack()).
            putTransitionsBack(sec, state.sections[i]);
        }

        if(!t->lSections.isEmpty()) {
            putNode(&t->lSections[0]->lNodes[0], state.firstNode);
        }
        t->smoothedUntil = state.smoothedUntil;
        t->bAutoSmooth = state.bAutoSmooth;

        // First the track as a load computes it, from the anchor
        // (updateFromAnchor()). When that gives the first node as captured,
        // that is the state: what a load and every command since decision
        // record 0013 leave. (Restoring the captured first node and computing
        // from it would not: when roll smoothing turned that node, taking the
        // turn off again, removeSmooth(), is exact only to the last digits,
        // and every node after it inherits that.) Any other first node (one an
        // import made, or one an edit outside the commands left stale) is put
        // back as captured and the track computed from it.
        bool fromAnchor = false;
        if(!t->lSections.isEmpty()) {
            t->updateFromAnchor();
            fromAnchor = nodeBytes(t->lSections[0]->lNodes[0]) == state.firstNode;
        }
        if(!fromAnchor) {
            if(!t->lSections.isEmpty()) {
                putNode(&t->lSections[0]->lNodes[0], state.firstNode);
            }
            t->smoothedUntil = state.smoothedUntil;
            t->updateTrack(0, 0);
        }

        // The six saved fields of each smoothing entry, as captured; the load
        // above and updateTrack() rewrote some of them from the track.
        byteStream smooth(state.smoothing);
        std::iostream& sfile = smooth.stream();
        int count = readInt(&sfile);
        lenAssert(count == t->smoothList.size());
        for(int i = 0; i < count && i < t->smoothList.size(); ++i) {
            smoothHandler* entry = t->smoothList[i];
            int namelength = readInt(&sfile);
            entry->sName = QString(readString(&sfile, namelength).c_str());
            entry->setFrom(readInt(&sfile));
            entry->setTo(readInt(&sfile));
            entry->setLength(readInt(&sfile));
            entry->setIterations(readInt(&sfile));
            entry->active = readBool(&sfile);
        }

        t->povPos = povPos;
        t->drawTrack = drawTrack;
    }

    if(t->mListener != NULL)
        t->mListener->trackReset(t);
}
