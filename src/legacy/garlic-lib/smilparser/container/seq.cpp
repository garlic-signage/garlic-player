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

#include "seq.h"

TSeq::TSeq(Expr *expr, QObject *parent) : TContainer(expr, parent)
{
    setObjectName("TSeq");
}

TSeq::~TSeq()
{
}

void TSeq::preloadParse(QDomElement element)
{
    root_element   = element;
    parseTimingAttributes();
    if (!root_element.hasChildNodes())
        return;

    traverseChilds();

    QDomElement e = root_element.firstChildElement();
    if (e.tagName() == "metadata")
    {
        MyShuffle = new TShuffle(dom_childs_list, this);
        MyShuffle->parse(e);
    }
}

void TSeq::prepareDurationTimers()
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

void TSeq::next(BaseTimings *ended_element)
{
    if (status == _stopped)
        return;

    removeActiveChild(ended_element);

    if (hasActivatedChild())
    {
        startFirstActivatedChild();
        return;
    }

    // not stopped by parent
    if (status == _active)
        finishIntrinsicDuration(); // check for dur and the for repeat play
}

void TSeq::start()
{
    // eigentlich ist das nicht sinnvoll
    if (dom_childs_list.size() == 0)
        return;

    // check if this is a restart attempt and check restart attribute
    if (!proceedStart())
    {
       return;
    }

    startFirstActivatedChild();
}

void TSeq::repeat()
{
    if (dom_childs_list.size() == 0)
        return;

    collectActivatableChilds();
    startFirstActivatedChild();
}

void TSeq::stop(bool is_forced)
{
    handleTriggerStops(is_forced);
}

void TSeq::pause()
{
    status = _paused;
    pauseAllTimers();
    pauseFirstActivatedChild();
}

void TSeq::resume()
{
    status = _active;
    resumeAllTimers();
    resumeFirstActivatedChild();
}

void TSeq::collectActivatableChilds()
{
    QList<QDomElement>            list;
    QList<QDomElement>::iterator  iterator;

    if (MyShuffle == Q_NULLPTR)
        list = dom_childs_list;
    else
        list = MyShuffle->getShuffeledList();

    for (iterator = list.begin(); iterator < list.end(); iterator++)
    {
        insertAsActiveChildFromDom(*iterator);
    }
}

void TSeq::traverseChilds()
{
    QDomNodeList childs = root_element.childNodes();
    QDomElement  element;
    for (int i = 0; i < childs.length(); i++)
    {
        element = childs.item(i).toElement();
        if (element.tagName() != "metadata" && element.tagName() != "") // e.g. comments
        {
            dom_childs_list.append(element);
            emit preloadElementSignal(this, element);
        }
    }
    dom_childs_list_iterator = dom_childs_list.begin();
}

