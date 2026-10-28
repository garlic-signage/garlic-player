#ifndef NOTIFY_H
#define NOTIFY_H

#include "base_trigger.h"

class Notify : public Trigger::BaseTrigger
{
        Q_OBJECT
    public:
        explicit Notify(QObject *parent = nullptr);
        bool     parse(QString value);
        QString  getSymbol();


    private:
        QString symbol;
        QString determineNotification(QString value);

    signals:

};

#endif // NOTIFY_H
