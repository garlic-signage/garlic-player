#ifndef VIDEO_H
#define VIDEO_H

#include "mediaplayer_wrapper.h"
#include "mediawidget_wrapper.h"
#include "player_base_media.h"

class PlayerVideo : public PlayerBaseMedia
{
        Q_OBJECT
    public:
        PlayerVideo(QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent = nullptr);
        ~PlayerVideo();
        void              loadMedia(BaseMedia *media, Region *reg);
        void              restart();
        void              play();
        void              stop();
        void              resume();
        void              pause();
        void              changeSize(int w, int h);
        QWidget          *getView();
    protected:
        MediaWidgetWrapper  VideoWidget;
        MediaPlayerWrapper  MediaDecoder;
    protected slots:
        void                  finished();
};

#endif // VIDEO_H
