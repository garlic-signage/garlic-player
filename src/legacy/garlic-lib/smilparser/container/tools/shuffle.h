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
#ifndef TSHUFFLE_H
#define TSHUFFLE_H

#include <QObject>
#include <QtXml>
#include <algorithm>

/**
 * @brief The TShuffle class
 * <metadata>
 *   <meta name="adapi:pickingAlgorithm" content="shuffle"/>
 *   <meta name="adapi:pickingBehavior" content="pickN"/>
 *   <meta name="adapi:pickNumber" content="1"/>
 * </metadata>
 */
class TShuffle : public QObject
{
        Q_OBJECT
    public:
        explicit TShuffle(QList<QDomElement> list, QObject *parent = Q_NULLPTR);
        void         parse(QDomElement metadata);
        QList<QDomElement> getShuffeledList();
    private:
        QList<QDomElement>            dom_list, shuffle_list, work_list;
        QList<QDomElement>::iterator  shuffle_iterator;
        QString pickingAlgorithm = "shuffle";
        QString pickingBehavior  = "pickN";
        int     pickNumber       = 1;
        void    checkForResetWorkList();
        void    randomizePlaylist();
};

#endif // TSHUFFLE_H
