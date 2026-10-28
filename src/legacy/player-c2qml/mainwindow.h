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

#include <QQuickView>
#include <QMap>

#include "regions_list.h"
#include "../player-common/screen.h"
#include "../player-common/launcher.h"
#include "../player-common/player_configuration.h"
#include "lib_facade.h"

class MainWindow : public QQuickView
{
    Q_OBJECT
    public:
        MainWindow(TScreen *screen, LibFacade *lib_facade, PlayerConfiguration *pc);
        ~MainWindow();
        void                      init();
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
        QScopedPointer<Launcher>  MyLauncher;
        LibFacade                *MyLibFacade           = Q_NULLPTR;
        PlayerConfiguration      *MyPlayerConfiguration = Q_NULLPTR;
        TScreen                  *MyScreen;
        static LibFacade         *MyStaticLibFacade;
        RegionsList              *MyRegionsList;
        QSize                     mainwindow_size;
        int                       screen_state = 0;
        qint64                    last_touch = 0;
        int                       count_touch = 0;
        void                      openDebugInfos();
        void                      resizeEvent(QResizeEvent * event);
        void                      keyPressEvent(QKeyEvent *ke);
        bool                      event(QEvent *event);

        void                      sendClosePlayerCorrect();
        void                      quitApplication();
    protected slots:
        void                      prepareParsing();
        void                      startShowMedia(BaseMedia *media);
        void                      stopShowMedia(BaseMedia *media);
        void                      resumeShowMedia(BaseMedia *media);
        void                      pauseShowMedia(BaseMedia *media);
        void                      doStatusChanged(QQuickView::Status status);
        void                      takeScreenshot(QString file_path);
        void                      rebootOS(QString task_id);
        void                      installSoftware(QString file_path);
};

#endif // MAINWINDOW_H
