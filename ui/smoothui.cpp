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
#include "trackhistory.h"
#include "command.h"
#include "lenassert.h"

extern MainWindow* gloParent;

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
    m_handler = _track;
    bStepOpen = false;
    // blocks the main window while open: the step open on the track's history
    // must not meet another edit of the track
    setWindowModality(Qt::WindowModal);

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
    bShowingFields = false;
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

// Opens the dialog and its step. The entries are brought up to date from the
// track inside the step (updateUi()), so that Cancel takes that back too and
// opening and cancelling leaves the track as it was.
void smoothUi::openStep()
{
    if(bStepOpen) {
        raise();
        activateWindow();
        return;
    }
    m_handler->mHistory->beginStep("Change smoothing", commandTarget::forTrack(m_handler->getID()-1));
    bStepOpen = true;
    gloParent->setUndoButtons();   // Undo and Redo wait while the step is open
    updateUi();
    show();
}

// The track is about to be deleted or replaced: the step is rolled back and
// the dialog closed.
void smoothUi::cancelStep()
{
    if(bStepOpen) reject();
}

// Every way the dialog closes comes here: OK (accept()), Cancel, Escape and
// the window's close button (reject()), and cancelStep(). OK smooths the track
// (smoothing.apply) and commits the step; the others roll it back, which
// rebuilds the track: the rows and curHandler are let go of and made again
// (trackHandler::trackAboutToReset() and trackReset()), and nothing here uses
// an entry after endStep().
void smoothUi::done(int r)
{
    if(bStepOpen) {
        if(r == QDialog::Accepted) {
            runCommand("smoothing.apply", QVariantMap());
        }
        bStepOpen = false;
        m_handler->mHistory->endStep(r == QDialog::Accepted);
        gloParent->setUndoButtons();
    }
    QDialog::done(r);
}

// Runs a command on the track inside the dialog's step (it joins the step).
// False, with the reason in the log, when it fails; a failed command puts the
// track back as it was before it, which rebuilds the entries: curHandler and
// every entry pointer are dead afterwards.
bool smoothUi::runCommand(const QString& name, const QVariantMap& args)
{
    QString error;
    if(!bStepOpen) qWarning("smoothing dialog: %s runs with no step open", qPrintable(name));
    if(!commandRegistry::run(m_handler->mHistory, name, commandTarget::forTrack(m_handler->getID()-1), args, &error)) {
        qWarning("smoothing dialog: %s", qPrintable(error));
        // refused: the boxes show the selected entry's values again, not the
        // number the model did not take
        on_smoothUnitTree_itemSelectionChanged();
        return false;
    }
    return true;
}

int smoothUi::entryIndex(smoothHandler* _handler)
{
    if(_handler == NULL) return -1;
    return m_track->smoothList.indexOf(_handler);
}

// The arguments of smoothing.setEntry that leave the entry as it is; a slot
// changes the one value its box edits. The dialog smooths the track once, at
// OK (apply false).
QVariantMap smoothUi::entryArgs(int entry)
{
    smoothHandler* cur = m_track->smoothList[entry];
    QVariantMap args;
    args.insert("entry", entry);
    args.insert("length", cur->getLength()/1000.);
    args.insert("passes", cur->getIterations());
    args.insert("on", cur->active);
    args.insert("from", cur->getFrom()/1000.);
    args.insert("to", cur->getTo()/1000.);
    args.insert("name", cur->sName);
    args.insert("apply", false);
    return args;
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
    // the rows' itemChanged() then comes from the model, not from a person
    bool oldShowing = bShowingFields;
    bShowingFields = true;
    if(fields & smoothHandler::fieldLabel) row->setText(0, _handler->sLabel);
    if(fields & smoothHandler::fieldName) row->setText(1, _handler->sName);
    if(fields & smoothHandler::fieldFrom) row->setText(2, QString::number(_handler->getFrom()/1000.).append("s"));
    if(fields & smoothHandler::fieldTo) row->setText(3, QString::number(_handler->getTo()/1000.).append("s"));
    if(fields & smoothHandler::fieldLength) row->setText(4, QString::number(_handler->getLength()/1000.).append("s"));
    if(fields & smoothHandler::fieldIterations) row->setText(5, QString::number(_handler->getIterations()));
    if(fields & smoothHandler::fieldActive) row->setCheckState(6, _handler->active ? Qt::Checked : Qt::Unchecked);
    bShowingFields = oldShowing;
}

