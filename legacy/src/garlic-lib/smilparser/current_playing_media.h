#ifndef CURRENTPLAYINGMEDIA_H
#define CURRENTPLAYINGMEDIA_H

#include <QObject>
#include <QSet>
#include "media/base_media.h"

using namespace MediaParser;

class CurrentPlayingMedia : public QObject
{
        Q_OBJECT
    public:
        explicit CurrentPlayingMedia(Files::MediaManager *mm, QObject *parent = nullptr);
        void         insert(BaseMedia *SmilMedia);
        bool         remove(BaseMedia *SmilMedia);
        int          count();
        BaseMedia   *getFirstPlayingObject();
    protected:
        QSet<BaseMedia *>             currently_playing_smilobjects;
        Files::MediaManager          *MyMediaManager;

    signals:

};

#endif // CURRENTPLAYINGMEDIA_H
