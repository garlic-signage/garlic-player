#ifndef MEDIALIST_H
#define MEDIALIST_H

#include <QObject>
#include <QVector>
#include "head_parser.h"
#include "base_media.h"

using namespace MediaParser;

/**
 * @brief Maybe obsolete know. 2021-05-14
 */
class MediaList : public QObject
{
        Q_OBJECT
    public:
        explicit      MediaList(HeadParser *hp, QObject *parent = nullptr);
                     ~MediaList();
        BaseMedia    *getMediaOnZoneAndPosition(int position, int zone);
        void          insertSmilMedia(BaseMedia *MyBaseMedia);
    private:
        QMap<QString, QVector<BaseMedia *> *>  media_list;
        BaseMedia     *getMediaOnPosition(int position, QVector<BaseMedia *> list);
        QVector<QString>                       regions;
        QString                                determineZoneName(int zone);

    signals:

};

#endif // MEDIALIST_H
