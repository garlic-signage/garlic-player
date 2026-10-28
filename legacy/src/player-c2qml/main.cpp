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
#include <QQmlApplicationEngine>
#include "wrapper_settings.hpp"
#include "qdialog.h"
#include "tools/logger.h"
#include "../player-common/cmdparser.h"
#include "../player-common/screen.h"
#include "../player-common/player_configuration.h"

#if defined  Q_OS_ANDROID
    #include "Java2Cpp.h"
    #include "android_manager.h"
#endif

#ifdef Q_OS_IOS
    #include <QtPlugin>
#endif


#include <QtWebView>
#include "mainwindow.h"
#include "rest_api/httpd.h"

void handleMessages(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Logger& MyLogger = Logger::getInstance();
    MyLogger.dispatchMessages(type, context, msg);
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts); // Raspberry and POT needs this http://thebugfreeblog.blogspot.de/2018/01/pot-570-with-qt-5100-built-for-armv8.html

#ifdef Q_OS_IOS
    Q_IMPORT_PLUGIN(AVFMediaPlayerServicePlugin)
#endif

    QApplication app(argc, argv);
    QtWebView::initialize();

// must be checked and before app create directories
#if defined Q_OS_ANDROID
    AndroidManager *MyAndroidManager = new AndroidManager();
    if (!MyAndroidManager->checkPermissiones())
    {
        MyAndroidManager->sendCloseCorrect();
        app.quit();
        return -1;
    }
    MyAndroidManager->disableScreenSaver();
#endif

    MainConfiguration *MyMainConfiguration   = new MainConfiguration(new WrapperSettings());
    MyMainConfiguration->init();
    MyMainConfiguration->createDirectories();

	qInstallMessageHandler(handleMessages); // must set after createDiretories

    PlayerConfiguration  *MyPlayerConfiguration = new PlayerConfiguration(MyMainConfiguration);

    LibFacade  *MyLibFacade = new LibFacade();

#if defined Q_OS_ANDROID
    if (MyAndroidManager->hasLauncher())
    {
        MyAndroidManager->fetchDeviceInformation();
        // needed for launcher stuff to init or fetch
        QTime dieTime= QTime::currentTime().addSecs(5);
        while (MyAndroidManager->getUUIDFromLauncher().isEmpty() && QTime::currentTime() < dieTime)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        }
        MyLibFacade->toggleLauncher(MyAndroidManager->hasLauncher());
        MyPlayerConfiguration->setHasLauncher(MyAndroidManager->hasLauncher());

        MyPlayerConfiguration->setUuidFromLauncher(MyAndroidManager->getUUIDFromLauncher());

        MyPlayerConfiguration->setSmilIndexUriFromLauncher(MyAndroidManager->getSmilIndexFromLauncher());

        dieTime = QTime::currentTime().addSecs(5);
        while (MyAndroidManager->getLauncherVersion().isEmpty() && QTime::currentTime() < dieTime)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        }
        MyPlayerConfiguration->setVersionFromLauncher(MyAndroidManager->getLauncherVersion());
    }

    setGlobalLibFaceForJava(MyLibFacade);
#endif

    // This must can only be be done after Launcher inits
    MyPlayerConfiguration->determineInitConfigValues();

    MyLibFacade->init(MyMainConfiguration);
    MyPlayerConfiguration->printVersionInformation();

    qmlRegisterType<LibFacade>("com.garlic.LibFacade", 1, 0, "LibFacade");

    TCmdParser MyParser(MyMainConfiguration);
    MyParser.addOptions();

    if (!MyParser.parse(MyLibFacade))
        return 1;
#ifdef QT_DEBUG
    QLoggingCategory::setFilterRules("*.debug=true\nqt.*=false");
#else
    QLoggingCategory::setFilterRules("*.debug=false");
#endif

    TScreen    MyScreen(Q_NULLPTR);
    MyScreen.selectCurrentScreen(MyParser.getScreenSelect());
    // ToDo if init webserver
    QScopedPointer<RestApi::Httpd>             MyHttp;
    MyHttp.reset(new RestApi::Httpd(MyLibFacade));
    MyHttp.data()->init(&app);

    MainWindow w(&MyScreen, MyLibFacade, MyPlayerConfiguration);

    QQmlEngine::setObjectOwnership(&w, QQmlEngine::CppOwnership);

    if (MyMainConfiguration->getIndexUri().isEmpty() && w.openConfigDialog() == QDialog::Rejected)
    {
        return 0;
    }
    w.init();

#if defined Q_OS_ANDROID || defined Q_OS_IOS
    w.showFullScreen();
#else

    w.show();

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
#endif

    return app.exec();
}
