#ifndef EVENT_H
#define EVENT_H

#include "base_trigger.h"

class Event : public Trigger::BaseTrigger
{
        Q_OBJECT
    public:
        explicit Event(QObject *parent = nullptr);
        bool     parse(QString sync_value);

        QString  getNmToken();

    private:
        QString nm_token = "";
};

#endif // EVENT_H
