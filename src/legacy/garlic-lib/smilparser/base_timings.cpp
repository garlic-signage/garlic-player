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

#include "base_timings.h"

BaseTimings::BaseTimings(Expr *expr, QObject * parent) : TBase(parent)
{
    MyExpr = expr;
}

BaseTimings::~BaseTimings()
{
    BeginTimer->stop();
    delete BeginTimer;

    if (DurTimer != Q_NULLPTR)
    {
       DurTimer->stop();
       delete DurTimer;
    }
    if (EndTimer != Q_NULLPTR)
    {
        EndTimer->stop();
        delete EndTimer;
    }
    if (InternalTimer != Q_NULLPTR)
    {
        InternalTimer->stop();
        delete InternalTimer;
    }
}

/**
 * @brief BaseTimings::activate
 *
 * begin and end timer/trigger must be activated first.
 *
 * The begin trigger decides when to send signal for playing
 *
 */
void BaseTimings::activateTriggerTimers()
{
    qDebug() << "activateAllTimers: " + getID() + " parent: " + getParentContainer()->getID();

    // must before start otherwise when begin = 0s
    // elements starts play method immediately get the played status and will be overwriten here
    status       = _pending;
    has_started  = false;
    BeginTimer->activate();

    if (EndTimer != Q_NULLPTR)
    {
        EndTimer->activate();
    }

    if (BeginTimer->mustStartImmediately())
    {
        prepareDurationTimers();
    }
}

int BaseTimings::determineNextEndTime()
{
    int dur = DurTimer->getRemaining();
    int end = EndTimer->determineRemainingSeconds();

    if (dur == 0)
        return end;

    if (dur < end)
        return dur;

    return end;
}

void BaseTimings::pauseAllTimers()
{
    qDebug() << "pauseAllTimers: " + getID() + " parent: " + getParentContainer()->getID();
    BeginTimer->pause();

    if (DurTimer != Q_NULLPTR)
        DurTimer->pause();

    if (EndTimer != Q_NULLPTR)
        EndTimer->pause();
}

void BaseTimings::resumeAllTimers()
{
    qDebug() << "resumeAllTimers: " + getID() + " parent: " + getParentContainer()->getID();

    BeginTimer->resume();

    if (DurTimer != Q_NULLPTR)
        DurTimer->resume();

    if (EndTimer != Q_NULLPTR)
        EndTimer->resume();

    if (is_deferred == true)
    {
        // if it was defered a Begin trigger and prepareDurationTimers
        // where comitted in teh past
        is_deferred = false;
        prepareDurationTimers();
    }

}

void BaseTimings::selectWhichTimerShouldStop()
{
    if (isBeginTimerActive())
    {
        status = _pending;
        stopSimpleTimers();
    }
    else
    {
        stopAllTimers();
    }
}

void BaseTimings::stopAllTimers()
{
    qDebug() << "stopAllTimers: " + getID() + " parent: " + getParentContainer()->getID();
    status = _stopped;

    BeginTimer->stop();

    if (EndTimer != Q_NULLPTR)
        EndTimer->stop();

    stopSimpleTimers();

}

/**
 * @brief BaseTimings::stopSimpleTimers is used when you want to stop element, but not the triggers
 *
 * The happens when there is an begin-value-list or a wallclock with repeat periods
 * for example a stop from an higher priorityClass
 *
 * In this pending situation stopping begin/end triggers will prevent element from restart
 *
 * That is also the reason why Begin/End-Trigger fired to the listeners here
 */
void BaseTimings::stopSimpleTimers()
{
    qDebug() << "stopSimpleTimer: " + getID() + " parent: " + getParentContainer()->getID();
    if (DurTimer != Q_NULLPTR)
        DurTimer->stop();

    // fire begin trigger to all elements which are listening for the ending of this element
    QStringList sl = {};
    if (BeginTargets != Q_NULLPTR)
    {
        sl = BeginTargets->findListenerIDsByTriggerList({"end", "endEvent"});
        sendTriggerSignalToListeners(sl, "begin");
    }
    // fire end trigger to all elements which are listening for the ending of this element
    if (EndTargets != Q_NULLPTR)  // fire begin trigger to all elements which are listening
    {
        sl = EndTargets->findListenerIDsByTriggerList({"end", "endEvent"});
        sendTriggerSignalToListeners(sl, "end");
    }
}

void BaseTimings::startTrigger(QString source_id)
{
    qDebug() << "prepare trigger Play from : " + source_id + " and " + getID() + " parent: " + getParentContainer()->getID();
    status = _pending;
    BeginTimer->startFromExternalTrigger(source_id);
    if (EndTimer != Q_NULLPTR)
    {
        EndTimer->activate();
    }
}

void BaseTimings::stopTrigger(QString source_id)
{
    if (status == _stopped)
        return;

    if (EndTimer != Q_NULLPTR)
       EndTimer->startFromExternalTrigger(source_id);
    else
       finishedActiveDuration();
}

