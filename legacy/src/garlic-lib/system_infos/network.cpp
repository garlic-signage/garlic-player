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
#include "system_infos/network.h"

SystemInfos::Network::Network(QObject *parent) : QObject(parent)
{
    interface_list = QNetworkInterface::allInterfaces();
    resetInterface();
}

bool SystemInfos::Network::resetInterface()
{
    if (countInterfaces() == 0)
        return false;
    interface_list_iterator = interface_list.begin();
    prepareCurrentInterface();
    return true;
}

bool SystemInfos::Network::nextInterface()
{
    interface_list_iterator++;
    if (interface_list_iterator == interface_list.end())
        return false;
    prepareCurrentInterface();
    return true;
}

bool SystemInfos::Network::resetAddress()
{
    if (countAddresses() == 0)
        return false;
    addresses_list_iterator = addresses_list.begin();
    current_address         = *addresses_list_iterator;
    return true;
}

bool SystemInfos::Network::isRealInterface()
{
    if (!current_interface.isValid() || current_interface.addressEntries().size() == 0 || getMac().isEmpty())
        return false;

    return true;
}

bool SystemInfos::Network::nextAddress()
{
    addresses_list_iterator++;
    if (addresses_list_iterator == addresses_list.end())
        return false;
    current_address = *addresses_list_iterator;
    return true;
}

int SystemInfos::Network::countInterfaces()
{
   return interface_list.size();
}

int SystemInfos::Network::countAddresses()
{
    return addresses_list.size();
}

QString SystemInfos::Network::getInterfaceId()
{
    return current_interface.name();
}

QString SystemInfos::Network::getMac()
{
    return current_interface.hardwareAddress();
}

int SystemInfos::Network::getProtocol()
{
    return current_address.ip().protocol();
}

QString SystemInfos::Network::getType()
{
   switch (current_interface.type())
   {
       case QNetworkInterface::Loopback:
           return "loopback";
       case QNetworkInterface::Virtual:
           return "virtual";
       case QNetworkInterface::Ethernet:
           return "ethernet";
       case QNetworkInterface::Wifi:
          return "wifi";
       case QNetworkInterface::CanBus:
          return "canbus";
       case QNetworkInterface::Ppp:
          return "Ppp";
       case QNetworkInterface::Slip:
          return "slip";
       case QNetworkInterface::Phonet:
          return "phonet";
       case QNetworkInterface::Ieee802154:
          return "ieee802154";
       case QNetworkInterface::SixLoWPAN:
          return "SixLoWPAN";
       case QNetworkInterface::Ieee80216:
          return "wimax";
       case QNetworkInterface::Ieee1394:
          return "firewire";
        default:
           return "unknown";


   }
}

QString SystemInfos::Network::getIP()
{
    return current_address.ip().toString();
}

QString SystemInfos::Network::getNetMask()
{
    return current_address.netmask().toString();
}

QString SystemInfos::Network::getBroadcast()
{
    return current_address.broadcast().toString();
}

void SystemInfos::Network::prepareCurrentInterface()
{
    current_interface    = *interface_list_iterator;
    addresses_list       = current_interface.addressEntries();
    resetAddress();
}
