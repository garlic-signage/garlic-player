#include "begin_timer.h"

Timings::BeginTimer::BeginTimer(QObject *parent) : Timings::EnhancedTimer(parent)
{
    attribute = "begin";
}

qint64 Timings::BeginTimer::getElapsedButActiveTimeTrigger()
{
    qint64 temp = negative_trigger;
    resetNegativeTimeTrigger();
    return temp;
}

void Timings::BeginTimer::setDurTimer(Timings::SimpleTimer *dur)
{
    DurTimer = dur;
}

void Timings::BeginTimer::setEndTimer(Timings::EndTimer *end)
{
    EndTimer = end;
}

bool Timings::BeginTimer::mustStartImmediately()
{
    // when something like begin="0s" a maybe dur = 0s¨ will be recognised in the prepareDurationTimerBeforePlay()
    // allowFireImmediately checks if there is an EndTimer which maybe has an immediatelyStart, too
    // which means no start!
    if (fire_immediately)
    {
        bool ret = allowFireImmediately();
        if (!ret)
            resetNegativeTimeTrigger();
        return ret;
    }
    // this can mean indefinite, an external trigger or a timed trigger in future
    // without a timed trigger in the past we have to wait!
    if (negative_trigger == 0)
        return false;


    // if there is end attribute the dur attribute decides if start immediately
    if (EndTimer != Q_NULLPTR)
    {
        // if a previous Begin is before a previous End sure not start
        qint64 end_negative = EndTimer->getNegativeTrigger();
        bool ret = (negative_trigger > end_negative);
        resetNegativeTimeTrigger();
        return ret;
    }

    return allowByDurTimer();
}

void Timings::BeginTimer::resetNegativeTimeTrigger()
{
    // negative_trigger must be set to 0 when false or used one time
    // otherwise it could mess up with a probably dur value
    negative_trigger = 0;
}

bool Timings::BeginTimer::allowFireImmediately()
{
    if (EndTimer != Q_NULLPTR && EndTimer->hasFireImmediately())
        return false;

    return true;
}

bool Timings::BeginTimer::allowByDurTimer()
{
    bool ret = false;
    if (DurTimer != Q_NULLPTR)
    {
        int type = DurTimer->getType();
        switch (type)
        {
            case SimpleTimer::TYPE_INDEFINITE:
            case SimpleTimer::TYPE_MEDIA:
                ret = true;
            break;
            case SimpleTimer::TYPE_CLOCKVALUE:
                if (negative_trigger < 0)
                    ret = (negative_trigger + DurTimer->getParsedClock() > 0);
                if (!ret)
                    resetNegativeTimeTrigger();
            break;
        }
    }
    return ret;
}

