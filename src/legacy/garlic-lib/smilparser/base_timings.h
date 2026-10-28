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

#ifndef TBASETIMING_H
#define TBASETIMING_H

#include <QTimer>
#include "smilparser/base.hpp"
#include "smilparser/timings/begin_timer.h"
#include "smilparser/timings/end_timer.h"
#include "smilparser/trigger/target_trigger.h"
#include "smilparser/conditional/expr.hpp"

/**
 * @brief The abstract BaseTimings class should inherited for begin end duration capable smil elements in body section
 *        Full support as in http://www.w3.org/TR/SMIL3/ described for:
 *        dur          Clock Value in ms, media indefinite
 *        repeatCount  indefinite, numeric value)
 *
 *        Partial support Clock Value in ms, media indefinite and wallcock with repeating periods
 *        begin, end
 *
 */
class BaseTimings : public TBase
{
        Q_OBJECT
    public:
        enum RESTART
        {
            ALWAYS             = 0,
            NEVER              = 1,
            WHEN_NOT_ACTIVE    = 2,
            DEFAULT            = 3
        };
        const     int        _stopped  = 0;
        const     int        _pending  = 1; // wait for first start
        const     int        _active   = 2;
        const     int        _paused   = 3;
        const     int        _defered  = 4;

        explicit               BaseTimings(Expr *expr, QObject * parent);
                              ~BaseTimings();

                void           activateTriggerTimers();
                int            determineNextEndTime();
                void           pauseAllTimers();
                void           selectWhichTimerShouldStop();
                void           stopAllTimers();
                void           stopSimpleTimers();
                void           startTrigger(QString source_id);
                void           stopTrigger(QString source_id);
                void           resumeAllTimers();
                bool           hasActiveTimers();
                bool           isBeginTimerActive();
                bool           isEndTimerActive();
                void           addToExternalBegins(QString symbol, QString target_id);
                void           addToExternalEnds(QString symbol, QString target_id);
                QHash<QString, QString> fetchExternalBegins();
                QHash<QString, QString> fetchExternalEnds();

                void           setDefered();
                int            getStatus(){return status;}
                bool           isActive(){return (status == _active);}
                void           finishedNotFound();
                void           skipElement();
                void           emitActivated();

        virtual void           start()                = 0;
        virtual void           repeat()               = 0;
        virtual void           stop(bool is_forced)   = 0;
        virtual void           interruptByRestart()   = 0;
        virtual void           pause()       = 0;
        virtual void           resume()      = 0;
        virtual QString        getBaseType() = 0;

        virtual void           emitPause()   = 0;
        virtual void           emitResume() = 0;

                void           emitStartElementSignal(BaseTimings* bt);
                void           emitStopElementSignal(BaseTimings* bt, bool has_prio);
                void           emitResumeElementSignal(BaseTimings* bt);
                void           emitPauseElementSignal(BaseTimings* bt);


                void           finishIntrinsicDuration();
                bool           has_started  = false;
                bool           is_deferred  = false;
    public slots:
        virtual void           emitfinishedElement() = 0;
        virtual void           prepareDurationTimers() = 0; // slot fro begin-Timer
                void           finishedSimpleDuration();
    protected:
                QTimer        *InternalTimer = Q_NULLPTR;
                int            restart = ALWAYS;
                QString        begin   = "0";
                QString        dur     = "media";
                QString        end     = "indefinite";
                QString        fill    = "remove";     // stub ToDo
                QString        min     = "0";          // stub ToDo
                QString        max     = "indefinite"; // stub ToDo
                Expr          *MyExpr;

                int            status         = _stopped;
                int            repeatCount    = 0;
                bool           indefinite     = false;
                int            internal_count = 1;
                void           sendTriggerSignalToListeners(QStringList listener, QString trigger_signal);
                void           parseTimingAttributes();
                void           resetInternalRepeatCount();
                bool           startDurTimer();
                bool           handleRepeatCountStatus();
                bool           hasDurTimer();
                bool           hasEndTimer();
                bool           isDurTimerActive();
                bool           isRestartable();
    protected slots:
                void           finishedActiveDuration();
    private:
                Timings::BeginTimer    *BeginTimer   = Q_NULLPTR;
                Timings::EndTimer      *EndTimer     = Q_NULLPTR;
                Timings::SimpleTimer   *DurTimer     = Q_NULLPTR;

                TargetTrigger          *BeginTargets = Q_NULLPTR;
                TargetTrigger          *EndTargets   = Q_NULLPTR;
           //     Timings::SimpleTimer   *repeatDur  = Q_NULLPTR;
                void           setRepeatCount(QString rC);
                void           handleBeginTimer();
                void           setRestart(QString attr_value);
    signals:
                void  triggerSignal(QString,QString,QString); // signal(begin, end, activateEvent), target_id, source_id)
                void  startElementSignal(BaseTimings*);
                void  stopElementSignal(BaseTimings*,bool);
                void  resumeElementSignal(BaseTimings*);
                void  pauseElementSignal(BaseTimings*);

};

#endif // TBASETIMING_H
