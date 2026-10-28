#ifndef SYSTEM_REPORT_MANAGER_H
#define SYSTEM_REPORT_MANAGER_H

#include "base_report_manager.h"
#include "create/system_report.h"

namespace Reporting
{
    class SystemReportManager : public Reporting::BaseReportManager
    {
            Q_OBJECT
        public:
            explicit SystemReportManager(MainConfiguration *config, SystemInfos::DiscSpace *ds, QObject *parent = nullptr);
        protected:

            QScopedPointer<Reporting::CreateSystemReport> MyCreateSystemReport;
            void             handleSend();
        protected slots:
           void               doSucceed(TNetworkAccess *uploader);
           void               doFailed(TNetworkAccess *uploader);
       };
}
#endif // SYSTEM_REPORT_MANAGER_H
