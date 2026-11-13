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

#ifndef FORCELIMITS_H
#define FORCELIMITS_H

// The force-limit engine (decision record 0014, points 8 to 11): given what
// one rider feels over a ride (three components of the specific force against
// time), it says where and by how much the acceleration-duration limits of
// ASTM F2291 are met or broken. A pure computation on a trace: it knows
// nothing of tracks or trains, registers no check and touches no window.
// QtCore only; tests/forcelimits builds it alone.
//
// Every limit number lives in a data file (data/limits/astm-f2291-public.json,
// embedded as the resource defaultPath()), never in this code. Where each
// number comes from: docs/research/r3-force-limits.md (R3) and
// docs/research/r13-force-limits-second-sources.md (R13). Values are in g and
// seconds, computed in double.
//
// checkForceLimits() runs these steps, each a function of its own below:
//   1. limitFilterApply: the 5 Hz four-pole Butterworth low-pass, delay taken out
//   2. limitStepDuration: duration by slicing at 0.1 g levels, against the curves
//   3. limitStepEggs: the two-axis quarter-ellipses ("eggs")
//   4. limitStepReversal: fast reversals in x and y
//   5. limitStepAirtime: the two rules from airtime into positive z
//   6. limitStepRestraint: the restraint area (1 to 5) from the 1 Hz trace, as information
//
// Member prefixes as elsewhere, plus d for double.

#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

// ---------------------------------------------------------------- the data

// A number of the data file with its stable name (the file's "id"), where it
// came from, and whether the owner has checked it against his copy.
struct limitNumber
{
    limitNumber() : dValue(0.0), bPresent(false) {}
    QString sId;            // "combination.radius_factor"
    double dValue;
    bool bPresent;          // false when the file has null (an open number)
    QString sSource;
    QString sConfidence;    // "high", "medium" or "low", as the file says
    QString sChecked;       // empty, or the owner's note of where he checked it
    // "checked" once the owner has filled in his note, else sConfidence
    QString confidence() const { return sChecked.isEmpty() ? sConfidence : QStringLiteral("checked"); }
};

struct limitPoint
{
    limitPoint() : dTime(0.0), dLimit(0.0) {}
    QString sId;            // "z_pos@2.0s"
    double dTime;           // s
    double dLimit;          // g, signed (negative for the negative curves, the magnitude for "both")
    QString sSource;
    QString sConfidence;
    QString sChecked;
    QString confidence() const { return sChecked.isEmpty() ? sConfidence : QStringLiteral("checked"); }
};

struct limitCurve
{
    limitCurve() : iSign(1) {}
    QString sId;            // "z_pos"
    QString sName;          // "+Gz (eyes down)"
    QString sAxis;          // "x", "y" or "z"
    int iSign;              // +1 positive limits, -1 negative, 0 both directions (the magnitude is given)
    QList<limitPoint> lPoints;  // in time order
    QString sThen;          // a curve that applies after the last point, or empty (the last value holds)
};

struct limitPolygonPoint
{
    double dAx, dAz;
};

struct limitRegion
{
    QString sId;
    int iArea;              // 1 to 4: the region holds this area and every lower one
    QString sClass;
    QList<limitPolygonPoint> lPoints;
    QString sSource;
    QString sConfidence;    // at most what its corners allow (approximate: medium, open: low)
    QString sChecked;
    QString confidence() const { return sChecked.isEmpty() ? sConfidence : QStringLiteral("checked"); }
};

struct limitRestraintKind
{
    QString sId;            // "base", "otsr", "prone"
    QString sName;
    QString sNegXCurve;     // the eyes-front duration curve
    QString sEggNegXCurve;  // the curve whose min_event_s value is the egg's -x radius
};

// One row of the file's numbers, for listing (fvdcore limits).
struct limitListed
{
    QString sId, sValue, sSource, sConfidence, sChecked;
};