bool BaseTimings::hasActiveTimers()
{
    if (isEndTimerActive())
        return true;

    if (isDurTimerActive())
        return true;

    if (isBeginTimerActive())
        return true;

    return false;
}

void BaseTimings::addToExternalBegins(QString symbol, QString target_id)
{
    if (BeginTargets == Q_NULLPTR)
        BeginTargets = new TargetTrigger(this);

    BeginTargets->insert(symbol, target_id);
}

void BaseTimings::addToExternalEnds(QString symbol, QString target_id)
{
    if (EndTargets == Q_NULLPTR)
        EndTargets = new TargetTrigger(this);

    EndTargets->insert(symbol, target_id);
}

QHash<QString, QString>  BaseTimings::fetchExternalBegins()
{
    if (!BeginTimer->hasExternalTrigger())
        return {};

   return BeginTimer->fetchTriggerList();
}

QHash<QString, QString> BaseTimings::fetchExternalEnds()
{
    if (EndTimer == Q_NULLPTR || (EndTimer != Q_NULLPTR && !EndTimer->hasExternalTrigger()))
        return {};

   return EndTimer->fetchTriggerList();
}

void BaseTimings::setDefered()
{
    status = _defered;
}


void BaseTimings::finishedNotFound()
{
    // to fix https://github.com/sagiadinos/garlic-player/issues/12
    // when a lonely element (e.g video without dur) in a repeatCount=indefinite playlist not found,
    // a finishedActiveDuration will cause segmentation fault (heap crash) after some some hundreds runs
    // so we need to wait some milliseconds when timer is set.
    if (!BeginTimer->isActiveTimerTrigger() && isEndTimerActive() && isDurTimerActive())
    {
        skipElement();
    }
}

/**
 * @brief TBaseTiming::skipElement
 * this is needed to prevent a recursion stack overflow when download is not complete
 */
void BaseTimings::skipElement()
{
    if (InternalTimer == Q_NULLPTR)
    {
        InternalTimer = new QTimer(this);
        connect(InternalTimer, SIGNAL(timeout()), this, SLOT(finishedActiveDuration()));
        InternalTimer->setSingleShot(true);
        InternalTimer->setTimerType(Qt::PreciseTimer);
    }
    InternalTimer->start(100);
}

void BaseTimings::emitActivated()
{
    QStringList sl = {};
    if (BeginTargets != Q_NULLPTR)
    {
        sl = BeginTargets->findListenerIDsByTriggerList({"activateEvent", "click"});
        sendTriggerSignalToListeners(sl, "begin");
    }
    if (EndTargets != Q_NULLPTR)
    {
        sl = EndTargets->findListenerIDsByTriggerList({"activateEvent", "click"});
        sendTriggerSignalToListeners(sl, "end");
    }
}

void BaseTimings::emitStartElementSignal(BaseTimings *bt)
{
    emit startElementSignal(bt);
}

void BaseTimings::emitStopElementSignal(BaseTimings *bt, bool has_prio)
{
    emit stopElementSignal(bt, has_prio);
}

void BaseTimings::emitResumeElementSignal(BaseTimings *bt)
{
    emit resumeElementSignal(bt);
}

void BaseTimings::emitPauseElementSignal(BaseTimings *bt)
{
    emit pauseElementSignal(bt);
}

// ========================= protected methods ======================================================

void BaseTimings::parseTimingAttributes()
{
    setBaseAttributes();

    handleBeginTimer();

    if (root_element.hasAttribute("end"))
    {
        EndTimer = new Timings::EndTimer(this);
        if (EndTimer->parse(root_element.attribute("end"), parent()->objectName()))
        {
            connect(EndTimer, SIGNAL(timeout()), this, SLOT(finishedActiveDuration()));
        }
        else
        {
            delete EndTimer;
            EndTimer = Q_NULLPTR;
        }
        BeginTimer->setEndTimer(EndTimer);
    }

    if (root_element.hasAttribute("dur"))
    {
        DurTimer = new Timings::SimpleTimer(this);
        connect(DurTimer, SIGNAL(timeout()), this, SLOT(finishedSimpleDuration()));
        DurTimer->parse(root_element.attribute("dur"));
        BeginTimer->setDurTimer(DurTimer);
    }

    if (root_element.hasAttribute("repeatCount"))
    {
        setRepeatCount(root_element.attribute("repeatCount"));
    }
    if (root_element.hasAttribute("restart"))
    {
        setRestart(root_element.attribute("restart"));
    }
}

void BaseTimings::resetInternalRepeatCount()
{
    internal_count = repeatCount;
}

