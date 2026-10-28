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
#include "video.h"
#include "smilparser/media/video.h"

using namespace MediaParser;


PlayerVideo::PlayerVideo(QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent) : PlayerBaseMedia(r_id, lc, config, parent)
{
    connect(&MediaDecoder, SIGNAL(finished()), this, SLOT(finished()));

    MediaDecoder.setVideoOutput(&VideoWidget);
}

PlayerVideo::~PlayerVideo()
{
   disconnect(&MediaDecoder, SIGNAL(finished()), this, SLOT(finished()));
}

void PlayerVideo::loadMedia(BaseMedia *media, Region *reg)
{
    SmilMedia = media;
    region    = reg;

   QString path = SmilMedia->getLoadablePath();
   if (isFileExists(path))
   {
       MediaDecoder.load(path);
       TVideo  *MyParser = qobject_cast<TVideo *>(media);
       MediaDecoder.setVolume(MyParser->getSoundLevel());
   }
   else
   {
        SmilMedia->finishedNotFound();
   }
}

void PlayerVideo::restart()
{
    if (SmilMedia->getLogContentId() != "")
        qInfo(PlayLog).noquote() << createPlayLogXml();

    MediaDecoder.restart();

    if (SmilMedia->getLogContentId() != "")
        qInfo(PlayLog).noquote() << createPlayLogXml();
}

void PlayerVideo::play()
{
    MediaDecoder.play();
    if (SmilMedia->getLogContentId() != "")
        setStartTime();
}

void PlayerVideo::stop()
{
    MediaDecoder.stop();
    MediaDecoder.unload();
    if (SmilMedia->getLogContentId() != "")
        qInfo(PlayLog).noquote() << createPlayLogXml();
}

void PlayerVideo::resume()
{
    MediaDecoder.resume();
}

void PlayerVideo::pause()
{
    // todo add support for pauseDisplay
    MediaDecoder.pause();
}

void PlayerVideo::changeSize(int w, int h)
{
    Q_UNUSED(w)
    Q_UNUSED(h)
    if (!exists)
        return;

    QString fit = SmilMedia->getFit();
    if (fit == "fill")
        VideoWidget.ignoreAspectRatio();
    else if (fit == "meet")
        VideoWidget.keepAspectRatio();
    else if (fit == "meetBest")
        VideoWidget.keepAspectRatio();
    else if (fit == "slice")
        VideoWidget.keepAspectRatioByExpanding();
}

QWidget *PlayerVideo::getView()
{
    if (!exists)
        return Q_NULLPTR;
    return VideoWidget.getVideoWidget();
}

void PlayerVideo::finished()
{
   SmilMedia->finishIntrinsicDuration();
}
