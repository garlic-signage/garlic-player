#ifndef MPIMAGE_H
#define MPIMAGE_H

#include <QLabel>
#include <QImageReader>
#include <QPixmap>
#include "player_base_media.h"

    class PlayerImage : public PlayerBaseMedia
    {
            Q_OBJECT
        public:            explicit      PlayerImage(QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent = nullptr);
            ~PlayerImage();
            void          loadMedia(BaseMedia *media, Region *reg);
            void          restart();
            void          play();
            void          stop();
            void          resume();
            void          pause();
            void          changeSize(int w, int h);
            QWidget      *getView();
        protected:
            QPixmap       loaded_image;
            void          setAlignment();
            QLabel        ImageWidget;
    };


#endif // MPIMAGE_H
