#include "inventory_report.h"

Reporting::CreateInventoryReport::CreateInventoryReport(MainConfiguration *config, QObject *parent) : Reporting::CreateBase(config, parent)
{

}

void Reporting::CreateInventoryReport::process(QList<DB::InventoryDataset> list)
{
    init();
    createSystemInfo();
    content_directory = document.createElement("contentDirectory");
    object_info       = document.createElement("objectInfo");
    player.appendChild(content_directory);
    content_directory.appendChild(object_info);

    for (QList<DB::InventoryDataset>::iterator i = list.begin(); i != list.end(); i++)
    {
        object_info.appendChild(createObjectInfo(*i));
    }

}

QDomElement Reporting::CreateInventoryReport::createObjectInfo(DB::InventoryDataset dataset)
{
    QDomElement object = document.createElement("object");
    object.setAttribute("id", getObjectId(dataset.cache_name));
    object.setAttribute("persistent", "false");

    object.appendChild(createTagWithTextValue("resourceURI", dataset.resource_uri));
    object.appendChild(createTagWithTextValue("state", getState(dataset.state)));

    object.appendChild(createTagWithTextValue("contentType", dataset.content_type));
    object.appendChild(createTagWithTextValue("contentLength", QString::number(dataset.content_length)));
    object.appendChild(createTagWithTextValue("lastModified", dataset.last_update.toString(Qt::ISODate)));
    object.appendChild(createTagWithTextValue("expires", QString()));

    object.appendChild(createTagWithTextValue("transferLength", QString::number(determineTransferLength(dataset.cache_name, dataset.state))));

    return object;
}

QString Reporting::CreateInventoryReport::getObjectId(QString file_name)
{
    QFileInfo fi(file_name);
    return fi.baseName();
}

QString Reporting::CreateInventoryReport::getState(int state)
{
    switch (state)
    {
        case DB::TRANSFER:
            return "COMPLETE";
        case DB::STREAMING:
            return "STREAMING";
        case DB::CHECKSUM:
            return "CHECKSUM";
        case DB::CHECKSUM_ERROR:
            return "CHECKSUM_ERROR";
        case DB::COMPLETE:
            return "COMPLETE";
        default:
            return "ERROR";
     }
}
qint64 Reporting::CreateInventoryReport::determineTransferLength(QString file_name, int state)
{
    QFileInfo fi(MyConfiguration->getPaths("cache") + file_name);
    switch (state)
    {
        case DB::COMPLETE:
            return fi.size();
        case DB::TRANSFER:
             fi.setFile(fi.absoluteFilePath() + FILE_DOWNLOAD_SUFFIX);
             if (fi.exists())
                 return fi.size();
             else
                 return 0;
        default:
            return 0;

    }

}
