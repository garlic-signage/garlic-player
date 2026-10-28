#include "brush.h"

Brush::Brush(QQmlComponent *mc, QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent) : PlayerBaseMedia(mc, r_id, lc, config, parent)
{
    setRegionId(r_id);
    QString str(
                "import QtQuick 2.12\n \
                    Rectangle {  \
                        id: "+getRegionId()+"_image; \
                        anchors.fill: parent; \n \
                }\n"
    );
    brush_item.reset(createMediaItem(mc, str));

}

Brush::~Brush()
{
}

void Brush::loadMedia(BaseMedia *media, Region *reg)
{
    SmilMedia = media;
    region    = reg;
    MyBrush = qobject_cast<TBrush *> (media);
    brush_item.data()->setProperty("color", MyBrush->getColor());
}

void Brush::restart()
{
    stop();
    play();

}

void Brush::play()
{
    brush_item.data()->setVisible(true);
    if (SmilMedia->getLogContentId() != "")
        setStartTime();
}

void Brush::stop()
{
    if (!SmilMedia->getLogContentId().isEmpty())
        qInfo(PlayLog).noquote() << createPlayLogXml();
    brush_item.data()->setVisible(false);
}

void Brush::resume()
{
    // todo add support for pauseDisplay
    brush_item.data()->setVisible(true);
}

void Brush::pause()
{
    brush_item.data()->setVisible(true);
}

void Brush::changeSize(int w, int h)
{
    Q_UNUSED(w);Q_UNUSED(h)
}

void Brush::setParentItem(QQuickItem *parent)
{
    brush_item.data()->setParentItem(parent);
}
