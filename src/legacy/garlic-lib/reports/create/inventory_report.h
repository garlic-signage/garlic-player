#ifndef INVENTORY_REPORT_H
#define INVENTORY_REPORT_H

#include "base_reports.h"
#include "db/inventory_table.h"
#include "file_downloader.h"

namespace Reporting
{
    class CreateInventoryReport : public Reporting::CreateBase
    {
            Q_OBJECT
        public:
            CreateInventoryReport(MainConfiguration *config, QObject *parent = nullptr);
            void process(QList<DB::InventoryDataset> list);
        protected:
            QDomElement content_directory, object_info;
            QDomElement createObjectInfo(DB::InventoryDataset dataset);
            QString     getObjectId(QString file_name);
            QString     getState(int state);
            qint64      determineTransferLength(QString file_name, int state);
    };
}

#endif // INVENTORY_REPORT_H
