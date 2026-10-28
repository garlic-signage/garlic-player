#include "base_trigger.h"

Trigger::BaseTrigger::BaseTrigger(QObject *parent) : QObject(parent)
{

}


QString Trigger::BaseTrigger::getSourceId()
{
    return source_id;
}

bool Trigger::BaseTrigger::hasExternTrigger()
{
    return has_extern_trigger;
}

qint64  Trigger::BaseTrigger::getTimeTrigger()
{
    if (MyClockValue == Q_NULLPTR)
        return 0;

    return MyClockValue->getTriggerInMSec();
}

void Trigger::BaseTrigger::setActive(const bool &value)
{
    is_active = value;
}

bool Trigger::BaseTrigger::isActive() const
{
    return is_active;
}

QString Trigger::BaseTrigger::handlePossibleClockValue(QString value)
{
    if (value.contains("+") || value.contains("-"))
       return splitClockValue(value);
    else
        return value;
}

QString Trigger::BaseTrigger::splitClockValue(QString value)
{

    QStringList sl;
    MyClockValue = new ClockValue;
    if (value.contains("+"))
    {
        operant = '+';
        sl = value.split("+");
    }
    else
    {
        operant = '-';
        sl = value.split("-");
    }

    MyClockValue->parse(sl.at(1));

    return sl.at(0);
}
