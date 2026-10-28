#include "syncbase.h"

SyncBase::SyncBase(QObject *parent) : Trigger::BaseTrigger(parent)
{

}

bool SyncBase::parse(QString sync_value)
{
    has_extern_trigger = false;
    QStringList sl = handlePossibleClockValue(sync_value).split(".");
    if (sl.length() < 2)
        return false;

    bool ret = true;
    source_id = sl.at(0);

    // make sure that there is only begin or end and no hacking jokes
    if (sl.at(1) == "begin")
        symbol = "begin";
    else if (sl.at(1) == "end")
        symbol = "end";
    else
        ret = false;

    if (ret)
        has_extern_trigger = true;

    return ret;
}


QString SyncBase::getSymbol()
{
    return symbol;
}


