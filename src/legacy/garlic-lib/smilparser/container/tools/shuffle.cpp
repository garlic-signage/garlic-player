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
#include "shuffle.h"

TShuffle::TShuffle(QList<QDomElement> list, QObject *parent) : QObject(parent)
{
    dom_list   = list;
}

void TShuffle::parse(QDomElement metadata)
{
    QDomNodeList childs = metadata.childNodes();
    QDomElement  element;
    int          length = childs.length();
    for (int i = 0; i < length; i++)
    {
        element = childs.item(i).toElement();
        if (element.tagName() == "meta" && element.hasAttribute("name") && element.hasAttribute("content"))
        {
            if (element.attribute("name") == "adapi:pickingAlgorithm")
                pickingAlgorithm = element.attribute("content");
            else if (element.attribute("name") == "adapi:pickingBehavior")
                pickingBehavior = element.attribute("content");
            else if (element.attribute("name") == "adapi:pickNumber")
                pickNumber = element.attribute("content").toInt();
            randomizePlaylist();
        }
    }
    return;
}

QList<QDomElement> TShuffle::getShuffeledList()
{
    QList<QDomElement> ret_list;
    for (int i = 0; i < pickNumber; i++)
    {
        checkForResetWorkList();
        ret_list.append(work_list.takeFirst());
    }
    return ret_list;
}

void TShuffle::checkForResetWorkList()
{
    if (work_list.size() == 0)
    {
        work_list = shuffle_list;
    }
}

void TShuffle::randomizePlaylist()
{
    shuffle_list = dom_list;
    std::random_shuffle(shuffle_list.begin(), shuffle_list.end());

    shuffle_iterator = shuffle_list.begin();
    return;
}



