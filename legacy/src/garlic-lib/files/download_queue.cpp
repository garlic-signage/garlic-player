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

#include "download_queue.h"

DownloadQueue::DownloadQueue(MainConfiguration *config, FreeDiscSpace *fds, DB::InventoryTable *it, QObject *parent) : QObject(parent)
{
    MyInventoryTable= it;
    MyFreeDiscSpace = fds;
    MyConfiguration = config;
}

DownloadQueue::~DownloadQueue()
{
    clearQueues();
}

void DownloadQueue::clearQueues()
{
    media_queue.clear();
    QHash<QString, Downloader *>::iterator i;
    for (i = download_slots.begin(); i != download_slots.end(); i++)
    {
        i.value()->deleteLater(); // delete will crash
    }
    download_slots.clear();
}

void DownloadQueue::insertQueue(QString src, QString local)
{
    media_queue.enqueue(qMakePair(src, local));
    processQueue();
}

void DownloadQueue::processQueue()
{
    if (download_slots.size() < _max_download_slots && media_queue.size() > 0)
    {
        QPair<QString, QString> paths = media_queue.dequeue();

        // queue only unique values, otherways we get trouble with overwriting files in file_downloader.cpp!
        // have in mind, that there could be a also prefetch
        // or the same file more than one time in playlist
        if (download_slots.find(paths.second) == download_slots.end())
        {
            Downloader *MyDownloader = new Downloader(MyFreeDiscSpace, MyInventoryTable, MyConfiguration, this);

            connect(MyDownloader, SIGNAL(succeed(TNetworkAccess*)), SLOT(doSucceed(TNetworkAccess*)));
            connect(MyDownloader, SIGNAL(notcacheable(TNetworkAccess*)), SLOT(doNotCacheable(TNetworkAccess*)));
            connect(MyDownloader, SIGNAL(notmodified(TNetworkAccess*)), SLOT(doNotModified(TNetworkAccess*)));
            connect(MyDownloader, SIGNAL(failed(TNetworkAccess*)), SLOT(doFailed(TNetworkAccess*)));

            download_slots.insert(paths.second, MyDownloader);
            MyDownloader->processFile(QUrl(paths.first), QFileInfo(paths.second));
        }
    }
}

void DownloadQueue::doSucceed(TNetworkAccess *downloader)
{
    Downloader *MyDownloader = qobject_cast<Downloader *> (downloader);
    emit succeed(MyDownloader->getRemoteFileUrl().toString(), MyDownloader->getLocalFileInfo().absoluteFilePath());
    download_slots.remove(MyDownloader->getLocalFileInfo().absoluteFilePath());
    MyDownloader->deleteLater();
    processQueue();
}

void DownloadQueue::doFailed(TNetworkAccess *downloader)
{
    Downloader *MyDownloader = qobject_cast<Downloader *> (downloader);
    emit failed(MyDownloader->getRemoteFileUrl().toString());
    download_slots.remove(MyDownloader->getLocalFileInfo().absoluteFilePath());
    MyDownloader->deleteLater();
    processQueue();
}

void DownloadQueue::doNotCacheable(TNetworkAccess *downloader)
{
    Downloader *MyDownloader = qobject_cast<Downloader *> (downloader);
    emit notcacheable(MyDownloader->getRemoteFileUrl().toString());
    download_slots.remove(MyDownloader->getLocalFileInfo().absoluteFilePath());
    MyDownloader->deleteLater();
    processQueue();
}

void DownloadQueue::doNotModified(TNetworkAccess *downloader)
{
    Downloader *MyDownloader = qobject_cast<Downloader *> (downloader);
    emit notmodified(MyDownloader->getRemoteFileUrl().toString());
    download_slots.remove(MyDownloader->getLocalFileInfo().absoluteFilePath());
    MyDownloader->deleteLater();
    processQueue();
}
