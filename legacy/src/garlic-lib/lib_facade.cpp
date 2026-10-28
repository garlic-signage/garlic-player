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
#include "lib_facade.h"

LibFacade::LibFacade(QObject *parent) : QObject(parent)
{
    MyDiscSpace.reset(new SystemInfos::DiscSpace(&MyStorage));
    MyResourceMonitor.setDiscSpace(MyDiscSpace.data());
    MyFreeDiscSpace.reset(new FreeDiscSpace(MyDiscSpace.data()));

    QNetworkProxyFactory::setUseSystemConfiguration(true);
    QNetworkProxyQuery npq(QUrl(QLatin1String("http://www.google.com")));
    QList<QNetworkProxy> listOfProxies = QNetworkProxyFactory::systemProxyForQuery(npq);
    foreach (QNetworkProxy p, listOfProxies)
       qDebug() << "hostname" << p.hostName();

    connect(&RebootTimer, SIGNAL(reboot(QString)), this, SLOT(reboot(QString)));
}

LibFacade::~LibFacade()
{
    killTimer(resource_monitor_timer_id);
}

/**
 * This is needed because qmlRegisterType in player-c2qml did not work
 * with constructor params except QObject
 * So we need to make the dependency injections here
 *
 * @brief LibFacade::init
 * @param config
 */
void LibFacade::init(MainConfiguration *config)
{    
    MyConfiguration.reset(config);
    MyInventoryTable.reset(new DB::InventoryTable(this));
    MyInventoryTable.data()->init(MyConfiguration.data()->getPaths("logs"));
    MyDiscSpace.data()->init(MyConfiguration.data()->getPaths("cache"));
    MyFreeDiscSpace.data()->init(MyConfiguration.data()->getPaths("cache"));
    MyFreeDiscSpace.data()->setInventoryTable(MyInventoryTable.data());

    MyIndexManager.reset(new Files::IndexManager(MyInventoryTable.data(), MyConfiguration.data(), MyFreeDiscSpace.data(), this));
    connect(MyIndexManager.data(), SIGNAL(readyForLoading()), this, SLOT(loadIndex()));

    resource_monitor_timer_id = startTimer(300000); // every 300s for ressource monitor

    MyTaskScheduler.reset(new SmilHead::TaskScheduler(MyInventoryTable.data(), MyConfiguration.data(), MyFreeDiscSpace.data(), this));
    connect(MyTaskScheduler.data(), SIGNAL(applyConfiguration()), this, SLOT(changeConfig()));
    connect(MyTaskScheduler.data(), SIGNAL(installSoftware(QString)), this, SLOT(emitInstallSoftware(QString)));
    connect(MyTaskScheduler.data(), SIGNAL(reboot(QString)), this, SLOT(reboot(QString)));
    connect(MyTaskScheduler.data(), SIGNAL(applyCommand(QString,QString)), this, SLOT(applyCommand(QString,QString)));

}

ResourceMonitor *LibFacade::getResourceMonitor()
{
    return &MyResourceMonitor;
}

void LibFacade::shutDownParsing()
{
    MyBodyParser.data()->endPlayingBody();
    MyIndexManager.data()->deactivateRefresh();
}

void LibFacade::initParser()
{
    MyIndexManager.data()->init(MyConfiguration.data()->getIndexUri());
    MyIndexManager.data()->lookUpForUpdatedIndex();

    // load index from cache, because if remote check fails
    // or has a long timeout player will be show a white screen
    loadIndex();
}

void LibFacade::initParserWithTemporaryFile(QString uri)
{
    MyIndexManager.data()->init(uri);
    MyIndexManager.data()->lookUpForUpdatedIndex();
   // loadIndex();
}

void LibFacade::setConfigFromExternal(QString config_path, bool restart_smil_parsing)
{
    MyXMLConfiguration.reset(new SmilHead::XMLConfiguration(MyInventoryTable.data(), MyConfiguration.data(), MyFreeDiscSpace.data(), this));
    if (restart_smil_parsing)
        connect(MyXMLConfiguration.data(), SIGNAL(finishedConfiguration()), this, SLOT(reboot()));
    MyXMLConfiguration.data()->processFromLocalFile(config_path);
}

void LibFacade::transferNotify(QString key)
{
    if (MyElementsContainer.isNull())
        return;

    MyBodyParser.data()->triggerNotify(key);
}

void LibFacade::transferAccessKey(QChar key)
{
    if (MyElementsContainer.isNull())
        return;

    MyBodyParser.data()->triggerAccessKey(key);
}

/**
 * When new index/content_url came from an external source like a launcher at runtime
 *
 * @brief LibFacade::reloadWithNewIndex
 * @param index_path
 */
void LibFacade::reloadWithNewIndex(QString index_path)
{
    MyConfiguration->determineIndexUri(index_path);
    initParser();
}

void LibFacade::beginSmilPlaying()
{
    MyBodyParser.data()->startPresentationAfterPreload();
}

QString LibFacade::requestLoaddableMediaPath(QString path)
{
    return MyMediaManager.data()->requestLoadablePath(path);
}

void LibFacade::loadIndex()
{
    // validate
    MyIndexManager.data()->init(MyConfiguration.data()->getIndexUri());
    if (!MyBodyParser.isNull())
    {
        MyBodyParser.data()->endPlayingBody();
        MyIndexManager.data()->deactivateRefresh();
    }

    // Start with this only when it is absolutly sure that in the player component is no activity anymore.
    if (!MyIndexManager.data()->load())
    {
        // Why we are here?
        // index in cache? => wait
        // index is corrupt and there is no ongoing download process for a new index? =>Y reboot
        if (MyIndexManager.data()->getError() == Files::IndexManager::INDEX_CORRUPT && !MyIndexManager.data()->isIndexInDownload())
            reboot("index_broken");

        return;
    }

    MyPlaceHolder.reset(new SmilHead::PlaceHolder(this)); // must init before Filemanager

    initFileManager();
    processHeadParsing();
    configureRebootTimer();
}

