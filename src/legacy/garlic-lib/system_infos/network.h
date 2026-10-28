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
#ifndef SYSTEMINFOSNETWORK_H
#define SYSTEMINFOSNETWORK_H

#include <QObject>
#include <QNetworkInterface>

namespace SystemInfos
{
    class Network : public QObject
    {
            Q_OBJECT
        public:
            explicit Network(QObject *parent = nullptr);
            bool    resetInterface();
            bool    nextInterface();
            bool    resetAddress();
            bool    isRealInterface();
            bool    nextAddress();
            int     countInterfaces();
            int     countAddresses();
            bool    isWifi();
            QString getInterfaceId();
            QString getMac();
            int     getProtocol();

            QString getType();
            QString getIP();
            QString getNetMask();
            QString getBroadcast();
        protected:
            QList<QNetworkInterface>               interface_list;
            QList<QNetworkInterface>::iterator     interface_list_iterator;
            QNetworkInterface                      current_interface;
            QList<QNetworkAddressEntry>            addresses_list;
            QNetworkAddressEntry                   current_address;
            QList<QNetworkAddressEntry>::iterator  addresses_list_iterator;
            void                                   prepareCurrentInterface();

    };
}
#endif // SYSTEMINFOSNETWORK_H
