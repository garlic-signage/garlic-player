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
#ifndef ElementFactory_H
#define ElementFactory_H

#include "base_timings.h"
#include "media_manager.h"
#include "placeholder.h"
#include "conditional/expr.hpp"
#include <QObject>


class ElementFactory : public QObject
{
        Q_OBJECT
    public:
        explicit ElementFactory(Files::MediaManager *mm, MainConfiguration *config, SmilHead::PlaceHolder *ph, Expr *expr, QObject *parent = Q_NULLPTR);
        BaseTimings    *createBase(QDomElement dom_element, QObject *parent);
    private:
        Files::MediaManager   *MyMediaManager;
        SmilHead::PlaceHolder *MyPlaceHolder;
        MainConfiguration     *MyMainConfiguration;
        Expr                  *MyExpr;

};

#endif // ElementFactory_H
