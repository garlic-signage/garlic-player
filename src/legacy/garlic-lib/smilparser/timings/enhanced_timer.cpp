/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2021 Nikolaos Sagiadinos <ns@smil-control.com>
    This file is part of the garlic-player source code

    This program is free software: you can redistribute it and/or  modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************/
#include "enhanced_timer.h"
#include <cmath>

Timings::EnhancedTimer::EnhancedTimer(QObject *parent) : QObject(parent)
{
}

Timings::EnhancedTimer::~EnhancedTimer()
{
    deleteTimer();
}

bool Timings::EnhancedTimer::initTimer(int type, QString value)
{
    TriggerStruct *ts = new TriggerStruct();
    ts->type        = type;
    ts->remaining   = 0;

    ts->MyTimer     = new QTimer(this);
    ts->MyTimer->setTimerType(Qt::PreciseTimer);
    ts->MyTimer->setSingleShot(true);
    connect(ts->MyTimer, SIGNAL(timeout()), this, SLOT(emitTimeout()));
    bool ret = false;
    switch (ts->type)
    {
        case TYPE_INDEFINITE:
            is_indefinite = true;
            ret = true;
        break;
        case TYPE_OFFSET:
            ts->MyClockValue = new ClockValue(this);
            ts->MyClockValue->parse((value));
            ret = true; // we assume offset always return at least 0
        break;
        case TYPE_WALLCLOCK:
            ts->MyWallClock = new WallClock(this);
            ts->MyWallClock->parse(value);
            ret = true; // we assume always return at least 2000-01-01 00:00:00
        break;
        case TYPE_ACCESSKEY:
            ts->MyAccessKey = new AccessKey(this);
            ret = ts->MyAccessKey->parse(value);
            if (!ret)
            {
                delete ts->MyAccessKey;
                ts->MyAccessKey = Q_NULLPTR;
            }
            if (ret && ts->MyAccessKey->hasExternTrigger())
                has_external_trigger = true;
        break;
        case TYPE_NOTIFY:
            ts->MyNotify = new Notify(this);
            ret = ts->MyNotify->parse(value);
            if (!ret)
            {
                delete ts->MyNotify;
                ts->MyNotify = Q_NULLPTR;
            }
            if (ret && ts->MyNotify->hasExternTrigger())
                has_external_trigger = true;
        break;
        case TYPE_SYNCBASE:
            ts->MySyncBase = new SyncBase(this);
            ret = ts->MySyncBase->parse(value);

            if (!ret)
            {
                delete ts->MySyncBase;
                ts->MySyncBase = Q_NULLPTR;
            }

            if (ret && ts->MySyncBase->hasExternTrigger())
                has_external_trigger = true;
        break;
        case TYPE_EVENT:
            ts->MyEvent = new Event(this);
            ret = ts->MyEvent->parse(value);

            if (!ret)
            {
                delete ts->MyEvent;
                ts->MyEvent = Q_NULLPTR;
            }

            if (ret && ts->MyEvent->hasExternTrigger())
                has_external_trigger = true;
        break;
    }

    // add only when trigger item is valid
    if (ret)
        MyTriggerList.append(ts);
    else
        delete ts;
    return ret;
}

void Timings::EnhancedTimer::deleteTimer()
{
    if (MyTriggerList.size() == 0)
        return;
    for (TriggerStruct *ts : qAsConst(MyTriggerList))
    {
        ts->MyTimer->stop();
        switch (ts->type)
        {
            case TYPE_OFFSET:
                delete ts->MyClockValue;
            break;
            case TYPE_WALLCLOCK:
                delete ts->MyWallClock;
            break;
            case TYPE_ACCESSKEY:
                if (ts->MyAccessKey != Q_NULLPTR)
                    delete ts->MyAccessKey;
            break;
            case TYPE_NOTIFY:
                if (ts->MyNotify != Q_NULLPTR)
                    delete ts->MyNotify;
            break;
            case TYPE_SYNCBASE:
                if (ts->MySyncBase != Q_NULLPTR)
                    delete ts->MySyncBase;
            break;
            case TYPE_EVENT:
                if (ts->MyEvent != Q_NULLPTR)
                    delete ts->MyEvent;
            break;
        }
        delete ts->MyTimer;
        delete ts;
    }
    MyTriggerList.clear();
}