bool BaseTimings::startDurTimer()
{
    QStringList sl = {};
    if (BeginTargets != Q_NULLPTR)
    {
        sl = BeginTargets->findListenerIDsByTriggerList({"begin", "beginEvent"});
        sendTriggerSignalToListeners(sl, "begin");
    }
    if (EndTargets != Q_NULLPTR)
    {
        sl = EndTargets->findListenerIDsByTriggerList({"begin", "beginEvent"});
        sendTriggerSignalToListeners(sl, "end");
    }

    if (DurTimer == Q_NULLPTR)
        return false;

    // needed when a begin trigger ist started in the past
    // DurTime reduction can only happens on the first playlist run

    // So we are able to determine if past begin trigger is currently active
    // when a simple duration (dur-attribute) is last long enough
    DurTimer->recalculateTimeClock(BeginTimer->getElapsedButActiveTimeTrigger());
    return DurTimer->start();
}

void BaseTimings::sendTriggerSignalToListeners(QStringList listener, QString trigger_signal)
{
    for (const auto& target_id : qAsConst(listener))
    {
        emit triggerSignal(trigger_signal, target_id, getID());
    }
}

bool BaseTimings::handleRepeatCountStatus()
{
    if (indefinite)
        return true;

    bool ret = false;
    // > 1 because the first pass time was already
    if (internal_count > 1)
    {
        internal_count--;
        ret = true;
    }
    return ret;
}

bool BaseTimings::isBeginTimerActive()
{
    return (BeginTimer->isActiveTimerTrigger() && isRestartable());
}

bool BaseTimings::isEndTimerActive()
{
    if (EndTimer == Q_NULLPTR)
        return false;
    else
        return EndTimer->isActiveTimerTrigger();
}

bool BaseTimings::hasDurTimer()
{
    if (DurTimer == Q_NULLPTR)
        return false;
    else
        return true;
}

bool BaseTimings::hasEndTimer()
{
    if (EndTimer == Q_NULLPTR)
        return false;
    else
        return true;
}

bool BaseTimings::isDurTimerActive()
{
    if (DurTimer == Q_NULLPTR)
        return false;
    else
        return DurTimer->isActive();
}

bool BaseTimings::isRestartable()
{
    switch (restart)
    {
        case WHEN_NOT_ACTIVE:
            if (status != _active)
                return true;
            else
                return false;
        case NEVER:
             // important to prevent any kind of restart in this lifecycle
            // end this element without check for pending
             return false;
        case ALWAYS:
        default:
            return true;
    }
}

/**
 * The intrinsic Duration can be also the implicit duration
 * of a seq, par, or excl time container
 * https://www.w3.org/TR/SMIL3/smil-timing.html#q69
 *
 * If we need more differentation one day we should add an
 * finishImplicitDuration method. At the moment only dur is supported
 *
 */
void BaseTimings::finishIntrinsicDuration()
{
    if (isDurTimerActive())
        return;

    qDebug() << getID() <<  " end intrinsic duration";
    finishedSimpleDuration();
}

void BaseTimings::finishedSimpleDuration()
{
    // ignore repeatCount if begintimer has a wallclock repeatinterval
    // repeatCount and repeatDur extends simple duration
    if (!isBeginTimerActive() && handleRepeatCountStatus())
    {
        // ToDo: activate probably repeat event trigger here
        qDebug() << getID() <<  "repeat by repeatCount";
        repeat();
        return;
    }

    qDebug() << getID() <<  " end simple duration";

    if (isEndTimerActive())
    {
        status = _pending;
        emitfinishedElement();
        return;
    }

    finishedActiveDuration();
}

void BaseTimings::finishedActiveDuration()
{
    status = _pending;
    qDebug() << getID() <<  " end active duration";
    emitfinishedElement();
}

// ========================= private methods ======================================================

void BaseTimings::setRepeatCount(QString rC)
{
    if (rC == "indefinite")
        indefinite = true;
    else
       indefinite = false;
    repeatCount = rC.toInt();
}

void BaseTimings::handleBeginTimer()
{
    BeginTimer = new Timings::BeginTimer(this);
    QString begin_value = "";

    if (parent()->objectName() == "TExcl")
        begin_value = getAttributeFromRootElement("begin", "indefinite");
    else
        begin_value = getAttributeFromRootElement("begin", "0s");

    // in case begin_value is not parsable force default values
    // we need poarent cause seq cannot have wallclocks
    if (!BeginTimer->parse(begin_value, parent()->objectName()))
    {
        delete BeginTimer;
        BeginTimer = new Timings::BeginTimer(this);
        if (parent()->objectName() == "TExcl")
            BeginTimer->parse("indefinite", "");
        else
            BeginTimer->parse("0s", "");
    }

    connect(BeginTimer, SIGNAL(timeout()), this, SLOT(prepareDurationTimers()));
}

void BaseTimings::setRestart(QString attr_value)
{
    if (attr_value == "never")
        restart = NEVER;
    else if (attr_value == "always")
        restart = ALWAYS;
    else
        restart = WHEN_NOT_ACTIVE;
}
