#include "media/image.h"

Image::Image(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent) : PlayerBaseMedia(mc, r_id, lc, config, parent)
{
    setRegionId(r_id);
    QString str(
                "import QtQuick 2.12\n \
                    Image {  \
                        cache: false; \
                        id: "+getRegionId()+"_image; \
                        anchors.fill: parent; \
                }"
    );
    image_item.reset(createMediaItem(mc, str));
 }

Image::~Image()
{
}

void Image::loadMedia(BaseMedia *media, Region *reg)
{
    SmilMedia = media;
    region    = reg;
    if (load(image_item.data()))
    {
        QFile CurrentFile(SmilMedia->getLoadablePath());
        if(CurrentFile.open(QIODevice::ReadOnly))
              loaded_image.loadFromData(CurrentFile.readAll());
    }
}

void Image::restart()
{
    stop();
    play();
}

void Image::play()
{
    image_item.data()->setVisible(true);
    if (SmilMedia->getLogContentId() != "")
        setStartTime();
}


void Image::stop()
{
    if (!SmilMedia->getLogContentId().isEmpty())
        qInfo(PlayLog).noquote() << createPlayLogXml();
    image_item.data()->setVisible(false);
}

void Image::resume()
{
     // todo add support for pauseDisplay
   image_item.data()->setVisible(true);
}

void Image::pause()
{
    image_item.data()->setVisible(true);
}

void Image::changeSize(int w, int h)
{
    QString smil_fit = SmilMedia->getFit().toLower();
    int fill_mode = 6; // for do nothing
    if (smil_fit == "fill")
        fill_mode = Qt::IgnoreAspectRatio;
    else if (smil_fit == "meet")
        fill_mode = Qt::KeepAspectRatio;
    else if (smil_fit == "meetbest")
    {
        if (loaded_image.width() >= w || loaded_image.height() > h)
            fill_mode = Qt::KeepAspectRatio;
       else
            fill_mode = NONE;
    }
    else if (smil_fit == "slice")
        fill_mode =  Qt::KeepAspectRatioByExpanding;

    image_item.data()->setProperty("fillMode",  fill_mode);
    setAlignment();
}

void Image::setParentItem(QQuickItem *parent)
{
    image_item.data()->setParentItem(parent);
}

void Image::setAlignment()
{
    QString media_align = SmilMedia->getMediaAlign().toLower();
    if (media_align.isEmpty())
        media_align = region->mediaAlign;

     if(media_align == "center")
     {
         image_item.data()->setProperty("horizontalAlignment", Qt::AlignHCenter);
         image_item.data()->setProperty("verticalAlignment", Qt::AlignVCenter);
     }
     else if(media_align == "topleft")
     {
         image_item.data()->setProperty("horizontalAlignment", Qt::AlignLeft);
         image_item.data()->setProperty("verticalAlignment", Qt::AlignTop);
     }
     else if(media_align == "topmid")
     {
         image_item.data()->setProperty("horizontalAlignment", Qt::AlignHCenter);
         image_item.data()->setProperty("verticalAlignment", Qt::AlignTop);
     }
     else if(media_align == "topright")
     {
         image_item.data()->setProperty("horizontalAlignment", Qt::AlignRight);
         image_item.data()->setProperty("verticalAlignment", Qt::AlignTop);
     }
     else if(media_align == "midleft")
     {
         image_item.data()->setProperty("horizontalAlignment", Qt::AlignLeft);
         image_item.data()->setProperty("verticalAlignment", Qt::AlignVCenter);
     }
     else if(media_align == "midright")
     {
         image_item.data()->setProperty("horizontalAlignment", Qt::AlignRight);
         image_item.data()->setProperty("verticalAlignment", Qt::AlignVCenter);
     }
     else if(media_align == "bottomleft")
     {
         image_item.data()->setProperty("horizontalAlignment", Qt::AlignLeft);
         image_item.data()->setProperty("verticalAlignment", Qt::AlignBottom);
     }
     else if(media_align == "bottommid")
     {
         image_item.data()->setProperty("horizontalAlignment", Qt::AlignHCenter);
         image_item.data()->setProperty("verticalAlignment", Qt::AlignBottom);
     }
     else if(media_align == "bottomright")
     {
         image_item.data()->setProperty("horizontalAlignment", Qt::AlignRight);
         image_item.data()->setProperty("verticalAlignment", Qt::AlignBottom);
     }
     else
     {
         image_item.data()->setProperty("horizontalAlignment", Qt::AlignLeft);
         image_item.data()->setProperty("verticalAlignment", Qt::AlignTop);
     }
}
