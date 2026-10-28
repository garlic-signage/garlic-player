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
#ifndef LAUNCHERMANAGER_H
#define LAUNCHERMANAGER_H

#include <QObject>
#include <QtAndroidExtras>

class AndroidManager
{
    public:
        AndroidManager();
        bool hasLauncher();
        void fetchDeviceInformation();
        bool checkPermissiones();
        void disableScreenSaver();
        void sendCloseCorrect();
        QString getLauncherVersion();
        QString getLauncherName();
        QString getSmilIndexFromLauncher();
        QString getUUIDFromLauncher();
    protected:
        QAndroidJniObject MyActivity;
        QString launcher_name;
};

#endif // LAUNCHERMANAGER_H
