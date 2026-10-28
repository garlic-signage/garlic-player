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
#include "audio.h"
#include "smilparser/media/audio.h"

using namespace MediaParser;

PlayerAudio::PlayerAudio(QString r_id, Launcher *lc, MainConfiguration *config, QObject *parent) : PlayerBaseMedia(r_id, lc, config, parent)
{
    connect(&MediaDecoder, SIGNAL(finished()), this, SLOT(finished()));
}

PlayerAudio::~PlayerAudio()
{
    MediaDecoder.unload();
}

void PlayerAudio::loadMedia(BaseMedia *media, Region *reg)
{
    SmilMedia    = media;
    region       = reg;
    QString path = SmilMedia->getLoadablePath();
    if (isFileExists(path))
    {
        MediaDecoder.load(path);
        TAudio  *MyParser = qobject_cast<TAudio *>(media);
        MediaDecoder.setVolume(MyParser->getSoundLevel());
    }
    else
    {
         SmilMedia->finishedNotFound();
    }
}

void PlayerAudio::restart()
{
    if (SmilMedia->getLogContentId() != "")
        qInfo(PlayLog).noquote() << createPlayLogXml();

    MediaDecoder.restart();

    if (SmilMedia->getLogContentId() != "")
        qInfo(PlayLog).noquote() << createPlayLogXml();
}

void PlayerAudio::play()
{
    MediaDecoder.play();
    if (SmilMedia->getLogContentId() != "")
        setStartTime();
}

void PlayerAudio::stop()
{
    MediaDecoder.stop();
    if (SmilMedia->getLogContentId() != "")
        qInfo(PlayLog).noquote() << createPlayLogXml();
}

void PlayerAudio::resume()
{
    MediaDecoder.play();
}

void PlayerAudio::pause()
{
    MediaDecoder.pause();
}

void PlayerAudio::changeSize(int w, int h)
{
    Q_UNUSED(w);Q_UNUSED(h)
}


QWidget *PlayerAudio::getView()
{
    return Q_NULLPTR;
}

void PlayerAudio::finished()
{
   SmilMedia->finishIntrinsicDuration();
}
