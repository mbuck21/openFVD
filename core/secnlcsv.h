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
    void loadTrack(QString filename);
private:
    QList<mnode> csvNodes;
    void initDistances();
    mnode getNodeAtDistance(float distance);
};

#endif // SECNLCSV_H
