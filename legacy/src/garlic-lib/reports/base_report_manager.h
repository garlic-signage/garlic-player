#ifndef BASE_REPORT_MANAGER_H
#define BASE_REPORT_MANAGER_H

#include <QTimerEvent>
#include <QTimer>
#include "files/webdav.h"
#include "smilparser/head/subscription.h"
#include "system_infos/disc_space.hpp"

namespace Reporting
{
    class BaseReportManager : public QObject
    {
            Q_OBJECT
        public:
            explicit BaseReportManager(MainConfiguration *config, SystemInfos::DiscSpace *ds, QObject *parent = nullptr);
            ~BaseReportManager();
            void                 init(QString action, int refresh);
            virtual void         handleSend() = 0;
        protected:
            QScopedPointer<WebDav, QScopedPointerDeleteLater>  MyWebDav;
            SystemInfos::DiscSpace *MyDiscSpace;
            int                     timer_id = 0;
            QString                 action_url = "";
            QString                 current_send_file_path = "";
            SubScription           *MySubscription;
            MainConfiguration      *MyConfiguration;
            void                 timerEvent(QTimerEvent *event);
         protected slots:
            virtual void         doSucceed(TNetworkAccess *uploader) = 0;
            virtual void         doFailed(TNetworkAccess *uploader) = 0;
    };
}

#endif // BASE_REPORT_MANAGER_H
