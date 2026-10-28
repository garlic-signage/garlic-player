#ifndef DOWNLOADFACTORY_H
#define DOWNLOADFACTORY_H

#include <QObject>
#include "main_configuration.hpp"
#include "downloader.h"
namespace Files
{
    class DownloadFactory : public QObject
    {
        Q_OBJECT
    public:
        explicit DownloadFactory(QObject *parent = nullptr);
        static Downloader *createDownloader(MainConfiguration *config, DB::InventoryTable *it, FreeDiscSpace *fds, QObject *parent);

    signals:
    };
}

#endif // DOWNLOADFACTORY_H
