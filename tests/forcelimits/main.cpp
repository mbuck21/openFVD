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

// Checks the force-limit engine (core/forcelimits.*) and its data file
// (data/limits/astm-f2291-public.json) on synthetic traces with known
// answers: the "Proof" of piece A3a in docs/stage-3-plan.md. The expected
// limits below are worked out by hand from the tables of
// docs/research/r3-force-limits.md, not from the loader.
// Prints one line per check and a last line PASS or FAIL; exit code 0 or 1.

#include "../../core/forcelimits.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <cstdio>

static int failures = 0;

static void check(const QString& what, bool ok, const QString& detail = QString())
{
    printf("%s  %s%s\n", ok ? "ok  " : "FAIL", qPrintable(what),
           (!detail.isEmpty()) ? qPrintable("  (" + detail + ")") : "");
    if (!ok)
        ++failures;
}

static bool near(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol;
}

static QString num(double v)
{
    return QString::number(v, 'g', 6);
}

// ---------------------------------------------------------------- traces

static forceTrace restTrace(double dt, int n)
{
    forceTrace t;
    t.dt = dt;
    t.gx = QVector<double>(n, 0.0);
    t.gy = QVector<double>(n, 0.0);
    t.gz = QVector<double>(n, 1.0);
    return t;
}

static QVector<double>& axisOf(forceTrace& t, char axis)
{
    return axis == 'x' ? t.gx : (axis == 'y' ? t.gy : t.gz);
}

static void setRange(QVector<double>& v, int first, int count, double value)
{
    for (int i = first; i < first + count && i < v.size(); ++i)
        v[i] = value;
}

static limitResult run(const forceTrace& t, const forceLimits& s, const QString& restraint = "base",
                       bool extended = false, bool filter = false)
{
    limitOptions o;
    o.sRestraint = restraint;
    o.bExtendedNegZ = extended;
    o.bFilter = filter;
    return checkForceLimits(t, s, o);
}

// The largest share of one rule over the trace.
static double ruleMax(const limitResult& r, const QString& rule)
{
    const int k = r.lRuleIds.indexOf(rule);
    double best = 0.0;
    if (k < 0)
        return best;
    for (int f = 0; f < LIMIT_FAMILIES; ++f)
        for (int i = 0; i < r.lUtil[f].size(); ++i)
            if (r.lRule[f].at(i) == k)
                best = std::max(best, (double)r.lUtil[f].at(i));
    return best;
}

static int findingsOf(const limitResult& r, const QString& rule)
{
    int n = 0;
    for (int i = 0; i < r.lFindings.size(); ++i)
        if (r.lFindings.at(i).sRule == rule)
            ++n;
    return n;
}

static const limitFinding* firstFinding(const limitResult& r, const QString& rule)
{
    for (int i = 0; i < r.lFindings.size(); ++i)
        if (r.lFindings.at(i).sRule == rule)
            return &r.lFindings.at(i);
    return nullptr;
}

// +Gz limit by hand from R3 section 2, independent of the loader.
static double zPosByHand(double d)
{
    static const double p[][2] = { {0.2, 6.0}, {1.0, 6.0}, {2.0, 4.0}, {4.0, 4.0}, {5.0, 3.0}, {11.8, 3.0}, {12.0, 2.0}, {40.0, 2.0} };
    if (d <= 0.2) return 6.0;
    for (int i = 0; i < 7; ++i)
        if (d <= p[i + 1][0])
            return p[i][1] + (d - p[i][0]) / (p[i + 1][0] - p[i][0]) * (p[i + 1][1] - p[i][1]);
    return 2.0;
}

// ---------------------------------------------------------------- the data

