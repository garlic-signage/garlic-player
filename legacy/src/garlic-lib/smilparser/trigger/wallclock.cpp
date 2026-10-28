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
#include "wallclock.hpp"

WallClock::WallClock(QObject *parent) : QObject(parent)
{
}

void WallClock::parse(QString iso_date)
{
    QStringList ar;
    if (iso_date.contains("/"))
        ar = iso_date.split("/");

    if (ar.length() > 1)
    {
        QString tmp = ar.at(0);
        if (tmp.length() > 0 && tmp.at(0) == 'R')
           repeats = MyIsoDate.analyseRepeats(tmp);
        else
            trigger_datetime = MyIsoDate.analyseDate(tmp);

        tmp = ar.at(1);
        if (tmp.at(0) != 0 && !trigger_datetime.isValid())
            trigger_datetime = MyIsoDate.analyseDate(tmp);
        else
            period = MyIsoDate.analysePeriods(ar.at(2));

        if (ar.size() == 3)
            period = MyIsoDate.analysePeriods(ar.at(2));
    }
    else
        trigger_datetime = MyIsoDate.analyseDate(iso_date);

    // GitHub issue #31
    if (!MyIsoDate.hasPeriod(period))
       repeats = 0;

    return;
}

qint64 WallClock::getPreviousTimerTrigger()
{
    return previous_trigger;

}

qint64 WallClock::getNextTimerTrigger()
{
    return next_trigger;
}

void WallClock::calculateCurrentTrigger(QDateTime current)
{
    calculateNextTrigger(current);
    calculatePreviousTrigger(current);
}

void WallClock::calculateNextTrigger(QDateTime current)
{
    qint64    ret       = 0;

    if (current > trigger_datetime)  // datetime of wallclock is in past
    {
        if (repeats == -1) // infinite repeats
            ret = calculateWithInfiniteRepeats(current);
        else if (repeats > 0)
            ret = calculateWithRemainingRepeats(current);
        // repeat 0 return 0 ms
    }
    else
        ret = current.msecsTo(trigger_datetime);

    next_trigger = ret;
}

// ================================ protected methods ===============================================

void WallClock::calculatePreviousTrigger(QDateTime current)
{
    // the trigger date is in the future there is no previus trigger
    if (trigger_datetime > current)
         return;

    qint64 ret = 0;

    if (repeats != 0)
    {
        // substract one period from next trigger

        QDateTime prev = substractInterval(current.addMSecs(next_trigger));
        if (prev == current)
            prev = substractInterval(prev);
        ret = -(prev.msecsTo(current));
    }
    else  // we can only be here when trigger was the past so get the difference to current
    {
        ret = -(trigger_datetime.msecsTo(current));
    }
    previous_trigger = ret;
}

qint64 WallClock::calculateWithInfiniteRepeats(QDateTime current)
{
    QDateTime calculate = trigger_datetime;

    calculate = addOptimizations(current); // do some shortcuts cause addInterval slow down on long times
    do
    {
        calculate = addInterval(calculate);
    }
    while (current >= calculate);

    return current.msecsTo(calculate);
}


qint64 WallClock::calculateWithRemainingRepeats(QDateTime current)
{
    QDateTime calculate = trigger_datetime;
    qint64    ret       = 0;
    int       i         = 0;
    remaining_repeats   = 0;
    do
    {
        calculate = addInterval(calculate);
        i++;
    }
    while (current > calculate && i < repeats+1);
    if (current < calculate)
    {
        ret = current.msecsTo(calculate);
        remaining_repeats = repeats - i;
    }
    return ret;
}

QDateTime WallClock::addInterval(QDateTime calculated)
{
    calculated = calculated.addYears(period.years);
    calculated = calculated.addMonths(period.months);
    calculated = calculated.addDays(period.days);
    return calculated.addSecs((period.hours*3600) + (period.minutes*60) + (period.seconds));
}

QDateTime WallClock::substractInterval(QDateTime calculated)
{
    calculated = calculated.addYears(-period.years);
    calculated = calculated.addMonths(-period.months);
    calculated = calculated.addDays(-period.days);
    return calculated.addSecs(-(period.hours*3600) - (period.minutes*60) - (period.seconds));
}

QDateTime WallClock::addOptimizations(QDateTime current)
{
    QDateTime calculate = trigger_datetime;
    calculate = calculate.addYears(current.date().year() - trigger_datetime.date().year() - period.years);
    calculate = calculate.addMonths(current.date().month() - trigger_datetime.date().month() - period.months);
    // at least to get sure, that the calculated date is < than current substract 1 extra day
    // remark: substract 1 hour will not work look at tests
    calculate = calculate.addDays(current.date().day() - trigger_datetime.date().day() - period.days - 1);

    if (MyIsoDate.hasWeekDay())
    {
        calculate = findNextWeekDay(calculate);
    }
    return calculate;
}


QDateTime WallClock::findNextWeekDay(QDateTime calculated)
{
    if (calculated.date().dayOfWeek() == MyIsoDate.getWeekDay())
        return calculated;

    QDate temp = calculated.date();
    temp       = temp.addDays(MyIsoDate.determineDayDiffByOperator('+', MyIsoDate.getWeekDay() - temp.dayOfWeek()));
    return QDateTime(temp, calculated.time());

}