int Timings::EnhancedTimer::determineRemainingSeconds()
{
    int ret = 0;

    if (MyTriggerList.size() == 0)
        return ret;

    for (TriggerStruct *ts : qAsConst(MyTriggerList))
    {
        if ((ts->type == TYPE_OFFSET || ts->type == TYPE_WALLCLOCK) && ts->MyTimer != Q_NULLPTR && ts->MyTimer->isActive())
        {

            if (ret == 0 || ret > ts->MyTimer->remainingTime())
                ret = ts->MyTimer->remainingTime();
        }
    }

    return std::floor(ret / 1000);
}

bool Timings::EnhancedTimer::parse(QString attr_value, QString p_tag)
{
    parent_tag = p_tag;
    // do not toLower here, we need origial values e.g. for ISO 8601
    QStringList value_list = attr_value.simplified().split(';');

    // seq do not support value-lists get the first value only;
    if (parent_tag == "TSeq")
        return parseOneValue(value_list.first());


    // if there is one parsable item in the list the the bigin/end trigger is valid
    // otherwise trigger is invalid an would be handlet in BaseTimings-Class
    bool        is_parsable = false;
    for (const auto& value : qAsConst(value_list))
    {
        if (value.isEmpty()) // some might end with semikolon
            continue;

        is_parsable = parseOneValue(value);
    }

    if (value_list.size() > 1) // in a value an indefinite can be ignored
        is_indefinite = false;

    return is_parsable;
}

void Timings::EnhancedTimer::activate()
{
    if (MyTriggerList.size() == 0)
        return;
    for (TriggerStruct *ts : qAsConst(MyTriggerList))
    {
        switch (ts->type)
        {
            case TYPE_OFFSET:
                  activateOffset(ts);
            break;
            case TYPE_WALLCLOCK:
                  activateWallClock(ts);
                break;
            case TYPE_ACCESSKEY:
                ts->MyAccessKey->setActive(true);
            break;
            case TYPE_NOTIFY:
                ts->MyNotify->setActive(true);
            break;
            case TYPE_SYNCBASE:
                ts->MySyncBase->setActive(true); // rest is done by Timings::EnhancedTimer::startFromExternalTrigger
            break;
            case TYPE_EVENT:
                ts->MyEvent->setActive(true); // rest is done by Timings::EnhancedTimer::startFromExternalTrigger
            break;
        }
    }
}

void Timings::EnhancedTimer::startFromExternalTrigger(QString source_id)
{
    if (MyTriggerList.size() == 0)
        return;
    qint64 next_trigger;
    for (TriggerStruct *ts : qAsConst(MyTriggerList))
    {
        switch (ts->type)
        {
            case TYPE_ACCESSKEY: // source_id == accesskey
                if (ts->MyAccessKey->getSourceId() == source_id)
                {
                    next_trigger = ts->MyAccessKey->getTimeTrigger();
                    if (next_trigger == 0)
                        emitTimeout();
                    else
                       ts->MyTimer->start(next_trigger);
                 }
            break;
            case TYPE_NOTIFY: // source_id == notify
                if (ts->MyNotify->getSourceId() == source_id)
                {
                    next_trigger = ts->MyNotify->getTimeTrigger();
                    if (next_trigger == 0)
                        emitTimeout();
                    else
                       ts->MyTimer->start(next_trigger);
                 }
            break;
            case TYPE_SYNCBASE:
                if (ts->MySyncBase->getSourceId() == source_id)
                {
                    next_trigger = ts->MySyncBase->getTimeTrigger();
                    if (next_trigger == 0)
                        emitTimeout();
                    else
                       ts->MyTimer->start(next_trigger);
                }
            break;
            case TYPE_EVENT:
                if (ts->MyEvent->getSourceId() == source_id)
                {
                    next_trigger = ts->MyEvent->getTimeTrigger();
                    if (next_trigger == 0)
                        emitTimeout();
                    else
                       ts->MyTimer->start(next_trigger);
                }
            break;
        }
    }
}

