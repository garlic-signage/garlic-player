#ifndef INVENTORY_REPORT_MANAGER_H
#define INVENTORY_REPORT_MANAGER_H

#include "create/inventory_report.h"
#include "base_report_manager.h"

namespace Reporting
{
    class InventoryReportManager : public Reporting::BaseReportManager
    {
            Q_OBJECT
        public:
            InventoryReportManager(MainConfiguration *config, DB::InventoryTable *inv_table, SystemInfos::DiscSpace *ds, QObject *parent = nullptr);

        protected:
            DB::InventoryTable *MyInventoryTable = Q_NULLPTR;
            QScopedPointer<Reporting::CreateInventoryReport> MyCreateInventoryReport;
            void             handleSend();
         protected slots:
            void               doSucceed(TNetworkAccess *uploader);
            void               doFailed(TNetworkAccess *uploader);
    };
}

#endif // INVENTORY_REPORT_MANAGER_H
