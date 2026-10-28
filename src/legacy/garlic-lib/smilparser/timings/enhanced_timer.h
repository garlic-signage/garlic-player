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
#ifndef ENHANCEDTIMER_H
#define ENHANCEDTIMER_H

#include <QObject>
#include <QList>
#include "timings/simple_timer.hpp"
#include "trigger/wallclock.hpp"
#include "trigger/syncbase.h"
#include "trigger/event.h"
#include "trigger/accesskey.hpp"
#include "trigger/notify.h"
#include "tools/logger.h"
namespace Timings
{
    class EnhancedTimer : public QObject
    {
            Q_OBJECT
        public:
            enum Constants
            {
                TYPE_NOT_SET        = 0,
                TYPE_OFFSET         = 1, // same as clock value but depends on type of container https://www.w3.org/TR/REC-smil/smil-timing.html#Timing-OffsetValueSyntax
                TYPE_SYNCBASE       = 2,
                TYPE_EVENT          = 3,
                TYPE_REPEAT         = 4,
                TYPE_ACCESSKEY      = 5,
                TYPE_MEDIAMARKER    = 6,
                TYPE_WALLCLOCK      = 7,
                TYPE_INDEFINITE     = 8,
                TYPE_NOTIFY         = 9
            };


            explicit EnhancedTimer(QObject *parent = nullptr);
            ~EnhancedTimer();
            bool        parse(QString svalue, QString p_tag);
            void        deleteTimer();
            int         determineRemainingSeconds();
            void        activate();
            void        startFromExternalTrigger(QString source_id);
            void        pause();
            void        stop();
            void        resume();
            bool        isActiveTimerTrigger();
            bool        hasExternalTrigger();
            QHash<QString, QString> fetchTriggerList();
        protected:
            struct TriggerStruct
            {
                QTimer     *MyTimer      = Q_NULLPTR;
                ClockValue *MyClockValue = Q_NULLPTR;
                WallClock  *MyWallClock  = Q_NULLPTR;
                SyncBase   *MySyncBase   = Q_NULLPTR;
                Event      *MyEvent      = Q_NULLPTR;
                AccessKey  *MyAccessKey  = Q_NULLPTR;
                Notify     *MyNotify     = Q_NULLPTR;
                QString     id           = "";
                QString     event        = "";
                int         type         = TYPE_NOT_SET;
                int         remaining    = 0;
            };
            QString     attribute = "";
            QString     parent_tag = "";
            qint64      pause_start;
            bool        fire_immediately = false;
            bool        has_external_trigger = false;
            bool        has_negative_offset_trigger = false;
            bool        has_wallclock_next_trigger = false;
            qint64      negative_trigger = 0;
            qint64      positive_trigger = 0;
            bool        parseOneValue(QString value);
            void        activateOffset(TriggerStruct *ts);
            void        activateWallClock(TriggerStruct *ts);
            void        calculateNegativeTrigger(qint64 negative_time);
            void        calculatePositiveTrigger(qint64 positive_time);
            bool        is_indefinite = false;
            QList<TriggerStruct *> MyTriggerList;
            bool initTimer(int type, QString value);

        protected slots:
          void   emitTimeout();

        private:
          void        handleOffsetResume(TriggerStruct *ts);
          void        determineNextTrigger();

        signals:
            void timeout();
    };
}
#endif // ENHANCEDTIMER_H
