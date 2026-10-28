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

#ifndef TREGION_H
#define TREGION_H

#include <QObject>
#include <QWidget>
#include <QStackedLayout>
#include <QPainter>
#include <QPixmap>
#include <QPaintEvent>
#include <QStyle>
#include <QStyleOption>
#include <QGraphicsOpacityEffect>
#include <event.h>
#include "head_parser.h"
#include "lib_facade.h"


#include "media/media_factory.h"

class TRegion : public QWidget
{
    Q_OBJECT
public:
    explicit TRegion(LibFacade *lf, QWidget *parent);
    ~TRegion();
    void                paintEvent(QPaintEvent *);
    void                setRootSize(int w, int h);
    void                setRegion(Region region, Launcher *lc, MainConfiguration *config);
    void                startShowMedia(BaseMedia *media);
    void                stopShowMedia(BaseMedia *media);
    void                resumeShowMedia(BaseMedia *media);
    void                pauseShowMedia(BaseMedia *media);
    Region             *getRegion() {return &region;}
    bool                event(QEvent *event);
protected:
    qint64                last_touch = 0;
    int                   count_touch = 0;
    qreal                 root_width_px = 0;
    qreal                 root_height_px = 0;
    QString               actual_media = "";
    Region                region;
    LibFacade            *MyLibFacade;
    QScopedPointer<QStackedLayout>          layout;
    PlayerBaseMedia      *MyMedia = Q_NULLPTR;
    QMap<QString, PlayerBaseMedia *>    MyMediaList;
    QScopedPointer<MediaFactory>        MyMediaFactory;
    bool                  has_background_image = false;
    void                  resizeGeometry();
    void                  secureStopDisplayingMedia(PlayerBaseMedia *TmpMedia);
    void                  determineStylesheets();
    QString               determineBackgroundImage();
    QString               determineBackgroundImageRepeat();
    PlayerBaseMedia      *findMediaById(QString id);
};

#endif // TREGION_H
