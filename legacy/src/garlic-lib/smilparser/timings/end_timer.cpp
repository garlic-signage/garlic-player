#include "end_timer.h"

Timings::EndTimer::EndTimer(QObject *parent) : Timings::EnhancedTimer(parent)
{
    attribute = "end";
}


qint64 Timings::EndTimer::getNegativeTrigger()
{
    return negative_trigger;
}

qint64 Timings::EndTimer::getPositiveTrigger()
{
    return positive_trigger;
}

qint64 Timings::EndTimer::hasFireImmediately()
{
    return fire_immediately;
}
