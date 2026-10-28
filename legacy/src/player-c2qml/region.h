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

#include <QQuickItem>

#include "lib_facade.h"
#include "media/media_factory.h"
#include "media/player_base_media.h"

class TRegion : public QQuickItem
{
        Q_OBJECT
    public:
        explicit TRegion(LibFacade *lf, QObject *parent);
        ~TRegion();
        void                setRootSize(int w, int h);
        void                setRegion(Region r, Launcher *lc, QQmlEngine *e, MainConfiguration *config);
        void                startShowMedia(BaseMedia *media);
        void                stopShowMedia(BaseMedia *media);
        void                resumeShowMedia(BaseMedia *media);
        void                pauseShowMedia(BaseMedia *media);
    public slots:
        void clickSlot();

    protected:
        qint64                last_touch = 0;
        int                   count_touch = 0;
        QQuickItem           *root_item;
        QQmlEngine           *engine;
        LibFacade            *MyLibFacade;
        Region               region;
        PlayerBaseMedia                    *MyMedia = Q_NULLPTR;
        QMap<QString, PlayerBaseMedia *>    MyMediaList;

        QScopedPointer<QQmlComponent>       rectangle;
        QScopedPointer<QQmlComponent>       media_component;
        QScopedPointer<QQuickItem>          rectangle_item;

        QScopedPointer<MediaFactory>        MyMediaFactory;
        qreal                 root_width_px = 0;
        qreal                 root_height_px = 0;

        void                secureStopDisplayingMedia(PlayerBaseMedia *TmpMedia);
        void                registerEventStarts();
        void                registerEventEnds();
        void                resizeGeometry();
        PlayerBaseMedia    *findMediaById(QString id);
};

#endif // TREGION_H
