#ifndef SMOOTHUI_H
#define SMOOTHUI_H

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
#include <QMap>

#include "track.h"
#include "trackhandler.h"
#include "graphwidget.h"
#include "smoothhandler.h"

class trackWidget;
class QTreeWidgetItem;

namespace Ui {
class smoothUi;
}

class smoothUi : public QDialog, public smoothHandlerListener
{
    Q_OBJECT
    
public:
    explicit smoothUi(trackHandler* _track, QWidget *parent = 0);
    ~smoothUi();

    void updateUi();

    void smoothHandlerChanged(smoothHandler* which, int fields);
    void smoothHandlerDeleted(smoothHandler* which);

    char customChar;

public slots:
    void on_buttonBox_accepted();

private slots:
    void on_smoothUnitTree_itemSelectionChanged();

    void on_lengthBox_valueChanged(double arg1);

    void on_iterBox_valueChanged(int arg1);

    void on_fromBox_valueChanged(double arg1);

    void on_toBox_valueChanged(double arg1);

    void on_smoothUnitTree_itemChanged(QTreeWidgetItem *item, int column);

    void on_newButton_released();

    void on_removeButton_released();

private:
    QTreeWidgetItem* rowFor(smoothHandler* _handler);
    void showFields(smoothHandler* _handler, QTreeWidgetItem* row, int fields);

    void generateWarnings();

    Ui::smoothUi *ui;

    track* m_track;
    trackWidget* m_trackwidget;

    // One list row per smoothing entry, made when the list first shows the entry.
    QMap<smoothHandler*, QTreeWidgetItem*> rows;

    bool phantomChanges;
    smoothHandler* curHandler;
};

#endif // SMOOTHUI_H
