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
#ifndef WALLCLOCK_H
#define WALLCLOCK_H

#include <QObject>
#include <QDateTime>
#include "iso_date.h"


class WallClock : public QObject
{
        Q_OBJECT
    public:
        explicit  WallClock(QObject *parent = nullptr);
        void      parse(QString iso_date);
        IsoPeriod getPeriod(){return period;}
        int       getRepeats(){return repeats;}
        QDateTime getDateTime(){return trigger_datetime;}
        void      calculateCurrentTrigger(QDateTime current);
        void      calculateNextTrigger(QDateTime current);
        qint64    getPreviousTimerTrigger();
        qint64    getNextTimerTrigger();
    protected:
        int       repeats = 0;
        QDateTime trigger_datetime;
        qint64    next_trigger     = 0;
        qint64    previous_trigger = 0;

        IsoPeriod period;
        int       remaining_repeats = 0;
        void      calculatePreviousTrigger(QDateTime current);
        qint64    calculateWithInfiniteRepeats(QDateTime current);
        qint64    calculateWithRemainingRepeats(QDateTime current);

    private:
        IsoDate MyIsoDate;
        QDateTime addInterval(QDateTime calculated);
        QDateTime substractInterval(QDateTime calculated);
        QDateTime addOptimizations(QDateTime current);

        QDateTime findNextWeekDay(QDateTime calculated);

};

#endif // WALLCLOCK_H
