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

#ifndef TPAR_H
#define TPAR_H

#include "container.h"

class TPar : public TContainer
{
        Q_OBJECT
    public:
        explicit TPar(Expr *expr, QObject *parent);
        ~TPar();
        void                preloadParse(QDomElement element);
        void                next(BaseTimings *ended_element);
        bool                interruptByEndSync(QString id);
        void                start();
        void                repeat();
        void                stop(bool is_forced);

        void                pause();
        void                resume();
        void                collectActivatableChilds();
    protected:
        void                traverseChilds();
    protected slots:
        void                prepareDurationTimers();
    private:
        QString             endsync            = "last";
        bool                is_endsync   = false;
        QDomNodeList        childs;

};

#endif // TPAR_H
