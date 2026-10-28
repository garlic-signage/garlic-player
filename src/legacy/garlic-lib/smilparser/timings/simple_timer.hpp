/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2016 Nikolaos Saghiadinos <ns@smil-control.com>
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

#ifndef TSIMPLETIMER_H
#define TSIMPLETIMER_H

#include <QObject>
#include <QTimer>
#include <QDateTime>

#include "clock_value.hpp"

namespace Timings
{
    class SimpleTimer : public QObject
    {
        Q_OBJECT
    public:
        enum Constants
        {
            TYPE_NONE        = 0,
            TYPE_CLOCKVALUE  = 1,
            TYPE_INDEFINITE  = 2,
            TYPE_MEDIA       = 3,
        };

        // need 10ms tolerances to handle timer latencies
        const     int        tolerance           = 10; // in milli seconds

        explicit SimpleTimer(QObject *parent = nullptr);
        ~SimpleTimer();

        void         parse(QString s_value);
        bool         isActive();
        bool         start();
        void         pause();
        void         resume();
        void         stop();
        qint64       getParsedClock();
        qint64       getClockInMs();
        int          getType(){return type;}
        void         recalculateTimeClock(qint64 negative_trigger);
        int          getRemaining() const;

    protected:
        QTimer      *MyTimer = Q_NULLPTR;
        ClockValue   MyClockValue;
        qint64       clock_in_ms;
        int          remaining = 0;
        int          type = 3;
        void         initTimer();
        void         deleteTimer();
    protected slots:
        void         emitTimeout();

    signals:
        void timeout();
    };
}
#endif // TTIMER_H
