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
#ifndef REPORTING_BASE_H
#define REPORTING_BASE_H

#include <QObject>
#include <QDomDocument>
#include <QDomElement>
#include "tools/main_configuration.hpp"
#include "system_infos/memory.h"
#include "system_infos/disc_space.hpp"

namespace Reporting
{
    class CreateBase : public QObject
    {
            Q_OBJECT
        public:
            explicit CreateBase(MainConfiguration *config, QObject *parent = nullptr);
            void     init();
            QString  asXMLString();
            void     setDiscSpace(SystemInfos::DiscSpace *value);

        protected:
            SystemInfos::DiscSpace              *MyDiscSpace;
            QScopedPointer<SystemInfos::Memory>  MyMemory;
            MainConfiguration                   *MyConfiguration;
            QDomDocument          document;
            QDomElement           root, player, system_info;
            void                  createSystemInfo();
            QDomElement           createTagWithTextValue(QString tag_name, QString tag_value);
            QDomElement           createPropTag(QString name, QString value);
    };
}

#endif // REPORTING_BASE_H
