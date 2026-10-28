/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2016 Nikolaos Saghiadinos <ns@smil-control.com>
    This file is part of the garlic-player source code

    This program is free software: you can redistribute it and/or  modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************/

#include "region.h"
#include <QLayout>

TRegion::TRegion(LibFacade *lf, QWidget *parent) : QWidget(parent)
{
    setParent(parent);
    MyLibFacade = lf;
    layout.reset(new QStackedLayout(this));
    layout.data()->setMargin(0);
    setLayout(layout.data());
}

TRegion::~TRegion()
{
    MyMediaList.clear();
}

/**
 * @brief TRegion::paintEvent needed to draw a background color setted in smil layout header
 * @param event
 */
void TRegion::paintEvent(QPaintEvent *)
{
    QStyleOption o;
    o.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &p, this);
}

void TRegion::startShowMedia(BaseMedia *media)
{
    // check if this is media is already player now and if yes restart only
    PlayerBaseMedia  *MyTmpMedia  = findMediaById(media->getID());
    if (MyTmpMedia != Q_NULLPTR)
    {      
        MyMedia = MyTmpMedia;
        MyMedia->restart();
        return;
    }

    MyMedia = MyMediaFactory.data()->create(MyLibFacade->getConfiguration(), media);
    if (MyMedia == Q_NULLPTR)
        return;


    MyMediaList.insert(media->getID(), MyMedia);
    MyMedia->loadMedia(media, &region);
    MyMedia->play();

    QWidget *widget = MyMedia->getView();
    if (widget != Q_NULLPTR)
    {
        layout.data()->addWidget(widget);
        layout.data()->setCurrentWidget(widget);
    }
}

void TRegion::stopShowMedia(BaseMedia *media)
{
    MyMedia = findMediaById(media->getID());
    if (MyMedia == Q_NULLPTR)
        return;

    QWidget *widget = MyMedia->getView();
    if (widget != Q_NULLPTR)
    {
        layout.data()->removeWidget(widget);
    }

    MyMedia->stop();

    MyMediaList.remove(media->getID());
    delete MyMedia;
    MyMedia = Q_NULLPTR;
}

void TRegion::resumeShowMedia(BaseMedia *media)
{
    MyMedia = findMediaById(media->getID());
    if (MyMedia == Q_NULLPTR)
    {
        startShowMedia(media); // could be a resume by defer
        return;
    }

    MyMedia->resume();
    QWidget *widget = MyMedia->getView();
    if (widget != Q_NULLPTR) // bring current widget to the top of the LayoutStack
    {
        layout.data()->setCurrentWidget(widget);
    }
}

void TRegion::pauseShowMedia(BaseMedia *media)
{
    MyMedia = findMediaById(media->getID());
    if (MyMedia == Q_NULLPTR)
        return;

    MyMedia->pause();
}


bool TRegion::event(QEvent *event)
{
    if(event->type() == QEvent::TouchBegin || event->type() == QEvent::MouseButtonPress)
    {
        qint64 delay = QDateTime::currentMSecsSinceEpoch() - last_touch;
        if (delay < 500)
        {
            count_touch++;
        }
        else
        {
            count_touch = 0;
        }

        if (count_touch > 2)
        {
            count_touch = 0;
        }
    }
    if(event->type() == QEvent::TouchEnd || event->type() == QEvent::MouseButtonRelease)
    {
        last_touch = QDateTime::currentMSecsSinceEpoch();
        if (count_touch < 2 && MyMedia != Q_NULLPTR)
        {
            event->accept();
            BaseMedia *smil = MyMedia->getSmilMedia();
            if (smil != Q_NULLPTR)
                smil->emitActivated();
        }
    }
    return QWidget::event(event);

}

/**
 * Needs the real pixel of the actual root size regardless
 * if fullscreen or in window
 *
 * @brief TRegion::setRootSize
 * @param w
 * @param h
 */
void TRegion::setRootSize(int w, int h)
{

    root_width_px  = w;
    root_height_px = h;
    resizeGeometry();
}

void TRegion::setRegion(Region r, Launcher *lc, MainConfiguration *config)
{
    region = r;
    determineStylesheets();
    MyMediaFactory.reset(new MediaFactory(r.regionName, lc, config, this));
}

void TRegion::resizeGeometry()
{
    qreal  xr, yr, wr, hr = 0.0;
    xr = (root_width_px*region.left);
    yr = (root_height_px*region.top);
    wr = (root_width_px*region.width*1);
    hr = (root_height_px*region.height*1);
    setGeometry(qRound(xr), qRound(yr), qRound(wr), qRound(hr));
    if (MyMedia != Q_NULLPTR)
        MyMedia->changeSize(qRound(wr), qRound(hr));
}

void TRegion::secureStopDisplayingMedia(PlayerBaseMedia *TmpMedia)
{
    // MyMedia can be null, when file path not found
    if (TmpMedia == Q_NULLPTR)
        return;

    QWidget *widget = TmpMedia->getView();

    if (widget != Q_NULLPTR)
    {
        layout.data()->removeWidget(widget);
    }
    TmpMedia->stop();
    TmpMedia = Q_NULLPTR;
}

void TRegion::determineStylesheets()
{
    QString style_sheet = "background: " + region.backgroundColor;
    style_sheet += " " + determineBackgroundImage();
    style_sheet += " " + determineBackgroundImageRepeat() + ";";
    setStyleSheet(style_sheet);
}

QString TRegion::determineBackgroundImage()
{
    QString ret = "";
    // currently we do not support sub regions (inherited)
    if (region.backgroundImage.toLower() != "none" && region.backgroundImage.toLower() != "inherited")
    {
        ret = "url(\""+MyLibFacade->requestLoaddableMediaPath(region.backgroundImage)+"\")";
        has_background_image = true;
    }
    return ret;
}

QString TRegion::determineBackgroundImageRepeat()
{
    if (!has_background_image)
        return "";

    QString ret = "";
    if (region.backgroundRepeat.toLower() == "repeat")
    {
        ret = "repeat";
    }
    else if (region.backgroundRepeat.toLower() == "repeatx")
    {
        ret = "repeat-x";
    }
    else if (region.backgroundRepeat.toLower() == "repeaty")
    {
        ret = "repeat-y";
    }
    else if (region.backgroundRepeat.toLower() == "norepeat")
    {
        ret = "no-repeat";
    }
  // sub regions snot supported so no inherited neccesary
    else if (region.backgroundRepeat.toLower() == "inherited")
    {
        ret = "repeat";
    }

    return ret;
}

PlayerBaseMedia *TRegion::findMediaById(QString id)
{
    PlayerBaseMedia                   *MyTmpMedia  = Q_NULLPTR;
    QMap<QString, PlayerBaseMedia *>::iterator  it = MyMediaList.find(id);
    if (it != MyMediaList.end())
    {
        MyTmpMedia = *it;
    }
    return MyTmpMedia;
}

