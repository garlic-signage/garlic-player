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
#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QObject>
#include "file_downloader.h"
#include "tools/logger.h"
#include "files/free_disc_space.h"

/**
 * @brief The Downloader class capsulates Download accesses
 * - handles head, get and redirect requests
 * - emit a signal with QIODevioce for download when succeeded
 * - logs success (info) and error notifications
 */
class Downloader : public TNetworkAccess
{
    Q_OBJECT
    public:

        explicit Downloader(FreeDiscSpace *fds, DB::InventoryTable *it, MainConfiguration *config, QObject *parent);
        ~Downloader();

        void       processFile(QUrl url, QFileInfo fi);

        //Getter/Setter
        QFileInfo  getLocalFileInfo();
        void       setLocalFileInfo(const QFileInfo &value) {local_file_info = value;}
        bool       isRequestInProgress() const;

    protected:
        QFileInfo               local_file_info;
        qint64                  remote_size;
        QString                 content_type;
        QByteArray              remoteEtag;
        DB::InventoryTable      *MyInventoryTable = Q_NULLPTR;
        FreeDiscSpace           *MyFreeDiscSpace = Q_NULLPTR;
        DB::InventoryDataset    currentDataset;
        QScopedPointer <QNetworkAccessManager>  manager_head, manager_head_redirect, manager_get;
        QScopedPointer <FileDownloader>         MyFileDownloader;
        void                    checkStatusCode(QNetworkReply *reply, int status_code);
        void                    checkHttpHeaders(QNetworkReply *reply);
        void                    prepareDownload(QNetworkReply *reply);
        void                    startDownload(QNetworkReply *reply);
        bool                    validContentType(QString content_type);
        bool                    validFileExtension();
        void                    handleNetworkError(QNetworkReply *reply);
        QUrl                    examineRedirectUrl(QUrl redirect_url);
        quint64                 determineBytesTransfered();
        bool                    canStoreNewFile();
    protected slots:
        void                    finishedHeadRequest(QNetworkReply *reply);
        void                    finishedHeadRedirectRequest(QNetworkReply *reply);
        void                    doDownloadSuccessFul();
        void                    doDownloadError(QNetworkReply *reply);
        void                    insertDatabase();
    private:
        bool                    is_request_in_progress = false;
        void                    handleDifferentServer();
        QString                 determineLocalFileMd5();
        void                    emitNotModified(QNetworkReply *reply);
    signals:
        void                    notmodified(TNetworkAccess *);
        void                    notcacheable(TNetworkAccess *);
};

#endif // DOWNLOADER_H