class forceLimits
{
public:
    forceLimits();

    // Path of the limit set that ships inside the program (a Qt resource).
    static QString defaultPath();

    // Reads and checks a limit file (a file path or a Qt resource path). On the
    // first problem it returns false and, if error is not null, names the place
    // in the file ("curves.z_pos.points[3].s: ...").
    static bool load(const QString& path, forceLimits& out, QString* error = nullptr);

    // The set that ships inside the program, loaded once and kept. Not valid
    // (and logged with qWarning) if it cannot be read.
    static const forceLimits& defaultSet();

    bool isValid() const { return bValid; }

    // The curve with this id, or nullptr.
    const limitCurve* curve(const QString& id) const;

    // The limit of a curve for an event of this duration (R3 section 2): the
    // first value from min_event_s to the first point, straight lines between
    // points, after the last point the last value (or the curve named in
    // "then"), and durations beyond max_duration_s judged at max_duration_s.
    // Signed as the curve is. confidence, if not null, gets the lower
    // confidence of the points used.
    double limitAt(const limitCurve& c, double duration, QString* confidence = nullptr) const;

    // The restraint kind for a word ("base", "otsr", "prone", or an alias of
    // the train catalogue such as "lapbar" or ""), or nullptr.
    const limitRestraintKind* restraintKind(const QString& word) const;

    // The lower of two confidences (checked > high > medium > low).
    static QString lowerConfidence(const QString& a, const QString& b);

    bool bValid;
    QString sPath;
    QString sId, sName, sStandard, sEdition, sEditionNote, sDisclaimer;
    QMap<QString, QString> lSources;        // key ("R") to description
    QList<limitListed> lNumbers;            // every number of the file, in file order

    // filters
    int iFilterOrder;
    limitNumber filterCorner;
    int iRestraintFilterOrder;
    limitNumber restraintFilterCorner, restraintLineTolerance;

    limitNumber minEvent, maxDuration, sliceStep;
    QMap<QString, limitCurve> lCurves;
    QMap<QString, limitRestraintKind> lRestraintKinds;
    QMap<QString, QString> lRestraintAliases;   // word -> kind id
    QString sRestraintKindsConfidence;

    // push-pull (the curve z_pos_after_neg)
    limitNumber pushPullTrigger, pushPullBelow, pushPullWindow;

    // combination ("eggs")
    QString sEggXPos, sEggY, sEggZPos, sEggZNeg, sEggZNegExtended;   // curve ids of the radii
    QString sEggRadiiConfidence;
    QStringList lEggPairs;                  // "xz", "xy", "yz"
    QMap<QString, QString> lEggPairConfidence;
    limitNumber eggRadiusFactor, eggSustained;

    // reversal
    QStringList lReversalAxes;              // "x", "y"
    limitNumber reversalGap, reversalFactor, reversalMinEvent, reversalThreshold;

    // weightless into positive z
    limitNumber weightlessAtOrBelow, weightlessMin, weightlessThreshold, weightlessMinRise;

    // restraint areas
    QList<limitRegion> lRegions;            // areas 1 to 4 in order
};

// ---------------------------------------------------------- input, options

// What one rider feels, in the patron axes of R3 section 1, gravity included:
// gx positive pressed into the seat back (eyes back), gy with the sign of the
// program's lateral force, gz positive pressed into the seat (the program's
// normal force; +1 sitting still on level track). Samples dt seconds apart.
struct forceTrace
{
    forceTrace() : dt(0.0) {}
    double dt;
    QVector<double> gx, gy, gz;
    int size() const { return gz.size(); }
};

struct limitOptions
{
    limitOptions() : bExtendedNegZ(false), bFilter(true) {}

    QString sRestraint;         // a restraint kind or alias: selects the eyes-front line and the egg's -x radius
    bool bExtendedNegZ;         // allow the extended -Gz line (special restraints): the airtime line and the egg's -z radius
    bool bFilter;               // filter at the set's corner (5 Hz) before judging; off only for tests of exact shapes
};

