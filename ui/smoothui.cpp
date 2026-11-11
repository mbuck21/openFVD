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

#include "smoothui.h"
#include "ui_smoothui.h"
#include "smoothhandler.h"
#include "mainwindow.h"
#include "trackwidget.h"
#include "lenassert.h"

smoothUi::smoothUi(trackHandler* _track, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::smoothUi)
{
    setWindowFlags(windowFlags() | Qt::CustomizeWindowHint);
    setWindowFlags(windowFlags() & (~Qt::WindowMinimizeButtonHint));
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

#ifdef Q_OS_MAC // Damit es im Vordergrund bleibt, evtl. für windows auch sinnvoll, probiere es einfach mal unter windows aus
    setWindowFlags(windowFlags() | Qt::Popup);
#endif

    ui->setupUi(this);

    ui->smoothUnitTree->setItemDelegateForColumn(0, new NoEditDelegate(this));
    ui->smoothUnitTree->setItemDelegateForColumn(2, new NoEditDelegate(this));
    ui->smoothUnitTree->setItemDelegateForColumn(3, new NoEditDelegate(this));
    ui->smoothUnitTree->setItemDelegateForColumn(4, new NoEditDelegate(this));
    ui->smoothUnitTree->setItemDelegateForColumn(5, new NoEditDelegate(this));
    ui->smoothUnitTree->setItemDelegateForColumn(6, new NoEditDelegate(this));


    m_trackwidget = _track->trackWidgetItem;
    m_track = _track->trackData;

    ui->smoothUnitTree->setColumnWidth(0, 28);
    ui->smoothUnitTree->setColumnWidth(1, 120);
    ui->smoothUnitTree->setColumnWidth(2, 60);
    ui->smoothUnitTree->setColumnWidth(3, 60);
    ui->smoothUnitTree->setColumnWidth(4, 60);
    ui->smoothUnitTree->setColumnWidth(5, 60);
    ui->smoothUnitTree->setColumnWidth(6, 60);

    ui->optsFrame->hide();
    ui->regionFrame->hide();
    ui->removeButton->setEnabled(false);

    ui->upButton->hide();
    ui->downButton->hide();

    curHandler = NULL;

    ui->warningLabel->setText("");
    ui->warningLabel->hide();

    customChar = 'a';

    phantomChanges = false;
}

smoothUi::~smoothUi()
{
    QMap<smoothHandler*, QTreeWidgetItem*>::iterator it;
    for(it = rows.begin(); it != rows.end(); ++it) {
        it.key()->mListener = NULL;
        if(!it.value()->treeWidget()) delete it.value();
    }
    delete ui;
}

void smoothUi::on_buttonBox_accepted()
{
    m_track->applyRollSmooth();
}

void smoothUi::updateUi()
{
    m_track->updateSmoothEntries(&customChar);
    showRows();
}

// Makes the list rows of the entries that have none yet and puts every row in
// the list, in the entries' order. Changes nothing in the track.
void smoothUi::showRows()
{
    for(int i = 0; i < m_track->smoothList.size(); ++i) {
        smoothHandler* cur = m_track->smoothList[i];
        QTreeWidgetItem* row = rowFor(cur);
        if(!row) {
            row = new QTreeWidgetItem();
            if(cur->sec == NULL) row->setFlags(row->flags() | Qt::ItemIsEditable);
            showFields(cur, row, smoothHandler::fieldLabel | smoothHandler::fieldName | smoothHandler::fieldFrom | smoothHandler::fieldTo
                       | smoothHandler::fieldLength | smoothHandler::fieldIterations | smoothHandler::fieldActive);
            rows.insert(cur, row);
            cur->mListener = this;
        }
        if(!row->parent()) {
            ui->smoothUnitTree->insertTopLevelItem(i, row);
        }
    }
}

QTreeWidgetItem* smoothUi::rowFor(smoothHandler* _handler)
{
    return rows.value(_handler, NULL);
}

// Writes the entry's values into its row, for the columns in fields.
void smoothUi::showFields(smoothHandler* _handler, QTreeWidgetItem* row, int fields)
{
    if(fields & smoothHandler::fieldLabel) row->setText(0, _handler->sLabel);
    if(fields & smoothHandler::fieldName) row->setText(1, _handler->sName);
    if(fields & smoothHandler::fieldFrom) row->setText(2, QString::number(_handler->getFrom()/1000.).append("s"));
    if(fields & smoothHandler::fieldTo) row->setText(3, QString::number(_handler->getTo()/1000.).append("s"));
    if(fields & smoothHandler::fieldLength) row->setText(4, QString::number(_handler->getLength()/1000.).append("s"));
    if(fields & smoothHandler::fieldIterations) row->setText(5, QString::number(_handler->getIterations()));
    if(fields & smoothHandler::fieldActive) row->setCheckState(6, _handler->active ? Qt::Checked : Qt::Unchecked);
}

void smoothUi::smoothHandlerChanged(smoothHandler* which, int fields)
{
    QTreeWidgetItem* row = rowFor(which);
    if(row) showFields(which, row, fields);
}

void smoothUi::smoothHandlerDeleted(smoothHandler* which)
{
    delete rowFor(which);
    rows.remove(which);
}

