#ifndef EVENT_LOGS_H
#define EVENT_LOGS_H

#include "base_reports.h"

namespace Reporting
{
    class CreateEventLogs : public Reporting::CreateBase
    {
            Q_OBJECT
        public:
            CreateEventLogs(MainConfiguration *config, QObject *parent);
            void process(QString file_name);
        protected:
            void mergeXml(QString file_name);

    };
}
#endif // EVENT_LOGS_H
