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

#include "container.h"

TContainer::TContainer(Expr *expr, QObject *parent) : BaseTimings(expr, parent)
{
}

BaseTimings *TContainer::findChildByDom(QDomElement dom_element)
{
    BaseTimings                             *MyBaseTimings     = Q_NULLPTR;
    QHash<QString, BaseTimings *>::iterator  elements_iterator = childs_list.find(TBase::parseID(dom_element));
    if (elements_iterator != childs_list.end())
    {
        MyBaseTimings = *elements_iterator;
    }
    return MyBaseTimings;
}

void TContainer::appendChild(BaseTimings *MyBaseTimings)
{
    childs_list.insert(MyBaseTimings->getID(), MyBaseTimings);
}

void TContainer::interruptByRestart()
{
    stopAllActivatedChilds(false);
    removeAllActiveChilds();
}

void TContainer::removeActiveChild(BaseTimings *element)
{
    qDebug() << "remove: " + element->getID() + " parent: " + element->getParentContainer()->getID();
    if (activated_childs.indexOf(element) >= 0)
        activated_childs.removeOne(element);
}

void TContainer::removeAllActiveChilds()
{
    activated_childs.clear();
}

void TContainer::startFirstActivatedChild()
{
    status = _active;
    if (activated_childs.size() == 0)
        return;

    BaseTimings *bt = activated_childs.first();
    bt->activateTriggerTimers();
}

void TContainer::pauseFirstActivatedChild()
{
    if (activated_childs.size() == 0)
        return;

    BaseTimings *bt = activated_childs.first();
    bt->pauseAllTimers();
    emitPauseElementSignal(bt);
}

void TContainer::resumeFirstActivatedChild()
{
    status = _active;
    if (activated_childs.size() == 0)
        return;

    BaseTimings *bt = activated_childs.first();
    bt->resumeAllTimers();
    emitResumeElementSignal(bt);
}

void TContainer::startAllActiveChilds()
{
    status      = _active;
    if (activated_childs.size() == 0)
        return;

    for ( BaseTimings *bt : qAsConst(activated_childs))
    {
        if (bt->getStatus() == _stopped)
            bt->activateTriggerTimers();
    }
}

void TContainer::stopAllActivatedChilds(bool is_forced)
{
    if (activated_childs.size() == 0)
        return;

    for (BaseTimings *bt : qAsConst(activated_childs))
    {
        bt->stopAllTimers();
        emitStopElementSignal(bt, is_forced);
    }
}

void TContainer::pauseAllActivatedChilds()
{
    if (activated_childs.size() == 0)
        return;

    for (BaseTimings *bt : qAsConst(activated_childs))
    {
        bt->pauseAllTimers();
        emitPauseElementSignal(bt);
    }
}

void TContainer::resumeAllActivatedChilds()
{
    status      = _active;
    if (activated_childs.size() == 0)
        return;

    for (BaseTimings *bt : qAsConst(activated_childs))
    {
        bt->resumeAllTimers();
        emitResumeElementSignal(bt);
    }
}

bool TContainer::hasActivatedChild()
{
    return (activated_childs.size() > 0);
}

void TContainer::emitfinishedElement() // slot called from EndTimer
{
    emitStopElementSignal(this, false);
}

void TContainer::handleTriggerStops(bool is_forced)
{
    // if a repeated timer trigger like begin-end wallclock or a begin-end list perform a check
    selectWhichTimerShouldStop();
    stopAllActivatedChilds(is_forced); // must be after emit stop
    removeAllActiveChilds();
}

bool TContainer::proceedStart()
{
    if (has_started)
    {
        // check if this is a restart attempt and check restart attribute
        if (isRestartable())
        {
            interruptByRestart();
        }
        else
        {
            return false;
        }
    }
    collectActivatableChilds();
    has_started = true;

    return true;
}

void TContainer::emitPause()
{
    emitPauseElementSignal(this);
}

void TContainer::emitResume()
{
    emitResumeElementSignal(this);
}

void TContainer::insertAsActiveChildFromDom(QDomElement e)
{
    insertActivatedChild(findChildByDom(e));
}

void TContainer::insertActivatedChild(BaseTimings *MyBaseTimings)
{
    if (MyBaseTimings == Q_NULLPTR)
        return;

    qDebug() << "Parent: " + getID() << "insert activated: " +  MyBaseTimings->getID();
    if (activated_childs.indexOf(MyBaseTimings) < 0) // do not have double items
        activated_childs.append(MyBaseTimings);
}
