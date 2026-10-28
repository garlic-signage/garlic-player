#ifndef PLAY_LOGS_MANAGER_H
#define PLAY_LOGS_MANAGER_H

#include "create/play_logs.h"
#include "base_report_manager.h"

namespace Reporting
{
    class PlayLogsManager : public Reporting::BaseReportManager
    {
            Q_OBJECT
        public:
            PlayLogsManager(MainConfiguration *config, SystemInfos::DiscSpace *ds, QObject *parent = nullptr);

        protected:
            QStringList                     send_list;
            QDir                            log_dir;
            QScopedPointer<Reporting::CreatePlayLogs> MyCreatePlayLogs;

            void             handleSend();
            QString          checkForRotate(QString log_file);
            void             send();
            QUrl             generateSendUrl();
         protected slots:
            void             doSucceed(TNetworkAccess *uploader);
            void             doFailed(TNetworkAccess *uploader);
    };
}
#endif // PLAY_LOGS_MANAGER_H
