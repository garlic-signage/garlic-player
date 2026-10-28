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
#include "elements_container.h"

ElementsContainer::ElementsContainer(QObject *parent) : QObject(parent)
{

}

ElementsContainer::~ElementsContainer()
{
    // qDeleteAll crashes always althought elements_list contains pointer
    // lets trust Qt garbage collection

    elements_list.clear();
}

BaseTimings *ElementsContainer::findSmilElementByDom(QDomElement dom_element)
{
    return findSmilElementById(TBase::parseID(dom_element));
}

BaseTimings *ElementsContainer::findSmilElementById(QString id)
{
    BaseTimings                             *MyBaseTimings     = Q_NULLPTR;
    QHash<QString, BaseTimings *>::iterator  elements_iterator = elements_list.find(id);
    if (elements_iterator != elements_list.end())
    {
        MyBaseTimings = *elements_iterator;
    }
    return MyBaseTimings;
}


void ElementsContainer::insertSmilElement(BaseTimings *MyBaseTimings)
{
    elements_list.insert(MyBaseTimings->getID(), MyBaseTimings);
}

void ElementsContainer::distributeTriggers()
{
    QHash<QString, BaseTimings *>::Iterator i = elements_list.begin();
    BaseTimings *bt_listener = Q_NULLPTR;

    // traverse elements_container to find possible listeners which have external trigger
    while (i != elements_list.end())
    {
        bt_listener = i.value();
        // check if this is a listener and his begin depends on an external signal
        distributeBeginTrigger(bt_listener);

        // check if this is a listener and his end depends on an external signal
        distributeEndTrigger(bt_listener);
        i++;
    }
}

BaseTimings *ElementsContainer::findAccessKeysForBegin(QChar key)
{
    QMap<QChar, BaseTimings *>::Iterator i = begin_accesskey_list.find(key);
    if(i != begin_accesskey_list.end())
    {
        return  i.value();
    }
    return Q_NULLPTR;
}

BaseTimings *ElementsContainer::findAccessKeysForEnd(QChar key)
{
    QMap<QChar, BaseTimings *>::Iterator i = end_accesskey_list.find(key);
    if(i != end_accesskey_list.end())
    {
        return i.value();
    }

    return Q_NULLPTR;
}

BaseTimings *ElementsContainer::findNotifyForBegin(QString key)
{
    QMap<QString, BaseTimings *>::Iterator i = begin_notify_list.find(key);
    if(i != begin_notify_list.end())
    {
        return  i.value();
    }
    return Q_NULLPTR;
}

BaseTimings *ElementsContainer::findNotifyForEnd(QString key)
{
    QMap<QString, BaseTimings *>::Iterator i = end_notify_list.find(key);
    if(i != end_notify_list.end())
    {
        return i.value();
    }

    return Q_NULLPTR;
}

void ElementsContainer::distributeBeginTrigger(BaseTimings *bt_listener)
{
    QHash<QString, QString>  listener_trigger_list = bt_listener->fetchExternalBegins();
    QString                          listener_id   = bt_listener->getID();
    if (listener_trigger_list.isEmpty())
        return;

    // the element listen to one ore more external begin trigger
    // find the source and informs that they had to send triggers to the listener
    BaseTimings *bt_source = Q_NULLPTR;
    QHash<QString, QString> ::Iterator j = listener_trigger_list.begin();
    while (j != listener_trigger_list.end())
    {
        if (j.key() == "accesskey")
        {
            begin_accesskey_list.insert(j.value().at(0), bt_listener);
        }
        else if (j.key() == "notify")
        {
            begin_notify_list.insert(j.value(), bt_listener);
        }
        else
        {
            QHash<QString, BaseTimings *> ::Iterator test;
            test = elements_list.find(j.key());
            if (test != elements_list.end())
            {
                bt_source = test.value();
                bt_source->addToExternalBegins(j.value(),listener_id);
            }
        }
        j++;
    }
}

void ElementsContainer::distributeEndTrigger(BaseTimings *bt_listener)
{
    QHash<QString, QString> listener_trigger_list = bt_listener->fetchExternalEnds();
    QString                           listener_id = bt_listener->getID();
    if (listener_trigger_list.isEmpty())
        return;

    BaseTimings *bt_source = Q_NULLPTR;
    QHash<QString, QString> ::Iterator j = listener_trigger_list.begin();
    while (j != listener_trigger_list.end())
    {
        if (j.key() == "accesskey")
        {
            end_accesskey_list.insert(j.value().at(0), bt_listener);
        }
        else if (j.key() == "notify")
        {
            end_notify_list.insert(j.value(), bt_listener);
        }
        else
        {
            QHash<QString, BaseTimings *> ::Iterator test;
            test = elements_list.find(j.key());
            if (test != elements_list.end())
            {
                bt_source = test.value();
                bt_source->addToExternalEnds(j.value(),listener_id);
            }
        }
        j++;
    }
}
