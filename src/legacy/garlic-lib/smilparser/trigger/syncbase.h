#ifndef SYNCBASE_H
#define SYNCBASE_H

#include "base_trigger.h"

class SyncBase : public Trigger::BaseTrigger
{
        Q_OBJECT
    public:

        explicit SyncBase(QObject *parent = nullptr);
        bool     parse(QString sync_value);
        QString  getSymbol();

     private:
        QString symbol = "none";

};

#endif // SYNCBASE_H
