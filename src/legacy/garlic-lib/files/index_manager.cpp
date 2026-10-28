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
#include "index_manager.h"

Files::IndexManager::IndexManager(DB::InventoryTable *it, MainConfiguration *config, FreeDiscSpace *fds, QObject *parent) : BaseManager(config, fds, parent)
{
    MyDownloader    = new Downloader(fds, it, MyConfiguration, this);
    MyIndexModel    = new IndexModel(this);
    connect(MyDownloader, SIGNAL(succeed(TNetworkAccess*)), SLOT(doSucceed(TNetworkAccess*)));
    connect(MyDownloader, SIGNAL(notmodified(TNetworkAccess*)), SLOT(doNotModified(TNetworkAccess*)));
}

void Files::IndexManager::init(QString src)
{
    src_index_path = src;
}

void Files::IndexManager::lookUpForUpdatedIndex()
{
    if (src_index_path == "")
    {
        qWarning(Develop) << "set index path first";
        return;
    }
    if (isRemote(src_index_path))
    {
        MyDownloader->processFile(src_index_path, MyConfiguration->getPaths("cache")+"index.smil");
        // First Start surely do not have a LastPlayedIndexPath so set this here
        // especially needed to ensure cache play first, when switching between local and remote indexes
        MyConfiguration->setLastPlayedIndexPath(MyConfiguration->getPaths("cache")+"index.smil");
    }
    else if (MyConfiguration->getLastPlayedIndexPath() != src_index_path)
    {
        MyConfiguration->setLastPlayedIndexPath(src_index_path);
        emit readyForLoading();
    }
}

int Files::IndexManager::getError() const
{
    return error;
}

bool Files::IndexManager::isIndexInDownload() const
{
    return MyDownloader->isRequestInProgress();
}


bool Files::IndexManager::load()
{
    if (src_index_path == "")
    {
        qWarning(Develop) << "set index path first";
        is_loaded = false;
        return is_loaded;
    }

    if (isRemote(src_index_path))
        is_loaded = loadLocal(MyConfiguration->getLastPlayedIndexPath());
    else
        is_loaded = loadLocal(src_index_path);

    return is_loaded;

}

void Files::IndexManager::activateRefresh(int value)
{
    if (value == 0) // set not the timer, when no e.g. on index with no refresh time
        return;

    refresh_time   = value;
    timer_id       = startTimer(refresh_time*1000);
}

void Files::IndexManager::deactivateRefresh()
{
    if (timer_id == 0)
        return;

    refresh_time   = 0;
    killTimer(timer_id);
}

bool Files::IndexManager::exists()
{
   return QFile::exists(MyConfiguration->getLastPlayedIndexPath());
}

QDomElement Files::IndexManager::getSmil()
{
    return MyIndexModel->getSmil();
}

QDomElement Files::IndexManager::getHead()
{
    return MyIndexModel->getHead();
}

QDomElement Files::IndexManager::getBody()
{
    return MyIndexModel->getBody();
}

// ==================  protected methods =======================================

bool Files::IndexManager::loadLocal(QString local_path)
{
    renameDownloadedFile(local_path);
    if (!QFile::exists(local_path))
    {
        QStringList list;
        list << "instanceId" << "display:0" << "resourceURI" << src_index_path;
        qInfo(ContentManager) << Logger::getInstance().createEventLogMetaData("SMIL_NOT_AVAILABLE", list);
        return false;
    }

    if (!MyIndexModel->loadDocument(local_path))
    {
        error = INDEX_CORRUPT;
        return false;
    }
    return true;
}

void Files::IndexManager::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == timer_id)
    {
        lookUpForUpdatedIndex();
    }
}


// ==================  protected slots =======================================

void Files::IndexManager::doSucceed(TNetworkAccess *downloader)
{
    Q_UNUSED(downloader); // This class have one permenent downloader instance so function paramter not used

    emit readyForLoading();
}

void Files::IndexManager::doNotModified(TNetworkAccess *downloader)
{
    Q_UNUSED(downloader); // This class have one permenent downloader instance so function paramter not used
    if (!is_loaded)
        emit readyForLoading();

}
