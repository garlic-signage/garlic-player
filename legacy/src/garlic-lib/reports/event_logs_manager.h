#ifndef EVENT_LOGS_MANAGER_H
#define EVENT_LOGS_MANAGER_H

#include "create/event_logs.h"
#include "base_report_manager.h"

namespace Reporting
{
    class EventLogsManager : public Reporting::BaseReportManager
    {
            Q_OBJECT
        public:
            explicit EventLogsManager(MainConfiguration *config, SystemInfos::DiscSpace *ds, QObject *parent = nullptr);

       protected:
           QStringList                     send_list;
           QDir                            log_dir;
           QScopedPointer<Reporting::CreateEventLogs> MyCreateEventLogs;

           void             handleSend();
           QString          checkForRotate(QString log_file);
           void             send();
           QUrl             generateSendUrl();
        protected slots:
           void               doSucceed(TNetworkAccess *uploader);
           void               doFailed(TNetworkAccess *uploader);
     };
}
#endif // EVENT_LOGS_MANAGER_H
