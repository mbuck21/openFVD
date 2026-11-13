#ifndef PROJECTWIDGET_H
#define PROJECTWIDGET_H

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

#include <QWidget>
#include "projectfile.h"

class trackHandler;
class QTreeWidgetItem;
class trackThread;
class TrackProperties;

namespace Ui {
class projectWidget;
}

// The project: the list of trackHandlers, the ground texture path and the import
// entries. It is MainWindow's project tab and creates and deletes the trackHandlers.
// Implements projectFileClient: core/projectfile.* reads and writes the file and
// calls back here for everything that belongs to the windows.
class projectWidget : public QWidget, public projectFileClient
{
    Q_OBJECT
    
public:
    explicit projectWidget(QWidget *parent = 0);
    QString saveProject(const QString& fileName);
    QString loadProject(const QString& fileName);
    ~projectWidget();

    // A track was put back to another state (undo, redo): the Track
    // Properties dialog, if it is open for it, shows the values it has now.
    void trackReset(trackHandler* which);

    // projectFileClient: what reading and writing the project file asks of
    // the windows (core/projectfile.h)
    void projectCleared();
    bool projectGroundTexture(const QString& path);
    track* projectNewTrack();
    void projectTrackSmoothingUpdated(track* which);
    void projectTrackLoaded(track* which);
    void projectLoadFinished();
    void projectTrackSaving(track* which);
    QJsonObject projectAdditions() { return additions; }
    void projectAdditionsRead(const QJsonObject& read) { additions = read; }

    void init();
    void cleanUp();
    void appendTracks(QList<trackHandler*> &_list);
    void keyPressEvent(QKeyEvent* event);

    QList<trackHandler*> trackList;
    trackHandler* selTrack;
    QString texPath;
    QJsonObject additions;  // the project's own additions (appendix of the file), kept as read



public slots:
    void on_editButton_released();

    void on_trackListWidget_itemSelectionChanged();

    void on_trackListWidget_itemChanged(QTreeWidgetItem *item, int column);

    void on_addButton_released();

    void newEmptyTrack();

    void importFromProject(QString fileName = "");

    void importNLTrack();

    void importPointList();

    void importNoLimitsCSV();

    // the work of the three import entries above, after their file dialog
    void importNLTrackFile(QString fileName);
    void importPointListFile(QString fileName);
    void importNoLimitsCSVFile(QString fileName);

    void on_deleteButton_released();

    void on_texChooser_released();

private slots:
    void on_trackListWidget_customContextMenuRequested(const QPoint &pos);

    void on_trackListWidget_itemDoubleClicked(QTreeWidgetItem *item, int column);


    void on_propertyButton_released();

signals:
    void updateTracks();

private:
    bool phantomChanges;
    Ui::projectWidget *ui;
    int getTrack(QTreeWidgetItem *item);
    trackHandler* handlerFor(track* which);
    bool areYouSure();
    TrackProperties* properties;
};

#endif // PROJECTWIDGET_H
