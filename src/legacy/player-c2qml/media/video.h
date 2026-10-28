#ifndef VIDEO_H
#define VIDEO_H

#include "audio.h"

class Video : public Audio
{
        Q_OBJECT
    public:
        const int STRETCH            = 0;
        const int PRESERVEASPECTFIT  = 1;
        const int PRESERVEASPECTCROP = 2;
        const int TILE               = 4;
        const int TILE_V             = 5;
        const int NONE               = 6;

        Video(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent = nullptr);
        ~Video();
        void         loadInternal();
        void         changeSize(int w, int h);

};

#endif // VIDEO_H
