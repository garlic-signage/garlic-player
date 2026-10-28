#include "regions_list.h"

RegionsList::RegionsList(QObject *parent) : QObject(parent)
{
}

RegionsList::~RegionsList()
{
    remove();
}


void RegionsList::create(LibFacade *MyLibFacade, Launcher *lc, QSize rs)
{
    root_size = rs;
    QList<Region> *region_list = MyLibFacade->getHead()->getLayout();
    QMap<QString, TRegion *>::iterator j;
    for (int i = 0; i < region_list->length(); i++)
    {
        j = regions_list.insert(region_list->at(i).id, new TRegion(MyLibFacade, (QWidget *) parent()));
        regions_list[j.key()]->setRegion(region_list->at(i), lc, MyLibFacade->getConfiguration());
        regions_list[j.key()]->setRootSize(root_size.width(), root_size.height());
        regions_list[j.key()]->show();
        QString region_name = region_list->at(i).regionName;
        if (region_name.isEmpty())
        {
            region_name = region_list->at(i).id;
        }

        regions_assign_list.insert(region_name, region_list->at(i).id);
    }
    qDebug() << regions_list.size() << " region(s) created";
}

void RegionsList::remove()
{
    int size = regions_list.size();
    if (size == 0) // prevent to call functionx of deleted or not existing regions
    {
        return;
    }
    qDeleteAll(regions_list);
    regions_list.clear();
    regions_assign_list.clear();
    qDebug() << size << " region(s) deleted";
}

void RegionsList::resize(QSize rs)
{
    root_size = rs;
    if (regions_list.size() == 0)
        return;

    for (QMap<QString, TRegion *>::iterator i = regions_list.begin(); i != regions_list.end(); ++i)
    {
        regions_list[i.key()]->setRootSize(root_size.width(), root_size.height());
    }
}

QStringList RegionsList::select(QString region_name)
{
    QStringList region_names_list;

    QMultiMap<QString, QString>::iterator i = regions_assign_list.find(region_name);
    while (i != regions_assign_list.end() && i.key() == region_name)
    {
        region_names_list << i.value();
        ++i;
    }

    if (region_names_list.isEmpty())
    {
        region_names_list.append(regions_list.begin().key());
    }
    return region_names_list;
}

void RegionsList::startShowMedia(BaseMedia *media)
{
    if (regions_list.size() == 0) // prevent to call functionx of deleted or not existing regions
        return;

    QStringList region_ids = select(media->getRegionName());
    QStringList::iterator i;
    for (i = region_ids.begin(); i != region_ids.end(); ++i)
    {
        regions_list[*i]->startShowMedia(media);
        regions_list[*i]->setRootSize(root_size.width(), root_size.height());
   }
}

void RegionsList::stopShowMedia(BaseMedia *media)
{
    if (regions_list.size() == 0) // prevent to call functionx of deleted or not existing regions
        return;

    QStringList region_ids = select(media->getRegionName());
    QStringList::iterator i;
    for (i = region_ids.begin(); i != region_ids.end(); ++i)
    {
        regions_list[*i]->stopShowMedia(media);
   }
}

void RegionsList::resumeShowMedia(BaseMedia *media)
{
    if (regions_list.size() == 0) // prevent to call functionx of deleted or not existing regions
        return;

    QStringList region_ids = select(media->getRegionName());
    QStringList::iterator i;
    for (i = region_ids.begin(); i != region_ids.end(); ++i)
    {
        regions_list[*i]->resumeShowMedia(media);
   }
}

void RegionsList::pauseShowMedia(BaseMedia *media)
{
    if (regions_list.size() == 0) // prevent to call functionx of deleted or not existing regions
        return;

    QStringList region_ids = select(media->getRegionName());
    QStringList::iterator i;
    for (i = region_ids.begin(); i != region_ids.end(); ++i)
    {
        regions_list[*i]->pauseShowMedia(media);
   }
}
