#include "event.h"

Event::Event(QObject *parent) : Trigger::BaseTrigger(parent)
{

}

bool Event::parse(QString sync_value)
{
    has_extern_trigger = false;

    QStringList sl = sync_value.split(".");

    if (sl.length() < 2)
        return false;

    source_id = sl.at(0);

    QString test_token = handlePossibleClockValue(sl.at(1));

    bool ret = true;
    // make sure that there are no hacking jokes
    if (test_token == "beginEvent")
        nm_token = "beginEvent";
    else if (test_token == "endEvent")
        nm_token = "endEvent";
    else if (test_token == "activateEvent")
        nm_token = "activateEvent";
    else if (test_token == "click")
        nm_token = "click";
    else
        ret = false;

    if (ret)
        has_extern_trigger = true;

    return ret;
}

QString Event::getNmToken()
{
    return nm_token;
}

