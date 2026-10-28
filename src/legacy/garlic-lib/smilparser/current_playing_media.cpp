/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2021 Nikolaos Sagiadinos <ns@smil-control.com>
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
#include "current_playing_media.h"

CurrentPlayingMedia::CurrentPlayingMedia(Files::MediaManager *mm, QObject *parent) : QObject(parent)
{
    MyMediaManager = mm;
}

void CurrentPlayingMedia::insert(BaseMedia *SmilMedia)
{
    currently_playing_smilobjects.insert(SmilMedia);
    MyMediaManager->insertCurrentlyPlaying(SmilMedia->getSrc());
}

bool CurrentPlayingMedia::remove(BaseMedia *SmilMedia)
{
    bool ret = currently_playing_smilobjects.remove(SmilMedia);
    MyMediaManager->removeCurrentlyPlaying(SmilMedia->getSrc());
    return ret;
}

int CurrentPlayingMedia::count()
{
   return currently_playing_smilobjects.size();
}


BaseMedia *CurrentPlayingMedia::getFirstPlayingObject()
{
    QSet<BaseMedia *>::iterator i;
    if (currently_playing_smilobjects.size() > 0)
    {
        i = currently_playing_smilobjects.begin();
        return *i;
    }
    else
    {
        return Q_NULLPTR;
    }

}