void Timings::EnhancedTimer::stop()
{
    if (MyTriggerList.size() == 0)
        return;

    for (TriggerStruct *ts : qAsConst(MyTriggerList))
    {
        switch (ts->type)
        {
            case TYPE_OFFSET:
            case TYPE_WALLCLOCK:
                ts->MyTimer->stop();
                ts->remaining = 0;
            break;
            case TYPE_ACCESSKEY:
                ts->MyAccessKey->setActive(false);
            break;
            case TYPE_NOTIFY:
                ts->MyNotify->setActive(false);
            break;
            case TYPE_SYNCBASE:
                ts->MySyncBase->setActive(false);
            break;
            case TYPE_EVENT:
                ts->MyEvent->setActive(false);
            break;
        }
    }
}

void Timings::EnhancedTimer::pause()
{
    if (MyTriggerList.size() == 0)
        return;
    for (TriggerStruct *ts : qAsConst(MyTriggerList))
    {
        switch (ts->type)
        {
            case TYPE_OFFSET:
                ts->remaining = ts->MyTimer->remainingTime();
            break;
            case TYPE_WALLCLOCK:
            break;
        }
        if (ts->MyTimer != Q_NULLPTR)
            ts->MyTimer->stop();
    }
    pause_start = QDateTime::currentMSecsSinceEpoch();
}

void Timings::EnhancedTimer::resume()
{
    if (MyTriggerList.size() == 0)
        return;

    for (TriggerStruct *ts : qAsConst(MyTriggerList))
    {
        switch (ts->type)
        {
            case TYPE_OFFSET:
                handleOffsetResume(ts);
            break;
            case TYPE_WALLCLOCK:
                activateWallClock(ts); // wallclock depends on concrete times
            break;
        }
    }
}

/**
 * @brief Timings::EnhancedTimer::handleOffsetResume
 * @param remaining
 *
 * pause in enhancedTimer do not stop counting
 *
 */
void Timings::EnhancedTimer::handleOffsetResume(TriggerStruct *ts)
{
     // <= 0 means that a begin/end trigger had already fired
    // proceeding will cause an unneccessary timeout with side effects
    if (ts->remaining <= 0)
        return;

    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - pause_start;
    qint64 rest    = 0;

    rest = ts->remaining - elapsed;

    if (rest > 0)
        ts->MyTimer->start(rest);
    else
        emitTimeout();
}


bool Timings::EnhancedTimer::isActiveTimerTrigger()
{
  // it is useless to check for non time trigger,
  // because they do not affect the lifetime of an element

    if (MyTriggerList.size() == 0)
        return false;

    for (TriggerStruct *ts : qAsConst(MyTriggerList))
    {
        switch (ts->type)
        {
            case TYPE_OFFSET:
                if (ts->MyTimer->isActive())
                    return true;
            break;
            case TYPE_WALLCLOCK:
                if (ts->MyTimer->isActive())
                    return true;
            break;
            case TYPE_INDEFINITE:
                return true;
        }
    }
    return false;
}

bool Timings::EnhancedTimer::hasExternalTrigger()
{
    return has_external_trigger;
}

QHash<QString, QString> Timings::EnhancedTimer::fetchTriggerList()
{
    QHash<QString, QString> ret;
    if (MyTriggerList.size() == 0)
        return ret;

    for (TriggerStruct *ts : qAsConst(MyTriggerList))
    {
        switch (ts->type)
        {
            case TYPE_SYNCBASE:
                if (ts->MySyncBase->hasExternTrigger())
                    ret.insert(ts->MySyncBase->getSourceId(), ts->MySyncBase->getSymbol());
            break;
            case TYPE_ACCESSKEY:
                if (ts->MyAccessKey->hasExternTrigger())
                    ret.insert("accesskey", ts->MyAccessKey->getCharSymbol());
            break;
            case TYPE_NOTIFY:
                if (ts->MyNotify->hasExternTrigger())
                    ret.insert("notify", ts->MyNotify->getSymbol());
            break;
            case TYPE_EVENT:
                if (ts->MyEvent->hasExternTrigger())
                    ret.insert(ts->MyEvent->getSourceId(), ts->MyEvent->getNmToken());
            break;
        }
    }
    return ret;
}

