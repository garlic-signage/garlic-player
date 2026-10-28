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
#ifndef TPRIORITYCLASS_H
#define TPRIORITYCLASS_H
#include <QList>
#include <QStack>
#include "container.h"

/**
 * @brief TPriorityClass class
 * Priorities to high to lower is up to down
 *
 * elements in peers have no internal priority and interrupt each other
 *
 */
class TPriorityClass : public TBase
{
    Q_OBJECT
public:
    explicit TPriorityClass(QObject *parent);
    ~TPriorityClass();
    void                          preloadParse(QDomElement element);

    QString                       getPeers();
    QString                       getHigher();
    QString                       getLower();
    bool                          findElement(QDomElement dom_element);
    QList<QDomElement>            getChildList();
protected:
    QList<QDomElement>            childs_list;
    QList<QDomElement>::iterator  iterator;
private:
    QString                       peers  = "stop";   // how elements inside a group interrupts each other stop|pause|defer|never
    QString                       higher = "pause";  // how a group with higher priority interrupts this group pause|stop
    QString                       lower  = "defer";  // how a group with lower priority interrupts this group defer|never
    QString                       pause_display = "show";
    int                           count_childs       = 0;
    void                          setAttributes();
    void                          traverseChilds();
    void validatePeers();
    void validateHigher();
    void validateLower();
    void validatePauseDisplay();
};

#endif // TPRIORITYCLASS_H