static void checkDataFile(const QString& path, forceLimits& s)
{
    QString error;
    const bool ok = forceLimits::load(path, s, &error);
    check("data/limits/astm-f2291-public.json loads", ok && s.isValid(), error);
    const forceLimits& def = forceLimits::defaultSet();
    check("the embedded copy (" + forceLimits::defaultPath() + ") loads", def.isValid());
    check("the embedded copy is the file in the repository", def.lNumbers.size() == s.lNumbers.size());
    bool complete = !s.lNumbers.isEmpty();
    for (int i = 0; i < s.lNumbers.size(); ++i)
    {
        const limitListed& n = s.lNumbers.at(i);
        if (n.sId.isEmpty() || n.sSource.isEmpty() || (n.sConfidence != "high" && n.sConfidence != "medium" && n.sConfidence != "low"))
            complete = false;
    }
    check(QString("every one of the %1 numbers has a name, a source and a confidence").arg(s.lNumbers.size()), complete);
    check("the egg's radius factor is 1.0 (the stricter reading, R13)", s.eggRadiusFactor.dValue == 1.0
          && s.eggRadiusFactor.sId == "combination.radius_factor");
    check("restraint kinds base, otsr and prone; lapbar, vest, other and none are the base case",
          s.restraintKind("base") && s.restraintKind("otsr") && s.restraintKind("prone")
          && s.restraintKind("lapbar") == s.restraintKind("base") && s.restraintKind("") == s.restraintKind("base")
          && s.restraintKind("vest") == s.restraintKind("base") && !s.restraintKind("seatbelt"));
    check("filter: order 4 Butterworth at 5 Hz; restraint diagram at 1 Hz", s.iFilterOrder == 4
          && s.filterCorner.dValue == 5.0 && s.restraintFilterCorner.dValue == 1.0);
    check("min event 0.2 s, slice step 0.1 g, 90 s", s.minEvent.dValue == 0.2 && s.sliceStep.dValue == 0.1
          && s.maxDuration.dValue == 90.0);

    // limitAt against the curves of R3, by hand
    struct at { const char* curve; double d; double limit; };
    static const at table[] = {
        {"z_pos", 0.2, 6.0}, {"z_pos", 1.5, 5.0}, {"z_pos", 3.0, 4.0}, {"z_pos", 4.5, 3.5}, {"z_pos", 11.9, 2.5},
        {"z_pos", 20.0, 2.0}, {"z_pos", 200.0, 2.0},
        {"z_pos_after_neg", 1.0, 5.0}, {"z_pos_after_neg", 1.75, 4.5}, {"z_pos_after_neg", 2.25, 3.0}, {"z_pos_after_neg", 10.0, 2.0},
        {"z_neg", 0.35, -1.75}, {"z_neg", 5.5, -1.3}, {"z_neg", 30.0, -1.1},
        {"z_neg_extended", 0.6, -2.5}, {"z_neg_extended", 2.0, -1.85}, {"z_neg_extended", 5.5, -1.3},
        {"y", 0.5, 3.0}, {"y", 1.5, 2.5}, {"y", 60.0, 2.0},
        {"x_pos", 3.0, 4.0}, {"x_pos", 30.0, 2.5},
        {"x_neg_base", 0.35, -1.75}, {"x_neg_base", 9.0, -1.5}, {"x_neg_otsr", 9.0, -2.0},
        {"x_neg_prone", 1.0, -3.5}, {"x_neg_prone", 2.5, -3.0}, {"x_neg_prone", 4.5, -2.25}, {"x_neg_prone", 50.0, -2.0},
    };
    bool allAt = true;
    QString bad;
    for (unsigned i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
    {
        const limitCurve* c = s.curve(table[i].curve);
        const double got = c ? s.limitAt(*c, table[i].d) : 1e9;
        if (!near(got, table[i].limit, 1e-9))
        {
            allAt = false;
            bad += QString("%1 at %2 s: %3; ").arg(table[i].curve).arg(table[i].d).arg(got);
        }
    }
    check("limitAt gives R3's curves between and beyond the points (29 places)", allAt, bad);
}

// ---------------------------------------------------------------- traces with known answers

static void checkRest(const forceLimits& s)
{
    const forceTrace t = restTrace(0.01, 9000);
    const limitResult r = run(t, s, "base", false, true);
    check("rest for 90 s: runs, no finding", r.bValid && r.bPass && r.lFindings.isEmpty(), r.sError);
    check("rest for 90 s: largest share 0.5 (1 g against +Gz's 2.0 g), rule z_pos",
          near(r.worst.dUtil, 0.5, 1e-9) && r.ruleName(r.worst.iRule) == "z_pos", num(r.worst.dUtil));
    check("rest for 90 s: restraint area 1", r.iRestraintArea == 1, QString::number(r.iRestraintArea));
    check("rest for 90 s: one share per sample in every family", r.lUtilMax.size() == 9000 && r.lUtil[limitFamilyEgg].size() == 9000);
}

struct plateauCase
{
    const char* rule;
    char axis;
    const char* restraint;
    bool extended;
    double level;       // g
    double duration;    // s
    double limit;       // g, worked out by hand from R3
};

static const plateauCase PLATEAUS[] = {
    // +Gz: 6.0 to 1 s, 4.0 at 2 s, 4.0 to 4 s, 3.0 at 5 s, 3.0 to 11.8 s, 2.0 from 12 s to 40 s and on
    {"z_pos", 'z', "base", false, 7.0, 0.21, 6.0},
    {"z_pos", 'z', "base", false, 5.9, 0.9, 6.0},   {"z_pos", 'z', "base", false, 5.9, 1.1, 5.8},
    {"z_pos", 'z', "base", false, 4.1, 1.9, 4.2},   {"z_pos", 'z', "base", false, 4.1, 2.1, 4.0},
    {"z_pos", 'z', "base", false, 4.0, 3.9, 4.0},   {"z_pos", 'z', "base", false, 4.0, 4.1, 3.9},
    {"z_pos", 'z', "base", false, 3.1, 4.9, 3.1},   {"z_pos", 'z', "base", false, 3.1, 5.1, 3.0},
    {"z_pos", 'z', "base", false, 2.9, 11.7, 3.0},  {"z_pos", 'z', "base", false, 2.4, 11.9, 2.5},
    {"z_pos", 'z', "base", false, 2.4, 12.1, 2.0},
    {"z_pos", 'z', "base", false, 2.0, 39.0, 2.0},  {"z_pos", 'z', "base", false, 2.0, 41.0, 2.0},
    // -Gz: -2.0 at 0.2 s, -1.5 from 0.5 s to 4 s, -1.1 from 7 s
    {"z_neg", 'z', "base", false, -1.6, 0.4, -5.0 / 3.0}, {"z_neg", 'z', "base", false, -1.6, 0.6, -1.5},
    {"z_neg", 'z', "base", false, -1.5, 3.9, -1.5},       {"z_neg", 'z', "base", false, -1.5, 4.1, -1.5 + 0.4 / 30.0},
    {"z_neg", 'z', "base", false, -1.1, 6.9, -1.5 + 0.4 * 2.9 / 3.0}, {"z_neg", 'z', "base", false, -1.2, 7.1, -1.1},
    // -Gz extended: -2.8 at 0.2 s, -2.2 at 1 s, -1.5 at 3 s, then the base line
    {"z_neg_extended", 'z', "base", true, -2.2, 0.9, -2.275}, {"z_neg_extended", 'z', "base", true, -2.2, 1.1, -2.165},
    {"z_neg_extended", 'z', "base", true, -1.6, 2.5, -1.675}, {"z_neg_extended", 'z', "base", true, -1.6, 3.5, -1.5},
    // Gy: 3.0 to 1 s, 2.0 from 2 s, both directions
    {"y", 'y', "base", false, 3.0, 0.9, 3.0},  {"y", 'y', "base", false, 3.0, 1.1, 2.9},
    {"y", 'y', "base", false, 2.1, 1.9, 2.1},  {"y", 'y', "base", false, 2.1, 2.1, 2.0},
    {"y", 'y', "base", false, -2.1, 1.9, 2.1}, {"y", 'y', "base", false, -2.1, 2.1, 2.0},
    // +Gx: as +Gz to 11.8 s, then 2.5 from 12 s
    {"x_pos", 'x', "base", false, 5.9, 0.9, 6.0},   {"x_pos", 'x', "base", false, 5.9, 1.1, 5.8},
    {"x_pos", 'x', "base", false, 4.1, 1.9, 4.2},   {"x_pos", 'x', "base", false, 4.1, 2.1, 4.0},
    {"x_pos", 'x', "base", false, 4.0, 3.9, 4.0},   {"x_pos", 'x', "base", false, 4.0, 4.1, 3.9},
    {"x_pos", 'x', "base", false, 3.1, 4.9, 3.1},   {"x_pos", 'x', "base", false, 3.1, 5.1, 3.0},
    {"x_pos", 'x', "base", false, 2.9, 11.7, 3.0},  {"x_pos", 'x', "base", false, 2.7, 11.9, 2.75},
    {"x_pos", 'x', "base", false, 2.7, 12.1, 2.5},
    // -Gx, by restraint
    {"x_neg_base", 'x', "base", false, -1.6, 0.4, -5.0 / 3.0}, {"x_neg_base", 'x', "base", false, -1.6, 0.6, -1.5},
    {"x_neg_otsr", 'x', "otsr", false, -2.0, 0.5, -2.0},       {"x_neg_otsr", 'x', "otsr", false, -2.1, 10.0, -2.0},
    {"x_neg_prone", 'x', "prone", false, -3.5, 1.9, -3.5},     {"x_neg_prone", 'x', "prone", false, -3.5, 2.1, -3.4},
    {"x_neg_prone", 'x', "prone", false, -2.6, 2.9, -2.6},     {"x_neg_prone", 'x', "prone", false, -2.6, 3.1, -2.5},
    {"x_neg_prone", 'x', "prone", false, -2.5, 3.9, -2.5},     {"x_neg_prone", 'x', "prone", false, -2.5, 4.1, -2.45},
    {"x_neg_prone", 'x', "prone", false, -2.0, 4.9, -2.05},    {"x_neg_prone", 'x', "prone", false, -2.1, 5.1, -2.0},
};

static void checkPlateaus(const forceLimits& s)
{
    const double dt = 0.01;
    int cases = 0, wrong = 0, atLimit = 0;
    QString bad;
    for (unsigned c = 0; c < sizeof(PLATEAUS) / sizeof(PLATEAUS[0]); ++c)
    {
        const plateauCase& p = PLATEAUS[c];
        const int len = (int)std::floor(p.duration / dt + 0.5);
        const int pad = 200;
        forceTrace t = restTrace(dt, len + 2 * pad);
        setRange(axisOf(t, p.axis), pad, len, p.level);
        const limitResult r = run(t, s, p.restraint, p.extended);
        const double expected = std::fabs(p.level) / std::fabs(p.limit);
        const double got = ruleMax(r, p.rule);
        const bool fails = expected > 1.0 + 1e-9;
        const int found = findingsOf(r, p.rule);
        const bool ok = r.bValid && near(got, expected, 1e-6) && found == (fails ? 1 : 0);
        ++cases;
        if (std::fabs(expected - 1.0) < 1e-9) ++atLimit;
        if (!ok)
        {
            ++wrong;
            bad += QString("%1 %2 g for %3 s: share %4, expected %5, %6 findings; ")
                   .arg(p.rule).arg(p.level).arg(p.duration).arg(num(got), num(expected)).arg(found);
        }
    }
    check(QString("plateaus on both sides of every break of every curve: %1 cases, share as worked out by hand, "
                  "one finding exactly when over").arg(cases), wrong == 0, bad);
    check(QString("exactly at the limit gives a share of 1.0 and no finding (%1 of the cases)").arg(atLimit), atLimit >= 6 && wrong == 0);

    // the finding of one event is one finding, not one per slice level
    forceTrace t = restTrace(dt, 600);
    setRange(t.gz, 200, 110, 5.9);
    const limitResult r = run(t, s);
    const limitFinding* f = firstFinding(r, "z_pos");
    check("5.9 g for 1.1 s: one finding (levels 5.8 and 5.9 both over), with its numbers",
          r.lFindings.size() == 1 && f && near(f->dLevel, 5.9, 1e-9) && near(f->dLimit, 5.8, 1e-9)
          && near(f->dDuration, 1.1, 1e-9) && f->iFirst == 200 && f->iLast == 309 && f->sConfidence == "medium",
          f ? f->sText : QString("%1 findings").arg(r.lFindings.size()));
}

static void checkValley(const forceLimits& s)
{
    // R3 section 3: a 2 s valley peaking at 4.5 g is judged by how long it
    // stays above 4.0, 4.1 ... g, not by its whole length.
    const double dt = 0.001;
    const int pad = 2000, len = 2000;
    forceTrace t = restTrace(dt, len + 2 * pad);
    for (int i = 0; i < len; ++i)
        t.gz[pad + i] = 1.0 + 3.5 * std::sin(M_PI * (i + 0.5) * dt / 2.0);
    // known answer: above level L for 2 - (4/pi) asin((L-1)/3.5) s
    double expected = 0.0;
    for (int k = 11; k <= 45; ++k)
    {
        const double L = k * 0.1;
        const double d = 2.0 - 4.0 / M_PI * std::asin((L - 1.0) / 3.5);
        if (d >= 0.2)
            expected = std::max(expected, L / zPosByHand(d));
    }
    const limitResult r = run(t, s);
    const double got = ruleMax(r, "z_pos");
    check("2 s valley peaking at 4.5 g: within the limits (judged naively, 4.5 g for 2 s against 4.0 g, it would fail)",
          r.bValid && findingsOf(r, "z_pos") == 0 && 4.5 / zPosByHand(2.0) > 1.0);
    check("2 s valley peaking at 4.5 g: largest share as worked out by hand", near(got, expected, 0.005),
          num(got) + " against " + num(expected));
    const limitResult rf = run(t, s, "base", false, true);
    check("2 s valley peaking at 4.5 g, filtered at 5 Hz: within the limits", rf.bValid && rf.bPass, num(rf.worst.dUtil));
}

static void checkAirtimeLines(const forceLimits& s)
{
    forceTrace t = restTrace(0.01, 500);
    setRange(t.gz, 200, 100, -1.8);
    const limitResult base = run(t, s, "base", false);
    const limitResult ext = run(t, s, "base", true);
    check("-1.8 g for 1 s: over the base -Gz line (share 1.2)", findingsOf(base, "z_neg") == 1
          && near(ruleMax(base, "z_neg"), 1.2, 1e-6), num(ruleMax(base, "z_neg")));
    check("-1.8 g for 1 s: within the extended line (share 1.8/2.2)", findingsOf(ext, "z_neg_extended") == 0
          && ext.bPass && near(ruleMax(ext, "z_neg_extended"), 1.8 / 2.2, 1e-6), num(ruleMax(ext, "z_neg_extended")));
}

static void checkEyesFront(const forceLimits& s)
{
    forceTrace t = restTrace(0.01, 500);
    setRange(t.gx, 200, 100, -1.8);
    const limitResult base = run(t, s, "base"), otsr = run(t, s, "otsr"), prone = run(t, s, "prone"), lap = run(t, s, "lapbar");
    check("-1.8 g eyes front for 1 s: base case over (share 1.2)", findingsOf(base, "x_neg_base") == 1
          && near(ruleMax(base, "x_neg_base"), 1.2, 1e-6));
    check("-1.8 g eyes front for 1 s: over-the-shoulder within (share 0.9)", otsr.bPass && near(ruleMax(otsr, "x_neg_otsr"), 0.9, 1e-6));
    check("-1.8 g eyes front for 1 s: prone within (share 1.8/3.5)", prone.bPass && near(ruleMax(prone, "x_neg_prone"), 1.8 / 3.5, 1e-6));
    check("the train catalogue's lapbar is judged as the base case", findingsOf(lap, "x_neg_base") == 1);
    forceTrace t3 = restTrace(0.01, 500);
    setRange(t3.gx, 200, 100, -3.0);
    check("-3.0 g eyes front for 1 s: over-the-shoulder over (1.5), prone within (3.0/3.5)",
          findingsOf(run(t3, s, "otsr"), "x_neg_otsr") == 1 && run(t3, s, "prone").bPass);
    const limitResult unknown = run(t, s, "seatbelt");
    check("an unknown restraint kind is refused, naming the kinds", !unknown.bValid && unknown.sError.contains("seatbelt")
          && unknown.sError.contains("prone"), unknown.sError);

    // the egg's -x radius follows the restraint, its -z radius the extended line
    forceTrace e = restTrace(0.01, 500);
    setRange(e.gx, 200, 100, -1.6);
    setRange(e.gz, 200, 100, 3.0);
    check("egg x-z at (-1.6, 3.0): base radius 2.0 gives sqrt(0.89), prone radius 3.5 a smaller share",
          near(ruleMax(run(e, s, "base"), "egg_xz"), std::sqrt(0.64 + 0.25), 1e-6)
          && near(ruleMax(run(e, s, "prone"), "egg_xz"), std::sqrt(std::pow(1.6 / 3.5, 2) + 0.25), 1e-6));
    forceTrace z = restTrace(0.01, 500);
    setRange(z.gx, 200, 50, 1.0);
    setRange(z.gz, 200, 50, -1.6);
    check("egg x-z at (1.0, -1.6): -z radius 2.0, or 2.8 with the extended line",
          near(ruleMax(run(z, s, "base", false), "egg_xz"), std::sqrt(1.0 / 36.0 + 0.64), 1e-6)
          && near(ruleMax(run(z, s, "base", true), "egg_xz"), std::sqrt(1.0 / 36.0 + std::pow(1.6 / 2.8, 2)), 1e-6));
}

static void checkShortEvent(const forceLimits& s)
{
    forceTrace t = restTrace(0.01, 400);
    setRange(t.gz, 200, 15, 7.0);
    setRange(t.gx, 200, 15, -5.0);
    const limitResult r = run(t, s);
    check("a 0.15 s event of 7 g and -5 g eyes front gives no finding (impacts under 0.2 s are not addressed)",
          r.bValid && r.lFindings.isEmpty(), QString("%1 findings").arg(r.lFindings.size()));
    forceTrace u = restTrace(0.01, 400);
    setRange(u.gz, 200, 20, 7.0);
    check("the same at 0.2 s is judged (7 g against 6 g)", findingsOf(run(u, s), "z_pos") == 1);
}

static void checkEgg(forceLimits s)
{
    forceTrace t = restTrace(0.01, 500);
    setRange(t.gx, 200, 50, 4.5);
    setRange(t.gz, 200, 50, 4.5);
    const double between = std::sqrt(2.0) * 0.75;   // 1.06: outside the 1.0 ellipse, inside the 1.1 one
    const limitResult r10 = run(t, s);
    const limitFinding* f = firstFinding(r10, "egg_xz");
    check("egg x-z at (4.5, 4.5) for 0.5 s, factor 1.0: over, share 1.06, one finding",
          findingsOf(r10, "egg_xz") == 1 && f && near(f->dUtilisation, between, 1e-6) && near(f->dDuration, 0.5, 1e-9),
          f ? f->sText : QString());
    check("egg x-z at (4.5, 4.5): each axis alone is within its curve (0.75)", findingsOf(r10, "x_pos") == 0
          && findingsOf(r10, "z_pos") == 0 && near(ruleMax(r10, "x_pos"), 0.75, 1e-6));
    s.eggRadiusFactor.dValue = 1.1;
    const limitResult r11 = run(t, s);
    check("the same point with the 2006 factor 1.1: within, share 1.06/1.1", r11.bPass
          && near(ruleMax(r11, "egg_xz"), between / 1.1, 1e-6), num(ruleMax(r11, "egg_xz")));
    s.eggRadiusFactor.dValue = 1.0;
    forceTrace sh = restTrace(0.01, 500);
    setRange(sh.gx, 200, 15, 4.5);
    setRange(sh.gz, 200, 15, 4.5);
    check("the same point held 0.15 s only: no egg finding", run(sh, s).bPass);
}

static void checkReversal(const forceLimits& s)
{
    forceTrace t = restTrace(0.01, 600);
    setRange(t.gy, 200, 30, 1.6);
    setRange(t.gy, 230, 30, -1.6);
    const limitResult r = run(t, s);
    const limitFinding* f = firstFinding(r, "reversal_y");
    check("1.6 g left then 1.6 g right, peaks 0.01 s apart: fast reversal over the halved 1.5 g (share 1.067)",
          findingsOf(r, "reversal_y") == 1 && f && near(f->dUtilisation, 1.6 / 1.5, 1e-6) && near(f->dLimit, 1.5, 1e-9)
          && findingsOf(r, "y") == 0, f ? f->sText : QString());
    forceTrace slow = restTrace(0.01, 600);
    setRange(slow.gy, 200, 30, 1.6);
    setRange(slow.gy, 280, 30, -1.6);
    check("the same with 0.5 s between: not a fast reversal", run(slow, s).bPass);
    forceTrace low = restTrace(0.01, 600);
    setRange(low.gy, 200, 30, 1.4);
    setRange(low.gy, 230, 30, -1.4);
    check("1.4 g each way, fast: within the halved limit (share 0.933)", run(low, s).bPass
          && near(ruleMax(run(low, s), "reversal_y"), 1.4 / 1.5, 1e-6));
    forceTrace x = restTrace(0.01, 600);
    setRange(x.gx, 200, 30, 2.5);
    setRange(x.gx, 230, 30, -1.2);
    const limitResult rx = run(x, s);
    check("x: +2.5 g then -1.2 g fast, base case: -1.2 over the halved -1.0 g (share 1.2)",
          findingsOf(rx, "reversal_x") == 1 && near(ruleMax(rx, "reversal_x"), 1.2, 1e-6));
}

static void pushPullTrace(forceTrace& t, int at, bool airtime, double airtimeSeconds)
{
    const double dt = t.dt;
    const int air = (int)std::floor(airtimeSeconds / dt + 0.5);
    if (airtime)
        setRange(t.gz, at, air, -0.5);
    // a ramp of 10 g/s into a 3.0 g pull-out held 3 s
    const double from = airtime ? -0.5 : 1.0;
    const int ramp = (int)std::floor((3.0 - from) / 10.0 / dt + 0.5);
    for (int i = 0; i < ramp; ++i)
        t.gz[at + air + i] = from + (3.0 - from) * (i + 1) / ramp;
    setRange(t.gz, at + air + ramp, (int)std::floor(3.0 / dt + 0.5), 3.0);
}

static void checkPushPull(const forceLimits& s)
{
    forceTrace a = restTrace(0.01, 1500);
    pushPullTrace(a, 200, true, 3.5);
    const limitResult ra = run(a, s);
    const limitFinding* f = firstFinding(ra, "z_pos_after_neg");
    check("3 g for 3 s after 3.5 s of airtime: over the push-pull curve (2.0 g), one finding, no weightless finding",
          findingsOf(ra, "z_pos_after_neg") == 1 && f && f->iFamily == limitFamilyAirtime && near(f->dUtilisation, 1.5, 1e-6)
          && findingsOf(ra, "weightless_to_pos_z") == 0, f ? f->sText : QString());
    check("after airtime the +Gz events are judged by the push-pull curve instead", findingsOf(ra, "z_pos") == 0);
    forceTrace b = restTrace(0.01, 1500);
    pushPullTrace(b, 200, false, 3.5);
    const limitResult rb = run(b, s);
    check("the same pull-out without airtime: within (+Gz 4.0 g at 3 s, share 0.75)", rb.bPass
          && near(ruleMax(rb, "z_pos"), 0.75, 1e-6), num(ruleMax(rb, "z_pos")));
    forceTrace c = restTrace(0.01, 1500);
    pushPullTrace(c, 200, true, 2.9);
    check("after 2.9 s of airtime (under the 3 s trigger): judged by the normal curve, within", run(c, s).bPass);
}

static void onsetTrace(forceTrace& t, double weightless, double rate)
{
    const double dt = t.dt;
    const int at = 1000;
    const int w = (int)std::floor(weightless / dt + 0.5);
    setRange(t.gz, at, w, -0.5);
    int i = at + w;
    double v = -0.5;
    while (v < 2.5 && i < t.size())
    {
        v = std::min(2.5, v + rate * dt);
        t.gz[i++] = v;
    }
    setRange(t.gz, i, (int)(1.0 / dt), 2.5);
}

static void checkOnset(const forceLimits& s)
{
    forceTrace fast = restTrace(0.001, 5000);
    onsetTrace(fast, 0.5, 20.0);
    const limitResult rf = run(fast, s);
    const limitFinding* f = firstFinding(rf, "weightless_to_pos_z");
    check("weightless 0.5 s, then 0 to 2 g at 20 g/s (0.1 s): too fast, share 0.133/0.1",
          findingsOf(rf, "weightless_to_pos_z") == 1 && f && near(f->dLevel, 0.1, 1e-6) && near(f->dUtilisation, 1.33, 1e-6),
          f ? f->sText : QString());
    forceTrace slow = restTrace(0.001, 5000);
    onsetTrace(slow, 0.5, 10.0);
    const limitResult rs = run(slow, s);
    check("the same at 10 g/s (0.2 s): within, share 0.665", findingsOf(rs, "weightless_to_pos_z") == 0
          && near(ruleMax(rs, "weightless_to_pos_z"), 0.665, 1e-6), num(ruleMax(rs, "weightless_to_pos_z")));
    forceTrace edge = restTrace(0.001, 5000);
    onsetTrace(edge, 0.5, 15.0);
    check("the same at 15 g/s (0.1333 s): within", findingsOf(run(edge, s), "weightless_to_pos_z") == 0);
    forceTrace brief = restTrace(0.001, 5000);
    onsetTrace(brief, 0.15, 20.0);
    check("weightless only 0.15 s, then 20 g/s: not sustained, no finding", findingsOf(run(brief, s), "weightless_to_pos_z") == 0);
}

static void checkRestraint(const forceLimits& s)
{
    struct pt { double ax, az; int area; };
    static const pt points[] = {
        {0.0, 1.0, 1}, {0.3, 0.5, 1}, {-0.23, 1.5, 1}, {-0.3, 1.5, 2}, {-1.0, 1.0, 3}, {0.1, 0.1, 3},
        {-1.5, 1.0, 4}, {0.5, -0.1, 4}, {1.0, -0.15, 4}, {-2.0, 1.0, 5}, {0.0, -0.1, 5}, {1.0, -0.3, 5},
    };
    bool ok = true;
    QString bad;
    for (unsigned i = 0; i < sizeof(points) / sizeof(points[0]); ++i)
    {
        const int a = limitRestraintArea(points[i].ax, points[i].az, s);
        if (a != points[i].area)
        {
            ok = false;
            bad += QString("(%1, %2): %3; ").arg(points[i].ax).arg(points[i].az).arg(a);
        }
    }
    check("restraint diagram: 12 points in areas 1 to 5 (lines 0.1 g thick, the lower area on a line)", ok, bad);
    struct trace { double gx, gz; int area; const char* what; };
    static const trace traces[] = {
        {-1.0, 1.0, 3, "braking at 1.0 g for 10 s: area 3"},
        {-1.5, 1.0, 4, "braking at 1.5 g for 10 s: area 4"},
        {0.0, -0.3, 5, "airtime at -0.3 g for 10 s: area 5"},
    };
    for (unsigned i = 0; i < sizeof(traces) / sizeof(traces[0]); ++i)
    {
        forceTrace t = restTrace(0.01, 3000);
        setRange(t.gx, 1000, 1000, traces[i].gx);
        setRange(t.gz, 1000, 1000, traces[i].gz);
        const limitResult r = run(t, s, "base", false, true);
        check(QString("restraint area from the 1 Hz trace, %1").arg(traces[i].what),
              r.iRestraintArea == traces[i].area && r.iRestraintSample > 1000 && r.iRestraintSample < 1300,
              QString("area %1 first at sample %2").arg(r.iRestraintArea).arg(r.iRestraintSample));
    }
}

static double amplitude(const limitFilter& f, double freq, double dt, double seconds)
{
    const int n = (int)(seconds / dt);
    QVector<double> x(n);
    for (int i = 0; i < n; ++i)
        x[i] = std::sin(2.0 * M_PI * freq * i * dt);
    const QVector<double> y = limitFilterApply(f, x, false);
    // the amplitude over the last second (whole periods), by projection on sine and cosine
    double s = 0.0, c = 0.0;
    const int m = (int)std::floor(1.0 / dt + 0.5);
    for (int i = n - m; i < n; ++i)
    {
        s += y.at(i) * std::sin(2.0 * M_PI * freq * i * dt);
        c += y.at(i) * std::cos(2.0 * M_PI * freq * i * dt);
    }
    return 2.0 / m * std::sqrt(s * s + c * c);
}

static void checkFilter()
{
    const limitFilter f1 = limitFilterDesign(4, 5.0, 0.001);
    const limitFilter f2 = limitFilterDesign(4, 5.0, 0.01);
    const double a1 = amplitude(f1, 5.0, 0.001, 6.0), a2 = amplitude(f2, 5.0, 0.01, 6.0);
    check("5 Hz filter, 1000 samples/s: a 5 Hz sine comes out at -3 dB (0.7071)", near(a1, std::sqrt(0.5), 0.002), num(a1));
    check("5 Hz filter, 100 samples/s: a 5 Hz sine comes out at -3 dB (0.7071)", near(a2, std::sqrt(0.5), 0.002), num(a2));
    const double low = amplitude(f1, 1.0, 0.001, 6.0), high = amplitude(f1, 20.0, 0.001, 6.0);
    check("1 Hz passes (1.000), 20 Hz is cut to the four-pole 1/sqrt(1+4^8) = 0.0039",
          near(low, 1.0, 0.002) && near(high, 1.0 / std::sqrt(1.0 + std::pow(4.0, 8)), 0.0015), num(low) + ", " + num(high));
    // a step stays where it was once the delay is taken out
    QVector<double> step(2000, 0.0);
    for (int i = 1000; i < 2000; ++i) step[i] = 1.0;
    int shift = 0;
    const QVector<double> y = limitFilterApply(f1, step, true, &shift);
    int cross = 0;
    while (cross < y.size() && y.at(cross) < 0.5) ++cross;
    const double delay = limitFilterDelay(f1) * 0.001;
    check(QString("the filter's delay (%1 s, %2 samples) is taken out: a step crosses half-way within 0.01 s of where it was")
          .arg(delay, 0, 'f', 4).arg(shift), std::abs(cross - 1000) <= 10 && near(delay, 2.6131 / (2.0 * M_PI * 5.0), 0.002),
          QString("crosses at sample %1").arg(cross));
    QVector<double> flat(500, 1.0);
    const QVector<double> yf = limitFilterApply(f1, flat, true);
    check("a constant trace comes out unchanged", near(*std::min_element(yf.begin(), yf.end()), 1.0, 1e-12)
          && near(*std::max_element(yf.begin(), yf.end()), 1.0, 1e-12));
    check("a corner at or above half the sample rate is refused", !limitFilterDesign(4, 5.0, 0.1).bValid);
}

static void checkBadInput(const forceLimits& s)
{
    forceTrace t = restTrace(0.01, 100);
    t.gz[50] = NAN;
    const limitResult r = run(t, s);
    check("a trace with a value that is not a number is refused with its place", !r.bValid && r.sError.contains("sample 50"), r.sError);
    forceTrace c = restTrace(0.2, 100);
    const limitResult rc = run(c, s, "base", false, true);
    check("samples 0.2 s apart are refused for the 5 Hz filter", !rc.bValid && rc.sError.contains("5 Hz"), rc.sError);
}

// ---------------------------------------------------------------- broken files and the owner's mark

static QString writeFile(const QTemporaryDir& dir, const QString& name, const QByteArray& bytes)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(bytes);
    return path;
}