// ------------------------------------------------------------------ result

enum limitFamily
{
    limitFamilyX = 0,       // duration curves in x
    limitFamilyY,           // duration curves in y
    limitFamilyZ,           // duration curves in z (outside the push-pull windows)
    limitFamilyEgg,         // the two-axis eggs
    limitFamilyReversal,    // fast reversals in x and y
    limitFamilyAirtime,     // push-pull curve and weightless-to-positive rule
    LIMIT_FAMILIES
};

// One event that breaks a limit. Samples are indexes into the trace, at the
// place the force is (the filter's delay is taken out).
struct limitFinding
{
    limitFinding() : iFamily(0), iDirection(0), iFirst(0), iLast(0), iWorstSample(0),
        dLevel(0.0), dLimit(0.0), dDuration(0.0), dUtilisation(0.0) {}

    QString sRule;          // curve id ("z_pos", "x_neg_otsr"), "egg_xz", "reversal_y", "weightless_to_pos_z"
    int iFamily;            // limitFamily
    QString sAxis;          // "x", "y", "z", or a pair "xz"
    int iDirection;         // +1, -1, or 0 for an egg
    int iFirst, iLast;      // the stretch, inclusive
    int iWorstSample;       // where it is worst
    double dLevel;          // what was judged: g for a curve or a reversal peak; the share of the
                            // ellipse for an egg; the rise time in s for the weightless rule
    double dLimit;          // the limit for it (same unit)
    double dDuration;       // s: the event's duration (for a reversal the time between the peaks)
    double dUtilisation;    // the share of the limit; 1.0 is exactly at the limit
    QString sConfidence;    // the lowest confidence of the numbers used
    QString sText;          // one plain sentence
};

struct limitWorst
{
    limitWorst() : dUtil(0.0), iSample(-1), iRule(-1) {}
    double dUtil;
    int iSample;
    int iRule;              // index into limitResult::lRuleIds, -1 for none
};

struct limitResult
{
    limitResult() : bValid(false), bPass(true), iRestraintArea(0), iRestraintSample(-1),
        dFilterDelay(0.0), iFilterShift(0) {}

    bool bValid;                // false: the check could not run (sError says why)
    QString sError;
    bool bPass;                 // no finding
    QList<limitFinding> lFindings;  // in order of first sample

    // Per sample: the share of the nearest limit (1.0 = at the limit) and the
    // rule responsible (index into lRuleIds, -1 where no rule applies), one
    // array per family and the largest of them. Computed in double, kept as
    // float (a share is shown to two decimals) and a rule as one byte (the
    // engine has about fifteen rules): smaller arrays are quicker to fill.
    QStringList lRuleIds;
    QVector<float> lUtil[LIMIT_FAMILIES];
    QVector<qint8> lRule[LIMIT_FAMILIES];
    QVector<float> lUtilMax;
    QVector<qint8> lRuleMax;
    limitWorst worst;                       // over the whole trace
    limitWorst worstFamily[LIMIT_FAMILIES];

    QVector<qint8> lRestraintArea;  // per sample, 1 to 5, from the 1 Hz trace
    int iRestraintArea;         // the highest area reached (information, not a finding)
    int iRestraintSample;       // where it is first reached, -1 when the step did not run
    QString sRestraintConfidence;   // of the boundaries that decide that area and of the 1 Hz filter's numbers

    double dFilterDelay;        // s: the low-frequency delay of the 5 Hz filter (taken out)
    int iFilterShift;           // the same in whole samples
    QStringList lNotes;         // plain remarks

    // The index of a rule id in lRuleIds, added if new.
    int ruleIndex(const QString& id);
    QString ruleName(int index) const { return index >= 0 && index < lRuleIds.size() ? lRuleIds.at(index) : QString(); }
};

