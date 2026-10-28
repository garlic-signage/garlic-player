#include "notify.h"

Notify::Notify(QObject *parent) : Trigger::BaseTrigger(parent)
{

}

bool Notify::parse(QString value)
{
    QString cleared    = handlePossibleClockValue(value);
    cleared            = determineNotification(cleared);

    has_extern_trigger = false;

    if (cleared.length() < 1)
        return false;

    source_id          =  "notify";
    symbol             = cleared;
    has_extern_trigger = true;
    return true;
}


QString Notify::getSymbol()
{
    return symbol;
}

QString Notify::determineNotification(QString value)
{
    // notify
    return value.mid(7, value.length()-8);
}

