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

#ifndef TCONTAINER_H
#define TCONTAINER_H

#include <QList>
#include "base_timings.h"

/**
 * @brief The TComntainer is an abstract class for Playlists Container which is inherited by seq, par and excl.
 *
 * It holds two lists. Alle children as DomElements and one ActivatedChilds with TBaseTimings
 *
 * ActivatedChilds are the Childs which can be played/displayed
 *
 */
class TContainer : public BaseTimings
{
    Q_OBJECT
    public:
        explicit TContainer(Expr *expr, QObject *parent);
        virtual void         next(BaseTimings *ended_element) = 0;

                QString      getBaseType() {return "container";}

                BaseTimings *findChildByDom(QDomElement dom_element);
                void         appendChild(BaseTimings *element);
                void         interruptByRestart();


        virtual void         collectActivatableChilds() = 0;
                void         removeActiveChild(BaseTimings *element);
                void         removeAllActiveChilds();

                // seq only maybe move to seq
                void         startFirstActivatedChild();
                void         pauseFirstActivatedChild();
                void         resumeFirstActivatedChild();

                void         startAllActiveChilds();
                void         stopAllActivatedChilds(bool is_forced);
                void         pauseAllActivatedChilds();
                void         resumeAllActivatedChilds();

                bool         hasActivatedChild();

                void         emitPreloadElementSignal(TContainer* p, QDomElement e){emit preloadElementSignal(p, e);}
                void         emitPause();
                void         emitResume();
    public slots:
                void         emitfinishedElement();

    protected:
                QList<BaseTimings *>          activated_childs;
                QHash<QString, BaseTimings *> childs_list;

                QList<QDomElement>            dom_childs_list;
                QList<QDomElement>::iterator  dom_childs_list_iterator;

                bool                          is_child_active    = false;

                void  handleTriggerStops(bool is_forced);
                bool  proceedStart();
        virtual void  traverseChilds() = 0;
                void  insertAsActiveChildFromDom(QDomElement e);
                void  insertActivatedChild(BaseTimings *MyBaseTimings);
    signals:
                void  preloadElementSignal(TContainer*, QDomElement);

};

#endif // TCONTAINER_H
