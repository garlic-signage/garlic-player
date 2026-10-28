#ifndef AUDIO_H
#define AUDIO_H

#include <QLabel>
#include "mediaplayer_wrapper.h"
#include "player_base_media.h"

class PlayerAudio : public PlayerBaseMedia
{
        Q_OBJECT
    public:
        explicit PlayerAudio(QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent = nullptr);
        ~PlayerAudio();
        void          loadMedia(BaseMedia *media, Region *reg);
        void          restart();
        void          play();
        void          stop();
        void          resume();
        void          pause();
        QWidget      *getView();
        void          changeSize(int w, int h);
    protected:
        MediaPlayerWrapper   MediaDecoder;
    protected slots:
        void                  finished();
};

#endif // AUDIO_H
