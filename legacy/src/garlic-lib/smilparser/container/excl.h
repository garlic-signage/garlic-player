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

#ifndef TEXCL_H
#define TEXCL_H

#include <QMap>
#include <QList>
#include "priorityclass.h"
#include "excl/priority_queue.h"

class TExcl : public TContainer
{
        Q_OBJECT
    public:
        explicit TExcl(Expr *expr, QObject *parent);
                ~TExcl();

        void    preloadParse(QDomElement element);
        void    next(BaseTimings *ended_element);
        bool    determineContinue(BaseTimings *new_element);
        bool    interruptByEndSync(QString id); // called only when interrupted from endsync
        void    start();
        void    repeat();
        void    stop(bool is_forced); // called from regulary ended active duration
        void    pause();
        void    resume();
        void    collectActivatableChilds();

    public slots:
        void    prepareDurationTimers();

    private:
        QString                        endsync = "last";
        PriorityQueue                  MyPriorityQueue;
        QMap<int, TPriorityClass *>    PriorityClassList;
        TPriorityClass                *findPriorityClass(QDomElement dom_element);
        BaseTimings                   *current_activated_element = Q_NULLPTR;

        void         setCurrentActivedElement(BaseTimings *element);
        BaseTimings *getCurrentActiveElement();

        bool         areQueuesToProceed();
        void         secureRemoveActivated(BaseTimings *element);
        void         traverseChilds();
        void         traversePriorityClasses(QList<QDomElement> priority_class_childs);
        void         parsePriorityClass(QDomElement element);
        void         priorityStop();
        void         priorityPause();
        void         priorityNever(BaseTimings *new_element);
        void         priorityDefer(BaseTimings *new_element);


};

#endif // TEXCL_H