// The name of a family for listings ("x", "y", "z", "egg", "reversal", "airtime").
QString limitFamilyName(int family);

// ------------------------------------------------------------------ steps

// A Butterworth low-pass of even order as cascaded biquads, built by the
// bilinear transform with the corner pre-warped, so that the gain at the
// corner is exactly -3 dB. Single pass (causal).
struct limitBiquad
{
    double b0, b1, b2, a1, a2;
};
struct limitFilter
{
    limitFilter() : bValid(false), dt(0.0) {}
    bool bValid;                // false for an odd order or a corner at or above half the sample rate
    double dt;
    QList<limitBiquad> lSections;
};
limitFilter limitFilterDesign(int order, double cornerHz, double dt);
// The delay of the filter at low frequency, in samples (the group delay at 0 Hz).
double limitFilterDelay(const limitFilter& f);
// Filters x, starting from rest at its first value. With removeDelay the
// output is moved earlier by the low-frequency delay rounded to whole samples
// (the input is continued with its last value to fill the end), so a step
// stays where it was; *shift gets the samples moved. Taking the delay out is
// FVD++'s choice: the standard filters measured data and says nothing of it.
QVector<double> limitFilterApply(const limitFilter& f, const QVector<double>& x, bool removeDelay, int* shift = nullptr);
// The same for count traces of one length at once (faster than one by one):
// *out[k] gets the filtered *in[k] (an out must not be one of the ins).
void limitFilterApplyMany(const limitFilter& f, const QVector<double>* const* in, QVector<double>* const* out,
                          int count, bool removeDelay, int* shift = nullptr);

// One slice event: a stretch where the trace stays at or beyond a level. The
// events of one direction nest; the parent is the event one level lower that
// contains it (-1 for the first level). Parents come before their children.
struct limitSliceEvent
{
    int iLevel;             // the level is iLevel * step (a magnitude)
    int iFirst, iLast;      // inclusive
    int iParent;
};
// All slice events of one direction (sign +1 or -1) of values, R3 section 3.
// deepest, if not null, gets per sample the deepest event containing it, or -1.
QVector<limitSliceEvent> limitSlice(const QVector<double>& values, int sign, double step, QVector<int>* deepest = nullptr);

// Prepares a result for n samples (every share 0, no rule).
void limitResultInit(limitResult& r, int n);

// The steps on a trace that is already filtered (or deliberately not). Each
// adds its findings and fills its families' arrays; false and r.sError when
// the options do not fit the set. checkForceLimits runs them in order and
// then limitResultFinish.
bool limitStepDuration(const forceTrace& t, const forceLimits& s, const limitOptions& o, limitResult& r);
bool limitStepEggs(const forceTrace& t, const forceLimits& s, const limitOptions& o, limitResult& r);
bool limitStepReversal(const forceTrace& t, const forceLimits& s, const limitOptions& o, limitResult& r);
bool limitStepAirtime(const forceTrace& t, const forceLimits& s, const limitOptions& o, limitResult& r);
// The restraint step takes the unfiltered trace: it filters at 1 Hz itself.
bool limitStepRestraint(const forceTrace& raw, const forceLimits& s, limitResult& r);
// Sorts the findings and fills lUtilMax, lRuleMax, the worst values and bPass.
void limitResultFinish(limitResult& r);

// The restraint area (1 to 5) of one sustained (ax, az) point.
int limitRestraintArea(double ax, double az, const forceLimits& s);

// The windows after airtime in which the push-pull curve applies: for every
// stretch of trigger_s or more with gz below neg_below_g, the samples that
// follow it for window_s, as (first, last).
QList<QPair<int, int> > limitPushPullWindows(const QVector<double>& gz, double dt, const forceLimits& s);

// The whole check.
limitResult checkForceLimits(const forceTrace& trace, const forceLimits& limits, const limitOptions& options);

#endif // FORCELIMITS_H