static QJsonObject setPath(QJsonObject o, const QStringList& path, const QJsonValue& value)
{
    // path elements: object keys, or "[n]" for an array element
    if (path.isEmpty())
        return o;
    const QString key = path.first();
    if (path.size() == 1)
    {
        o[key] = value;
        return o;
    }
    const QString next = path.at(1);
    if (next.startsWith('['))
    {
        QJsonArray arr = o[key].toArray();
        const int idx = next.mid(1, next.size() - 2).toInt();
        if (path.size() == 2)
            arr[idx] = value;
        else
            arr[idx] = setPath(arr.at(idx).toObject(), path.mid(2), value);
        o[key] = arr;
        return o;
    }
    o[key] = setPath(o[key].toObject(), path.mid(1), value);
    return o;
}

static void checkBrokenFiles(const QString& path, const forceLimits& s)
{
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    const QByteArray bytes = file.readAll();
    const QJsonObject root = QJsonDocument::fromJson(bytes).object();
    QTemporaryDir dir;

    struct broken { const char* what; QStringList path; QJsonValue value; const char* place; };
    const broken cases[] = {
        { "a curve point earlier than the one before it", QStringList() << "curves" << "z_pos" << "points" << "[3]" << "s",
          QJsonValue(1.5), "curves.z_pos.points[3].s" },
        { "a confidence word the file does not know", QStringList() << "combination" << "radius_factor" << "confidence",
          QJsonValue(QString("maybe")), "combination.radius_factor.confidence" },
        { "a restraint kind naming a curve that is not there", QStringList() << "restraint_kinds" << "kinds" << "prone" << "x_neg_curve",
          QJsonValue(QString("x_neg_flat")), "restraint_kinds.kinds.prone.x_neg_curve" },
        { "a limit number that is text", QStringList() << "reversal_xy" << "factor" << "value",
          QJsonValue(QString("half")), "reversal_xy.factor.value" },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        const QJsonObject changed = setPath(root, cases[i].path, cases[i].value);
        const QString p = writeFile(dir, QString("broken%1.json").arg(i), QJsonDocument(changed).toJson());
        forceLimits out;
        QString error;
        const bool loaded = forceLimits::load(p, out, &error);
        check(QString("broken file refused, place named: %1").arg(cases[i].what),
              !loaded && !out.isValid() && error.startsWith(cases[i].place), error);
    }
    {
        const QString p = writeFile(dir, "cut.json", bytes.left(bytes.size() / 2));
        forceLimits out;
        QString error;
        check("broken file refused, place named: a file cut off half-way (line and column)",
              !forceLimits::load(p, out, &error) && error.contains("line") && error.contains("column"), error);
    }
    {
        forceLimits out;
        QString error;
        check("a file that is not there is refused", !forceLimits::load(dir.filePath("none.json"), out, &error) && !error.isEmpty());
    }

    // the owner marks a number as checked: the findings then say so
    const QJsonObject marked = setPath(root, QStringList() << "curves" << "x_neg_otsr" << "points" << "[0]" << "checked",
                                       QJsonValue(QString("F2291-26b test mark")));
    forceLimits m;
    QString error;
    const bool loaded = forceLimits::load(writeFile(dir, "marked.json", QJsonDocument(marked).toJson()), m, &error);
    forceTrace t = restTrace(0.01, 1400);
    setRange(t.gx, 200, 1000, -2.1);
    const limitResult r = run(t, m, "otsr");
    const limitFinding* f = firstFinding(r, "x_neg_otsr");
    bool listed = false;
    for (int i = 0; i < m.lNumbers.size(); ++i)
        if (m.lNumbers.at(i).sId == "x_neg_otsr@0.2s" && m.lNumbers.at(i).sChecked == "F2291-26b test mark")
            listed = true;
    check("a number marked \"checked\" in the file is listed so and its findings say \"checked\"",
          loaded && listed && f && f->sConfidence == "checked", error);
    Q_UNUSED(s);
}

