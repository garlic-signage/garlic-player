#include "medialist.h"

MediaList::MediaList(HeadParser *hp, QObject *parent) : QObject(parent)
{
    QList<Region> *layout = hp->getLayout();
    for (int i = 0; i < layout->length(); i++)
    {
        regions.append(layout->at(i).regionName);
    }
}

MediaList::~MediaList()
{
    // not neccesary do delete here cause same objects are in all_elements_list
    media_list.clear();
}

void MediaList::insertSmilMedia(BaseMedia *MyBaseMedia)
{
    if (!MyBaseMedia->isMedia())
    {
        return;
    }

    QString s = MyBaseMedia->getRegionName();

    QMap<QString, QVector<BaseMedia *>*>::iterator i = media_list.find(s);
    if (i != media_list.end())
    {
        i.value()->append(MyBaseMedia);
    }
    else
    {
        QVector<BaseMedia *> *vec = new QVector<BaseMedia *>;
        vec->append(MyBaseMedia);
        media_list.insert(s, vec);
    }
}


BaseMedia *MediaList::getMediaOnZoneAndPosition(int position, int zone)
{
    QMap<QString, QVector<BaseMedia *> *>::iterator i =  media_list.find(determineZoneName(zone));

    if (i != media_list.end())
    {
        return getMediaOnPosition(position, *i.value());
    }
    return Q_NULLPTR;
}

BaseMedia *MediaList::getMediaOnPosition(int position, QVector<BaseMedia *> list)
{
    int array_position = position - 1;

    if (array_position < 0)
    {
        return list.first();
    }
    if (array_position > list.size())
    {
        return list.last();
    }
    return list.at(array_position);

}

QString MediaList::determineZoneName(int zone)
{
    int array_position = zone - 1;
    if (array_position < 1)
    {
        return regions.first();
    }

    if (array_position > regions.size())
    {
        return regions.last();
    }
    // zone-1 cause arrays start as 0
    return regions.at(zone-1);
}