bool Timings::EnhancedTimer::parseOneValue(QString value)
{
    bool is_parsable = false;
    if (value.contains("indefinite"))
    {
        initTimer(TYPE_INDEFINITE, "indefinite");
        is_parsable = true;
    }
    else if (value.mid(0, 9) == "wallclock" && parent_tag != "TSeq")
    {
        initTimer(TYPE_WALLCLOCK, value.mid(10,value.length()-11));
        is_parsable = true;
    }
    else if (value.at(0) == '+' || value.at(0) == '-')
    {
        initTimer(TYPE_OFFSET, value);
        is_parsable = true;
    }
    else if (value.at(0).isDigit())
    {
        initTimer(TYPE_OFFSET, value);
        is_parsable = true;
    }
    else if (value.mid(0, 9) == "accesskey" )
    {
        is_parsable = initTimer(TYPE_ACCESSKEY, value);
    }
    else if (value.mid(0, 6) == "notify" )
    {
        is_parsable = initTimer(TYPE_NOTIFY, value);
    }
    else if (value.at(0).isLetter() && value.contains("."))
    {
        QString check;
        // seperate the the +/i
        if (value.contains("+"))
            check = value.split("+").at(0);
        else if (value.contains("-"))
            check = value.split("-").at(0);
        else
             check = value;

        if (check.endsWith("begin") || check.endsWith("end"))
            is_parsable = initTimer(TYPE_SYNCBASE, value);
        else
            is_parsable = initTimer(TYPE_EVENT, value);
    }
    else
    {
    }

   return is_parsable;
}

void Timings::EnhancedTimer::activateOffset(Timings::EnhancedTimer::TriggerStruct *ts)
{
    qint64 next_trigger = ts->MyClockValue->getTriggerInMSec();

    if (next_trigger == 0)
    {
        fire_immediately = true;
    }
    else if (next_trigger < 0)
    {
        has_negative_offset_trigger = true;
        calculateNegativeTrigger(next_trigger);
    }
    else
       ts->MyTimer->start(next_trigger);
}

void Timings::EnhancedTimer::activateWallClock(Timings::EnhancedTimer::TriggerStruct *ts)
{
    // we need to calculate a previous trigger, cause it can be possible that an
    // element triggrered active in the past and last on  du to long or indefinite (active) duration

    QDateTime dt = QDateTime::currentDateTime();
    ts->MyWallClock->calculateCurrentTrigger(dt);
    qint64 previous_trigger = ts->MyWallClock->getPreviousTimerTrigger();
    qint64 next_trigger     = ts->MyWallClock->getNextTimerTrigger();

    if (next_trigger == 0)
    {
        fire_immediately = true;
    }
    else if (next_trigger > 0)
    {
        has_wallclock_next_trigger = true;
        ts->MyTimer->start(next_trigger);
    }

    if (previous_trigger < 0)
        calculateNegativeTrigger(previous_trigger);
    else
        calculatePositiveTrigger(next_trigger);
}

void Timings::EnhancedTimer::calculateNegativeTrigger(qint64 negative_time)
{
    // we must find the negative trigger which is nearest to zero
    if (negative_trigger == 0 || negative_trigger < negative_time)
        negative_trigger = negative_time;
}

void Timings::EnhancedTimer::calculatePositiveTrigger(qint64 positive_time)
{
    // we must find the positive trigger which is nearest to zero
    if (positive_trigger == 0 || positive_time < positive_trigger)
        positive_trigger = positive_time;
}

void Timings::EnhancedTimer::emitTimeout()
{
    determineNextTrigger();
    emit timeout();
}

void Timings::EnhancedTimer::determineNextTrigger()
{
    if (MyTriggerList.size() == 0)
        return;

    for (TriggerStruct *ts : qAsConst(MyTriggerList))
    {
        if (ts->type == TYPE_WALLCLOCK && ts->MyTimer != Q_NULLPTR && !ts->MyTimer->isActive())
        {
            QDateTime dt = QDateTime::currentDateTime();
            ts->MyWallClock->calculateNextTrigger(dt);
            qint64 next_trigger     = ts->MyWallClock->getNextTimerTrigger();
            if (next_trigger > 0) // not sure if this has sie effects, but it prevents an endless loop when next_trigger ==  0
                ts->MyTimer->start(next_trigger);
        }
    }
}
