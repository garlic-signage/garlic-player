#ifndef FILE_DOWNLOADER_H
#define FILE_DOWNLOADER_H

#include <QObject>
#include <QFile>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include "files/network_access.h"


const QString FILE_DOWNLOADED_SUFFIX = ".ready";
const QString FILE_DOWNLOAD_SUFFIX   = ".part";

class FileDownloader : public TNetworkAccess
{
        Q_OBJECT
    public:
        explicit FileDownloader(QNetworkAccessManager* nam, MainConfiguration *config, QObject* parent= nullptr);
        ~FileDownloader();

        void    startDownload(QUrl url, QString file_name, qint64 rs, QString etag);
        void    cancelDownload();
        quint64 getBytesTransfered();
    signals:
        void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void downloadSuccessful();
        void downloadError(QNetworkReply *reply);

    protected:
        QNetworkAccessManager  *network_manager;
        QUrl                    download_url;
        QFile                   destination_file;
        QString                 original_file_name;
        QPointer<QNetworkReply> network_reply;
        quint64                 bytes_transfered = 0;
        qint64                  remote_size;
        void addBytesTransfered(quint64 add_bytes);
        void renameAfterDownload();
        void cleanupDownload();
        void overwriteFile();
    protected slots:
        void readData();
        void finishDownload();

};


#endif // FILE_DOWNLOADER_H
