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
#include "media_factory.h"
#include "audio.h"
#include "media/brush.h"
#include "media/image.h"
#include "media/ref_command.h"
#include "media/video.h"
#include "media/web.h"

MediaFactory::MediaFactory(QString id, Launcher *l, MainConfiguration *config, QObject *parent) : QObject(parent)
{
    region_name     = id;
    MyLauncher      = l;
    MyMainConfiguration = config;
}

PlayerBaseMedia *MediaFactory::create(MainConfiguration *config, BaseMedia *media)
{
    QString type   = media->objectName();
    if (type == "TImage")
    {
        MyImage = new PlayerImage(region_name, MyLauncher, config, this);
        return MyImage;
    }
    else if (type == "TBrush")
    {
        MyBrush = new PlayerBrush(region_name, MyLauncher,config, this);
        return MyBrush;
    }
    else if (type == "TVideo")
    {
        MyVideo = new PlayerVideo(region_name, MyLauncher, config, this);
        return MyVideo;
    }
    else if (type == "TAudio")
    {
        MyAudio = new PlayerAudio(region_name, MyLauncher, config, this);
        return MyAudio;
    }
    else if (type == "TWeb")
    {
        MyWeb = new PlayerWeb(region_name, MyLauncher, config, this);
        return MyWeb;
    }
    else if (type == "TWidget")
    {
        MyWeb = new PlayerWeb(region_name, MyLauncher, config, this);
        return MyWeb;
    }
    else if (type == "TRefCommand")
    {
        MyRefCommand = new PlayerRefCommand(region_name, MyLauncher, config, this);
        return MyRefCommand;
    }
    else
        return Q_NULLPTR;
}
