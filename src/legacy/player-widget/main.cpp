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

#include <QString>
#include "wrapper_settings.hpp"
#include "tools/logger.h"
#include "mainwindow.h"
#include "../player-common/cmdparser.h"
#include "../player-common/screen.h"
#include "../player-common/player_configuration.h"
#include "rest_api/httpd.h"

void handleMessages(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Logger& MyLogger = Logger::getInstance();
    MyLogger.dispatchMessages(type, context, msg);
}

int main(int argc, char *argv[])
{
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--allow-file-access-from-files --disable-web-security");
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_X11InitThreads);

    QApplication app(argc, argv);
    MainConfiguration *MyMainConfiguration = new MainConfiguration(new WrapperSettings());
    MyMainConfiguration->init();
    MyMainConfiguration->createDirectories();

    qInstallMessageHandler(handleMessages); // must be after create Directories
    PlayerConfiguration  *MyPlayerConfiguration = new PlayerConfiguration(MyMainConfiguration);
    MyPlayerConfiguration->determineInitConfigValues();

    LibFacade      *MyLibFacade     = new LibFacade();
    MyLibFacade->init(MyMainConfiguration);
    MyPlayerConfiguration->printVersionInformation();

#ifdef QT_DEBUG
 //   QLoggingCategory::setFilterRules("*.debug=true\nqt.*=false");
    // QtAV::setLogLevel(QtAV::LogAll); // use with caution! produces crashes in debug.log
#else
    QLoggingCategory::setFilterRules("*.debug=false");
#endif

    TCmdParser MyParser(MyMainConfiguration);
    MyParser.addOptions();
    MyParser.parse(MyLibFacade);

    TScreen    MyScreen(Q_NULLPTR);
    MyScreen.selectCurrentScreen(MyParser.getScreenSelect());
    // ToDo if init webserver
    QScopedPointer<RestApi::Httpd>             MyHttp;
    MyHttp.reset(new RestApi::Httpd(MyLibFacade));
    MyHttp.data()->init(&app);
    MainWindow w(&MyScreen, MyLibFacade);

    // do not start without an index uri
    if (!MyPlayerConfiguration->hasLauncher() && MyMainConfiguration->getIndexUri() == "" && w.openConfigDialog() == QDialog::Rejected)
    {
        return 1;
    }

     QString val = MyParser.getWindowMode();
    if (val == "fullscreen")
        w.resizeAsNormalFullScreen();
    else if (val == "bigscreen")
        w.resizeAsBigFullScreen();
    else if (val == "windowed")
    {
        w.setMainWindowSize(MyParser.getWindowSize());
        w.resizeAsWindow();
    }

    MyLibFacade->initParser();

    return app.exec();
}
