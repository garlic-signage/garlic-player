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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

#include "regions_list.h"
#include "../player-common/configdialog.h"
#include "../player-common/screen.h"
#include "../player-common/debug_infos.h"
#include "../player-common/launcher.h"
#include "files/index_manager.h"
#include "files/media_manager.h"
class MainWindow : public QMainWindow
{
    Q_OBJECT

    public:
        MainWindow(TScreen *screen, LibFacade *lib_facade);
        ~MainWindow();
        int                       openConfigDialog();
        void                      resizeAsNormalFullScreen();
        void                      resizeAsBigFullScreen();
        void                      resizeAsWindow();
        void                      setMainWindowSize(QSize size);
        QSize                     getMainWindowSize();

    protected:
        const int                 WINDOWED      = 0;
        const int                 FULLSCREEN    = 1;
        const int                 BIGFULLSCREEN = 2;
        qint64                    last_touch = 0;
        qint64                    first_touch = 0;
        int                       count_touch = 0;
        QScopedPointer<Launcher>  MyLauncher;
        QWidget                  *centralWidget;
        LibFacade                *MyLibFacade      = Q_NULLPTR;
        TScreen                  *MyScreen;
        RegionsList              *MyRegionsList;
        QSize                     mainwindow_size;
        int                       screen_state = 0;
        void                      resizeEvent(QResizeEvent * event);
        void                      keyPressEvent(QKeyEvent *ke);
        void                      openDebugInfos();
        void                      openCommandline();
        bool                      event(QEvent *event);
    protected slots:
        void                      prepareParsing();
        void                      takeScreenshot(QString file_path);
        void                      rebootOS(QString task_id);
        void                      installSoftware(QString file_path);
        void                      startShowMedia(BaseMedia *media);
        void                      stopShowMedia(BaseMedia *media);
        void                      resumeShowMedia(BaseMedia *media);
        void                      pauseShowMedia(BaseMedia *media);

};

#endif // MAINWINDOW_H
