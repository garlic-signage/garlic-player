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
#ifndef SYSTEM_REPORT_H
#define SYSTEM_REPORT_H

#include <QFileInfo>
#include <QTimeZone>

#include "base_reports.h"
#include "system_infos/network.h"

namespace Reporting
{
    class CreateSystemReport : public Reporting::CreateBase
    {
            Q_OBJECT
        public:
            explicit CreateSystemReport(
                                  MainConfiguration *config,
                                  QObject *parent = nullptr);
            void process();
        protected:
             QScopedPointer<SystemInfos::Network> MyNetwork;
             // must name net_interface cause windows makes trouble
             QDomElement           network, net_interface, hardware_info, configuration;
             void                  createConfiguration();
             void                  createModelInfo();
             void                  createFactoryDefault();
             void                  createUserPref();
             void                  createHardwareInfo();
             void                  createNetwork();
             void                  appendNetworkChilds();
             void                  appendNetworkAddressChilds();

    };
}
#endif // SYSTEM_REPORT_H