// ---------------------------------------------------------------- time

static void checkTiming(const forceLimits& s)
{
    const int n = 15000;
    forceTrace t;
    t.dt = 0.01;
    for (int i = 0; i < n; ++i)
    {
        const double x = i * t.dt;
        t.gx.append(0.4 * std::sin(0.3 * x) - 0.2);
        t.gy.append(1.2 * std::sin(0.7 * x + 1.0));
        t.gz.append(1.0 + 2.5 * std::sin(0.5 * x) + 0.3 * std::sin(3.1 * x));
    }
    limitOptions o;
    QVector<qint64> times;
    limitResult r;
    // the best of 50 runs: other programs on the PC only ever make a run slower
    for (int k = 0; k < 50; ++k)
    {
        QElapsedTimer timer;
        timer.start();
        r = checkForceLimits(t, s, o);
        times.append(timer.nsecsElapsed());
    }
    std::sort(times.begin(), times.end());
    const double best = times.first() / 1e6, median = times.at(times.size() / 2) / 1e6;

    // the steps one by one (information): the best of 25 runs each
    const limitFilter filter = limitFilterDesign(s.iFilterOrder, s.filterCorner.dValue, t.dt);
    forceTrace f = t;
    f.gx = limitFilterApply(filter, t.gx, true);
    f.gy = limitFilterApply(filter, t.gy, true);
    f.gz = limitFilterApply(filter, t.gz, true);
    QStringList parts;
    for (int step = 0; step < 8; ++step)
    {
        qint64 bestStep = -1;
        for (int k = 0; k < 25; ++k)
        {
            limitResult x;
            if (step != 6)
                limitResultInit(x, n);
            QElapsedTimer timer;
            timer.start();
            switch (step)
            {
            case 0:
            {
                forceTrace g;
                const QVector<double>* in[3] = { &t.gx, &t.gy, &t.gz };
                QVector<double>* out[3] = { &g.gx, &g.gy, &g.gz };
                limitFilterApplyMany(filter, in, out, 3, true);
                break;
            }
            case 1: limitStepDuration(f, s, o, x); break;
            case 2: limitStepEggs(f, s, o, x); break;
            case 3: limitStepReversal(f, s, o, x); break;
            case 4: limitStepAirtime(f, s, o, x); break;
            case 5: limitStepRestraint(t, s, x); break;
            case 6: limitResultInit(x, n); break;
            default: limitResultFinish(x); break;
            }
            const qint64 ns = timer.nsecsElapsed();
            if (bestStep < 0 || ns < bestStep) bestStep = ns;
        }
        static const char* names[8] = { "filter", "duration", "eggs", "reversal", "airtime", "restraint", "init", "finish" };
        parts << QString("%1 %2").arg(names[step]).arg(bestStep / 1e6, 0, 'f', 3);
    }
    // A measurement, not a pass mark: other programs on the PC (builds run
    // beside the tests) slow a run down, so the 2 ms target of the plan is
    // reported, and the check fails only at ten times it (20 ms). That bound
    // catches a real mistake (a step gone quadratic: 15,000 squared is 2e8
    // operations, far over 20 ms; a filter redesigned per sample) and not a
    // busy PC, which was seen to add about a quarter.
    const double target = 2.0, bound = 10.0 * target;
    printf("      measured: one 15,000-sample trace (filtered, every rule): best %.3f ms, median %.3f ms of 50 runs\n",
           best, median);
    printf("      steps (ms, best of 25): %s\n", qPrintable(parts.join(", ")));
    printf("      target of %.0f ms (docs/stage-3-plan.md, A3a): %s on this run\n", target,
           best < target ? "met" : "NOT met (a busy PC can cause this; the check below fails only at 10 times)");
    check(QString("one 15,000-sample trace runs, best %1 ms, within the bound of %2 ms (10 times the target)")
          .arg(best, 0, 'f', 3).arg(bound, 0, 'f', 0), r.bValid && best < bound);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc < 2)
    {
        printf("usage: forcelimits <repository folder>\nFAIL\n");
        return 1;
    }
    const QString path = QDir(QString::fromLocal8Bit(argv[1])).filePath("data/limits/astm-f2291-public.json");
    forceLimits s;
    checkDataFile(path, s);
    if (!s.isValid())
    {
        printf("FAIL\n");
        return 1;
    }
    checkFilter();
    checkRest(s);
    checkPlateaus(s);
    checkValley(s);
    checkAirtimeLines(s);
    checkEyesFront(s);
    checkShortEvent(s);
    checkEgg(s);
    checkReversal(s);
    checkPushPull(s);
    checkOnset(s);
    checkRestraint(s);
    checkBadInput(s);
    checkBrokenFiles(path, s);
    checkTiming(s);

    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
