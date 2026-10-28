#ifndef MPIMAGE_H
#define MPIMAGE_H

#include "player_base_media.h"
#include <QImageReader>
#include <QPixmap>
#include <QQuickImageProvider>
#include <QQuickImageResponse>
class Image : public PlayerBaseMedia
{
        Q_OBJECT
    public:
        const int NONE               = 6;

        explicit      Image(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent = nullptr);
        ~Image();
        void          loadMedia(BaseMedia *media, Region *reg);
        void          restart();
        void          play();
        void          stop();
        void          resume();
        void          pause();
        void          setParentItem(QQuickItem *parent);
        void          changeSize(int w, int h);
    protected:
        QPixmap       loaded_image;
        QScopedPointer<QQuickItem, QScopedPointerDeleteLater>  image_item;
        void                        setAlignment();

};


#endif // MPIMAGE_H
