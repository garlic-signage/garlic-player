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

#include "par.h"

TPar::TPar(Expr *expr, QObject *parent) : TContainer(expr, parent)
{
    setObjectName("TPar");
}

TPar::~TPar()
{
}

void TPar::preloadParse(QDomElement element)
{
    root_element   = element;
    parseTimingAttributes();

    if (element.hasAttribute("endsync"))
        endsync = element.attribute("endsync");

    if (!element.hasChildNodes())
        return;

    traverseChilds();
}

void TPar::prepareDurationTimers()
{
    if (!MyExpr->executeQuery(expr))
    {
        skipElement();
        return;
    }

    // when a durtimer exists use it!
    if (hasDurTimer() && !startDurTimer() && !hasEndTimer())
    {
        skipElement();
        return;
    }

    // otherwise empty container with dur will not start
    if (dom_childs_list.size() > 0 || isDurTimerActive() || isEndTimerActive() || isBeginTimerActive())
    {
        resetInternalRepeatCount();
        emitStartElementSignal(this);
    }
    else
    {
        skipElement();
    }
}

/**
 * @brief TPar::next means that it looks if there are no active elements
 *        when nothing is active (anymore) and no repeat possible
 *        then the finished signal will emitted;
 * @return
 */
void TPar::next(BaseTimings *ended_element)
{
    if (status == _stopped)
        return;

    if (ended_element->getStatus() == _stopped)
        removeActiveChild(ended_element);

    if (interruptByEndSync(ended_element->getID()))
        return;

    if (hasActivatedChild()) // do nothing when childs are active
        return;

    if (status == _active)
         finishIntrinsicDuration(); // check for dur and then for repeat play
}

bool TPar::interruptByEndSync(QString id)
{
    // Active DurTimer or EndTimer results in ignore endsync
    if ((endsync == "first" || endsync == id) && !isDurTimerActive() && !isEndTimerActive())
    {
        status = _stopped;
        stopAllActivatedChilds(false);
        removeAllActiveChilds();
        finishedSimpleDuration();
        return true;
    }
    return false;
}

void TPar::start()
{
    if (dom_childs_list.size() == 0)
        return;

    // check if this is a restart attempt and check restart attribute
    if (!proceedStart())
    {
       return;
    }

    startAllActiveChilds();
}

void TPar::repeat()
{
    if (dom_childs_list.size() == 0)
        return;

    collectActivatableChilds();
    startAllActiveChilds();
}

void TPar::stop(bool is_forced)
{
    handleTriggerStops(is_forced);
}

void TPar::pause()
{
    status = _paused;
    pauseAllTimers();
    pauseAllActivatedChilds();
}

void TPar::resume()
{
    status = _active;
    resumeAllTimers();
    resumeAllActivatedChilds();
}

void TPar::collectActivatableChilds()
{
    for (dom_childs_list_iterator = dom_childs_list.begin(); dom_childs_list_iterator < dom_childs_list.end(); dom_childs_list_iterator++)
    {
        insertAsActiveChildFromDom(*dom_childs_list_iterator);
    }
}

void TPar::traverseChilds()
{
    // put all playlist elements into a QList
    QDomNodeList childs = root_element.childNodes();
    QString tag_name = "";
    for (int i = 0; i < childs.length(); i++)
    {
        tag_name = childs.item(i).toElement().tagName();
        if (tag_name != "") // e.g. comments
        {
            dom_childs_list.append(childs.item(i).toElement());
            emit preloadElementSignal(this, childs.item(i).toElement());
        }
    }
}

