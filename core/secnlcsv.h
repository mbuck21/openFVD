#ifndef SECNLCSV_H
#define SECNLCSV_H

#include <QMap>
#include "track.h"
#include "section.h"

// A track sampled from a NoLimits 2 CSV export (loadTrack()); the geometry is fixed and the physics are simulated over it.
class secnlcsv : public section
{
public:
    secnlcsv(track* getParent, mnode* first);
    virtual int updateSection(int node = 0);
    virtual void saveSection(std::iostream& file);
    virtual void loadSection(std::iostream& file);
    virtual void legacyLoadSection(std::iostream& file);
    virtual void saveSection(std::stringstream& file);
    virtual void loadSection(std::stringstream& file);
    virtual float getMaxArgument();
    virtual bool isLockable(func* _func);
    virtual bool isInFunction(int index, subfunc* func);
    // Reads the file and computes the track. False, with the reason in *error,
    // when the file cannot be read or is not a NoLimits 2 CSV (nothing is
    // changed then).
    bool loadTrack(QString filename, QString* error = NULL);
    // The reading alone: the nodes of the file (position, front, left). Checks
    // the input: at least ten tab-separated columns in every line that is not
    // blank, and at least two lines of data.
    static bool readNodes(const QString& filename, QList<mnode>& nodes, QString* error);
private:
    QList<mnode> csvNodes;
    void initDistances();
    mnode getNodeAtDistance(float distance);
};

#endif // SECNLCSV_H
