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

#ifndef RIDESHEET_H
#define RIDESHEET_H

// The ride sheet panel (FR-18, docs/decisions/0012): the key numbers of one
// track in a narrow, read-only column beside the 3D view. It only formats what
// computeRideSummary() (core/ridesummary.h) returns; there is no ride maths here.
//
// The panel keeps the track* it was told about and nothing else: never a
// pointer into the track's sections or nodes, because an undo rebuilds them.
// Before it uses the pointer it checks that the track is still one of the
// project's open tracks.
//
// Refreshing. The track announces a change once per recompute, which during a
// drag is every frame (MainWindow::rideSheetChanged() hands the notice to
// trackChanged()). trackChanged() therefore only marks the panel out of date
// and (re)starts a 150 ms single-shot timer; the sheet is computed when the
// timer fires, and only if the panel is visible. setTrack() (tab or selection
// changed, units changed) takes the same path.

#include <QWidget>
#include <QTimer>
#include <QList>
#include <QStringList>

class QLabel;
class QGridLayout;
class track;

class rideSheet : public QWidget
{
    Q_OBJECT

public:
    explicit rideSheet(QWidget* parent = 0);

    // The track to show (NULL: dashes). Marks the panel out of date.
    void setTrack(track* t);
    // The model announced a change in track t; ignored unless t is shown.
    void trackChanged(track* t);
    // Computes and shows the sheet now, visible or not (the batch check uses this).
    void refreshNow();

    // Every label and value as text lines (for the batch check).
    QStringList textLines() const;

    QSize sizeHint() const { return QSize(230, 400); }

protected:
    void showEvent(QShowEvent* event);

private slots:
    void timerFired();

private:
    void addRow(const QString& key, const QString& labelText);
    void setValue(const QString& key, const QString& text, const QString& tip = QString());
    void setLabel(const QString& key, const QString& text);
    void showDashes();
    bool trackIsOpen() const;
    void markStale();

    struct row {
        QString key;
        QLabel* label;
        QLabel* value;
    };

    QList<row> lRows;
    QGridLayout* mGrid;
    QLabel* mName;
    QLabel* mFindingList;
    QLabel* mNonFinite;
    QLabel* mNote;
    QTimer mTimer;
    track* mTrack;
    bool bStale;
};

#endif // RIDESHEET_H