// An entry with no row yet is a region added from outside the dialog
// (smoothing.addRegion gives it this listener): the list gets its row.
void smoothUi::smoothHandlerChanged(smoothHandler* which, int fields)
{
    QTreeWidgetItem* row = rowFor(which);
    if(row) showFields(which, row, fields);
    else if(m_track->smoothList.contains(which)) showRows();
}

// The entry is being deleted (it has left the track's list already). The row
// leaves the map before it is deleted: deleting it can move the selection,
// whose slot looks rows up again.
void smoothUi::smoothHandlerDeleted(smoothHandler* which)
{
    if(which == curHandler) curHandler = NULL;   // removed from outside the dialog
    QTreeWidgetItem* row = rows.take(which);
    delete row;
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
    int i = entryIndex(curHandler);
    if(i < 0) return;
    QVariantMap args = entryArgs(i);
    args.insert("length", arg1);
    phantomChanges = true;
    runCommand("smoothing.setEntry", args);
    generateWarnings();
    phantomChanges = false;
}

void smoothUi::on_iterBox_valueChanged(int arg1)
{
    if(phantomChanges || curHandler == NULL) return;
    int i = entryIndex(curHandler);
    if(i < 0) return;
    QVariantMap args = entryArgs(i);
    args.insert("passes", arg1);
    phantomChanges = true;
    runCommand("smoothing.setEntry", args);
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

    int i = entryIndex(curHandler);
    if(i < 0) return;
    QVariantMap args = entryArgs(i);
    args.insert("from", arg1);
    phantomChanges = true;
    runCommand("smoothing.setEntry", args);
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

    int i = entryIndex(curHandler);
    if(i < 0) return;
    QVariantMap args = entryArgs(i);
    args.insert("to", arg1);
    phantomChanges = true;
    runCommand("smoothing.setEntry", args);
    generateWarnings();
    phantomChanges = false;
}

void smoothUi::on_smoothUnitTree_itemChanged(QTreeWidgetItem *item, int column)
{
    // A change is run only when it comes from a person, not while the model
    // writes into the rows (showFields(): the row then shows what the entry
    // holds already).
    int i;
    for(i = 0; i < m_track->smoothList.size(); ++i) {
        if(rowFor(m_track->smoothList[i]) == item) break;
    }
    if(column == 1 && !bShowingFields && i < m_track->smoothList.size()) {
        // The entry keeps the name the row shows (it is saved with the project),
        // so a name typed into the list is stored at once, as the row did.
        if(m_track->smoothList[i]->sName != item->text(1)) {
            QVariantMap args = entryArgs(i);
            args.insert("name", item->text(1));
            runCommand("smoothing.setEntry", args);
        }
    }

    if(phantomChanges) return;
    phantomChanges = true;

    if(column == 6 && !bShowingFields && i < m_track->smoothList.size()) {
        bool on = item->checkState(6) == Qt::Checked;
        if(m_track->smoothList[i]->active != on) {
            QVariantMap args = entryArgs(i);
            args.insert("on", on);
            runCommand("smoothing.setEntry", args);
        }
    }

    generateWarnings();
    phantomChanges = false;
}

// The region the dialog's New always made: 0 to 1 s, 0.4 s long, one pass, on.
void smoothUi::on_newButton_released()
{
    QVariantMap args;
    args.insert("from", 0.0);
    args.insert("to", 1.0);
    args.insert("length", 0.4);
    args.insert("passes", 1);
    args.insert("on", true);
    args.insert("name", QString("custom Region"));
    args.insert("apply", false);
    runCommand("smoothing.addRegion", args);
    showRows();
}

void smoothUi::on_removeButton_released()
{
    int i;
    for(i = 0; i < m_track->smoothList.size(); ++i) {
        if(m_track->smoothList[i] == curHandler) break;
    }
    if(i >= m_track->smoothList.size() || curHandler->sec != NULL) return;
    // The command takes the entry out of the list before deleting it (deleting
    // it deletes its row, and the selection that moves on must not find it).
    curHandler = NULL;
    runCommand("smoothing.removeRegion", QVariantMap{{"region", i - m_track->lSections.size()}, {"apply", false}});
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
