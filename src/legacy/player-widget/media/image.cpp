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
#include "media/image.h"


PlayerImage::PlayerImage(QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent) : PlayerBaseMedia(r_id, lc, config, parent)
{
}

PlayerImage::~PlayerImage()
{
}

void PlayerImage::loadMedia(BaseMedia *media, Region *reg)
{
    SmilMedia    = media;
    region       = reg;
    QString path = SmilMedia->getLoadablePath();
    if (isFileExists(path))
    {
        // because sometimes people name a png with jpg extension or vice versa
        // in this case loaded_image.load(path)
        QFile CurrentFile(path);
        if(!CurrentFile.open(QIODevice::ReadOnly))
        {
            SmilMedia->finishedNotFound();
            return;
        }

        if (!loaded_image.loadFromData(CurrentFile.readAll()))
        {
            SmilMedia->finishedNotFound();
            return;
        }
    }
    else
    {
         SmilMedia->finishedNotFound();
    }
}

void PlayerImage::restart()
{
    stop();
    play();
}

void PlayerImage::play()
{
    ImageWidget.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    ImageWidget.setPixmap(loaded_image);
    if (SmilMedia->getLogContentId() != "")
        setStartTime();
}

void PlayerImage::stop()
{
    if (SmilMedia->getLogContentId() != "")
        qInfo(PlayLog).noquote() << createPlayLogXml();
}

void PlayerImage::resume()
{
    // todo add support for pauseDisplay
}

void PlayerImage::pause()
{
}

void PlayerImage::changeSize(int w, int h)
{
    if (!exists)
        return;

    QString fit = SmilMedia->getFit().toLower();    

    if (fit == "fill")
       ImageWidget.setPixmap(loaded_image.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    else if (fit == "meet")
        ImageWidget.setPixmap(loaded_image.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else if (fit == "meetbest")
    {
        if (loaded_image.width() >= w || loaded_image.height() > h)
            ImageWidget.setPixmap(loaded_image.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            ImageWidget.setPixmap(loaded_image);
    }
    else if (fit == "slice")
        ImageWidget.setPixmap(loaded_image.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    else
        ImageWidget.setPixmap(loaded_image);

    setAlignment();
}

QWidget *PlayerImage::getView()
{
    if (!exists)
        return Q_NULLPTR;

    return &ImageWidget;
}

void PlayerImage::setAlignment()
{
    QString media_align = SmilMedia->getMediaAlign().toLower();
    if (media_align.isEmpty())
        media_align = region->mediaAlign;

     if(media_align == "center")
         ImageWidget.setAlignment(Qt::AlignCenter | Qt::AlignCenter);
     else if(media_align == "topleft")
         ImageWidget.setAlignment(Qt::AlignTop | Qt::AlignLeft);
     else if(media_align == "topmid")
         ImageWidget.setAlignment(Qt::AlignTop | Qt::AlignHCenter);
     else if(media_align == "topright")
         ImageWidget.setAlignment(Qt::AlignTop | Qt::AlignRight);
     else if(media_align == "midleft")
         ImageWidget.setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
     else if(media_align == "midright")
         ImageWidget.setAlignment(Qt::AlignVCenter | Qt::AlignRight);
     else if(media_align == "bottomleft")
         ImageWidget.setAlignment(Qt::AlignBottom | Qt::AlignLeft);
     else if(media_align == "bottommid")
         ImageWidget.setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
     else if(media_align == "bottomright")
         ImageWidget.setAlignment(Qt::AlignBottom | Qt::AlignRight);
     else
         ImageWidget.setAlignment(Qt::AlignTop | Qt::AlignLeft);
}
