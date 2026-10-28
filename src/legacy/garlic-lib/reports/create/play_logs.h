#ifndef PLAY_LOGS_H
#define PLAY_LOGS_H

#include "base_reports.h"

namespace Reporting
{
    class CreatePlayLogs : public Reporting::CreateBase
    {
        Q_OBJECT
        public:
            CreatePlayLogs(MainConfiguration *config, QObject *parent);
            void process(QString file_name);
        protected:
            void mergeXml(QString file_name);

    };
}
#endif // PLAY_LOGS_H