void LibFacade::changeConfig()
{
    loadIndex();
}

void LibFacade::initFileManager()
{
    MyMediaModel.reset(new MediaModel(MyFreeDiscSpace.data(), this));
    MyDownloadQueue.reset(new DownloadQueue(MyConfiguration.data(), MyFreeDiscSpace.data(), MyInventoryTable.data(), this));
    MyMediaManager.reset(new Files::MediaManager(MyMediaModel.data(), MyDownloadQueue.data(), MyConfiguration.data(), MyFreeDiscSpace.data(), this));
    qDebug() <<  " end initFileManager" ;
}

void LibFacade::processHeadParsing()
{
    MyHeadParser.reset(new HeadParser(MyConfiguration.data(), MyMediaManager.data(), MyInventoryTable.data(), MyPlaceHolder.data(), MyDiscSpace.data(), this));
    connect(MyHeadParser.data(), SIGNAL(parsingCompleted()), this, SLOT(processBodyParsing()));

    qDebug() <<  " begin head parsing" ;
    MyHeadParser.data()->parse(MyIndexManager->getHead(), MyTaskScheduler.data());
    qDebug() <<  " end head parsing" ;
}

void LibFacade::processBodyParsing()
{
    MyExpr.reset(new Expr);
    MyElementsContainer.reset(new ElementsContainer(this)); // must be setted, when Layout is known
    MyElementFactory.reset(new ElementFactory(MyMediaManager.data(), MyConfiguration.data(), MyPlaceHolder.data(), MyExpr.data()));

    MyBodyParser.reset(new BodyParser(MyElementFactory.data(), MyMediaManager.data(), MyElementsContainer.data(), this));

    connect(MyBodyParser.data(), SIGNAL(startShowMedia(BaseMedia*)), this, SLOT(emitStartShowMedia(BaseMedia*)));
    connect(MyBodyParser.data(), SIGNAL(stopShowMedia(BaseMedia*)), this, SLOT(emitStopShowMedia(BaseMedia*)));
    connect(MyBodyParser.data(), SIGNAL(resumeShowMedia(BaseMedia*)), this, SLOT(emitResumeShowMedia(BaseMedia*)));
    connect(MyBodyParser.data(), SIGNAL(pauseShowMedia(BaseMedia*)), this, SLOT(emitPauseShowMedia(BaseMedia*)));

    qDebug() <<  " begin preloading" ;
    MySmil.reset(new Smil(this));
    MySmil.data()->preloadParse(MyIndexManager->getSmil());

    MyBodyParser->beginPreloading(MyExpr.data(), MySmil.data(), MyIndexManager->getBody());
    qDebug() <<  " end preloading" ;

    MyIndexManager.data()->activateRefresh(MyHeadParser->getRefreshTime());
    emit readyForPlaying();
}

void LibFacade::configureRebootTimer()
{
    RebootScheduler.reset(new Scheduler(MyConfiguration.data(), &MyWeekDayConverter));
    RebootScheduler.data()->determineNextReboot(QDateTime::currentDateTime());
    RebootTimer.stopTimer();
    if (RebootScheduler.data()->getNextDatetime().isValid())
    {
        RebootTimer.setRebootTime(RebootScheduler.data()->getNextDatetimeInMSecs());
    }
}

void LibFacade::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    MyResourceMonitor.refresh();

    qInfo(Develop) << MyResourceMonitor.getTotalDiscSpace() << MyResourceMonitor.getFreeDiscSpace();
    qInfo(Develop) << MyResourceMonitor.getTotalMemorySystem() << MyResourceMonitor.getFreeMemorySystem();
    qInfo(Develop) << MyResourceMonitor.getMemoryAppUse() << MyResourceMonitor.getMaxMemoryAppUsed();
    qInfo(Develop) << MyResourceMonitor.getThreadsNumber() << MyResourceMonitor.getMaxThreadsNumber();
}

void LibFacade::emitInstallSoftware(QString file_path)
{
    emit installSoftware(file_path);
}

void LibFacade::reboot(QString task_id)
{
    emit rebootOS(task_id);
}

void LibFacade::applyCommand(QString task_id, QString command)
{
    Q_UNUSED(task_id);
    if (command == "clear_all_caches")
    {
        MyFreeDiscSpace->clearPlayerCache();
        MyFreeDiscSpace->clearWebCache();
    }
    else if (command == "clear_playercache" || command == "clear_cache")
    {
        MyFreeDiscSpace->clearPlayerCache();
    }
    else if (command == "clear_webcache")
    {
        MyFreeDiscSpace->clearWebCache();
    }
    reboot(task_id); // otherwise a black screen
}

void LibFacade::takeScreenshot(QString file_path)
{
    emit screenshot(file_path);
}

void LibFacade::emitStartShowMedia(BaseMedia *media)
{
    emit startShowMedia(media);
    qDebug() << "emitStartShowMedia " << media->getID();
}

void LibFacade::emitStopShowMedia(BaseMedia *media)
{
    emit stopShowMedia(media);
    qDebug() << "emitStopShowMedia " << media->getID();
}

void LibFacade::emitResumeShowMedia(BaseMedia *media)
{
    emit resumeShowMedia(media);
    qDebug() << "emitResumeShowMedia " << media->getID();
}

void LibFacade::emitPauseShowMedia(BaseMedia *media)
{
    emit pauseShowMedia(media);
    qDebug() << "emitPauseShowMedia " << media->getID();
}
