#ifndef TRACKPROPERTIES_H
#define TRACKPROPERTIES_H

/*
#    FVD++, an advanced coaster design tool for NoLimits
#    Copyright (C) 2012-2015, Stephan "Lenny" Alt <alt.stephan@web.de>
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

#include <QDialog>
#include <QColor>
#include <QVariantMap>

class QColorDialog;
class trackHandler;

namespace Ui {
class TrackProperties;
}

// The Track Properties dialog. Window-modal. While it is open for a track, one
// step is open on that track's history ("Change track properties"): every
// change runs as a command inside it (core/commands_track.cpp). OK commits the
// step (nothing is recorded when nothing changed); Cancel, Escape and the
// window's close button roll it back, colours and drawing mode included. Every
// way out goes through done(); cancelFor() ends it when the track goes away.
class TrackProperties : public QDialog
{
    Q_OBJECT
    
public:
    explicit TrackProperties(QWidget *parent = 0);
    ~TrackProperties();

    void openForTrack(trackHandler* _curTrack);
    void trackReset(trackHandler* _track);
    // The track is about to be deleted or replaced: a step open on it is
    // rolled back and the dialog closed.
    void cancelFor(trackHandler* _track);
    bool stepOpen() const { return stepTrack != NULL; }

    trackHandler* curTrack;
    
    void done(int r);

public slots:
    void on_frictionBox_valueChanged(double arg1);

    void on_heartlineBox_valueChanged(double arg1);

    void on_trainTypeBox_currentIndexChanged(int index);

    void on_defaultColorButton_released();

    void ondefaultColor_received();

    void on_sectionColorButton_released();

    void onsectionColor_received();

    void on_transitionColorButton_released();

    void ontransitionColor_received();

    void on_drawBox_currentIndexChanged(int index);

    void on_styleBox_currentIndexChanged(int index);

    void on_wireframeBox_stateChanged(int arg1);

private:
    void fillBoxes();
    void fillTrainList();
    void applyBoxes();
    bool runCommand(const QString& name, const QVariantMap& args);

    trackHandler* stepTrack;    // the track whose step is open; NULL when none
    bool bFilling;              // fillBoxes() is setting the boxes
    // the values the boxes were filled with: OK changes only what differs
    double fFilledFriction;
    double fFilledResistance;
    double fFilledHeart;
    bool bSettingHeartline; // true while the train list is setting the heartline box
    QColorDialog* colorPicker;
    Ui::TrackProperties *ui;
};

#endif // TRACKPROPERTIES_H
