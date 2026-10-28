#include "download_factory.h"

Files::DownloadFactory::DownloadFactory(QObject *parent) : QObject(parent)
{

}

Downloader *Files::DownloadFactory::createDownloader(MainConfiguration *config, DB::InventoryTable *it, FreeDiscSpace *fds, QObject *parent)
{
    return new Downloader(fds, it, config, parent);
}