void smoothUi::on_smoothUnitTree_itemSelectionChanged()
{
    bool oldP = phantomChanges;
    phantomChanges = true;

    int i = m_track->smoothList.size();
    if(ui->smoothUnitTree->selectedItems().size() != 0) {
        QTreeWidgetItem* selected = ui->smoothUnitTree->selectedItems().at(0);
        for(i = 0; i < m_track->smoothList.size(); ++i) {
            if(rowFor(m_track->smoothList[i]) == selected) break;
        }
    }
    if(i >= m_track->smoothList.size()) {   // nothing selected, or a row whose entry is gone
        curHandler = NULL;
        ui->optsFrame->hide();
        ui->regionFrame->hide();
        ui->removeButton->setEnabled(false);
        phantomChanges = oldP;
        return;
    }
    curHandler = m_track->smoothList[i];
    if(m_track->smoothList[i]->sec != NULL) {
        ui->lengthBox->setValue(curHandler->getLength()/1000.);
        ui->iterBox->setValue(curHandler->getIterations());
        ui->optsFrame->show();
        ui->regionFrame->hide();
        ui->removeButton->setEnabled(false);
    } else {
        ui->lengthBox->setValue(curHandler->getLength()/1000.);
        ui->iterBox->setValue(curHandler->getIterations());
        ui->fromBox->setValue(curHandler->getFrom()/1000.);
        ui->toBox->setValue(curHandler->getTo()/1000.);
        ui->optsFrame->show();
        ui->regionFrame->show();
        ui->removeButton->setEnabled(true);
    }

    phantomChanges = oldP;
}

void smoothUi::on_lengthBox_valueChanged(double arg1)
{
    if(phantomChanges || curHandler == NULL) return;
    phantomChanges = true;
    curHandler->setLength((int)(arg1*1000.+0.5));
    generateWarnings();
    phantomChanges = false;
}

void smoothUi::on_iterBox_valueChanged(int arg1)
{
    if(phantomChanges || curHandler == NULL) return;
    phantomChanges = true;
    curHandler->setIterations(arg1);
    generateWarnings();
    phantomChanges = false;
}

void smoothUi::on_fromBox_valueChanged(double arg1)
{
    if(phantomChanges || curHandler == NULL) return;
    const int max = m_track->getNumPoints();
    const int min = 0;
    int setTo = (int)(arg1*1000.+0.5);
    if(setTo > max) {
        ui->fromBox->setValue(max/1000.);
        return;
    }
    if(setTo < min) {
        ui->fromBox->setValue(min/1000.);
        return;
    }

    phantomChanges = true;
    curHandler->setFrom((int)(arg1*1000.+0.5));
    generateWarnings();
    phantomChanges = false;
}

void smoothUi::on_toBox_valueChanged(double arg1)
{
    if(phantomChanges || curHandler == NULL) return;
    int setTo = (int)(arg1*1000.+0.5);
    const int max = m_track->getNumPoints();
    const int min = 0;
    if(setTo > max) {
        ui->toBox->setValue(max/1000.);
        return;
    }
    if(setTo < min) {
        ui->toBox->setValue(min/1000.);
        return;
    }

    phantomChanges = true;
    curHandler->setTo(setTo);
    generateWarnings();
    phantomChanges = false;
}

void smoothUi::on_smoothUnitTree_itemChanged(QTreeWidgetItem *item, int column)
{
    if(column == 1) {
        // The entry keeps the name the row shows (it is saved with the project),
        // so a name typed into the list is stored at once, as the row did.
        for(int i = 0; i < m_track->smoothList.size(); ++i) {
            if(rowFor(m_track->smoothList[i]) == item) {
                m_track->smoothList[i]->sName = item->text(1);
                break;
            }
        }
    }

    if(phantomChanges) return;
    phantomChanges = true;

    if(column == 6) {
        int i;
        for(i = 0; i < m_track->smoothList.size(); ++i) {
            if(rowFor(m_track->smoothList[i]) == item) break;
        }
        m_track->smoothList[i]->active = (item->checkState(6) == Qt::Checked);
    }

    generateWarnings();
    phantomChanges = false;
}

void smoothUi::on_newButton_released()
{
    m_track->smoothList.append(new smoothHandler(m_track, -2, &customChar, 400, 1, 0, 1000));
    updateUi();
}

void smoothUi::on_removeButton_released()
{
    int i;
    for(i = 0; i < m_track->smoothList.size(); ++i) {
        if(m_track->smoothList[i] == curHandler) break;
    }
    if(i >= m_track->smoothList.size()) return;
    // out of the list first: deleting the entry deletes its row, and the
    // selection that moves on must not find the entry in the list
    smoothHandler* entry = m_track->smoothList.takeAt(i);
    curHandler = NULL;
    delete entry;
}

// Before the track's model is replaced (trackHandler::trackAboutToReset()): no
// entry selected. The rows go as their entries are deleted
// (smoothHandlerDeleted()); showRows() makes the new ones.
void smoothUi::dropSelection()
{
    bool oldP = phantomChanges;
    phantomChanges = true;
    ui->smoothUnitTree->clearSelection();
    phantomChanges = oldP;
    curHandler = NULL;
    ui->optsFrame->hide();
    ui->regionFrame->hide();
    ui->removeButton->setEnabled(false);
}

void smoothUi::generateWarnings()
{
    QString str;
    for(int i = 0; i < m_track->smoothList.size(); ++i) {
        if(m_track->smoothList[i]->active) {
            smoothHandler* cur = m_track->smoothList[i];
            if(cur->getFrom() > cur->getTo()) {
                str.append(QString("Warning: Smoothing item \"").append(cur->sName).append("\" (").append(cur->sLabel).append(") ").append("is set to begin before it ends.\n"));
            } else if(2.f*cur->getLength() > cur->getTo() - cur->getFrom()) {
                str.append(QString("Warning: Smoothing item \"").append(cur->sName).append("\" (").append(cur->sLabel).append(") ").append("might be too short for its current filter length.\n"));
            }
        }
    }


    if(str.isEmpty()) {
        ui->warningLabel->hide();
    } else {
        ui->warningLabel->setText(str);
        ui->warningLabel->show();
    }
}
