#include "brush.h"

PlayerBrush::PlayerBrush(QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent) : PlayerBaseMedia(r_id, lc, config, parent)
{
    BrushWidget.reset(new QLabel);
}

PlayerBrush::~PlayerBrush()
{

}

void PlayerBrush::loadMedia(BaseMedia *media, Region *reg)
{
    SmilMedia = media; // other wise it cannote stopped in  TRegion::stopShowMedia
    region    = reg;
    MyBrush   = qobject_cast<TBrush *> (media);
    BrushWidget.data()->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    BrushWidget.data()->setAutoFillBackground(true);
    BrushWidget.data()->setStyleSheet("background-color:" + MyBrush->getColor());
}

void PlayerBrush::restart()
{
    stop();
    play();
}

void PlayerBrush::play()
{
    if (MyBrush->getLogContentId() != "")
        setStartTime();
}

void PlayerBrush::stop()
{
    if (MyBrush->getLogContentId() != "")
        qInfo(PlayLog).noquote() << createPlayLogXml();
}

void PlayerBrush::resume()
{
    // todo add support for pauseDisplay
}

void PlayerBrush::pause()
{
}

void PlayerBrush::changeSize(int w, int h)
{
    Q_UNUSED(w);Q_UNUSED(h)
}

QWidget *PlayerBrush::getView()
{
    return BrushWidget.data();
}
