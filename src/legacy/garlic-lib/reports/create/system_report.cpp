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
#include "system_report.h"

Reporting::CreateSystemReport::CreateSystemReport(MainConfiguration *config,
                           QObject *parent) : Reporting::CreateBase(config, parent)
{
    MyConfiguration = config;
    MyNetwork.reset(new SystemInfos::Network(this));

}

void Reporting::CreateSystemReport::process()
{
    init();
    createSystemInfo();
    createNetwork();
    createConfiguration();
    createModelInfo();
    createFactoryDefault();
    createUserPref();
    createHardwareInfo();
}

void Reporting::CreateSystemReport::createNetwork()
{
    network = document.createElement("network");
    system_info.appendChild(network);
    MyNetwork->resetInterface();
    for(int i = 0; i < MyNetwork->countInterfaces(); i++)
    {
        if (MyNetwork->isRealInterface())
            appendNetworkChilds();

        MyNetwork->nextInterface();
    }
}

void Reporting::CreateSystemReport::createConfiguration()
{
    configuration = document.createElement("configuration");
    system_info.appendChild(configuration);
}

void Reporting::CreateSystemReport::createModelInfo()
{
    QDomElement model_info = document.createElement("modelInfo");
    configuration.appendChild(model_info);
    model_info.appendChild(createPropTag("PCB", MyConfiguration->getAppName()));
    model_info.appendChild(createPropTag("PCBRevision", ""));
    model_info.appendChild(createPropTag("operatingSystem", MyConfiguration->getOS()));
    model_info.appendChild(createPropTag("manufacturer", "Sagiadinos"));
    model_info.appendChild(createPropTag("manufacturerURL", "https://garlic-player.com"));
    model_info.appendChild(createPropTag("modelDescription", ""));
    model_info.appendChild(createPropTag("modelName", ""));
    model_info.appendChild(createPropTag("modelURL", ""));
    model_info.appendChild(createPropTag("option", ""));
}

void Reporting::CreateSystemReport::createFactoryDefault()
{
    QDomElement factory_default = document.createElement("factoryDefault");
    configuration.appendChild(factory_default);
    factory_default.appendChild(createTagWithTextValue("variant", ""));
}

void Reporting::CreateSystemReport::createUserPref()
{
    QDomElement user_pref = document.createElement("userPref");
    configuration.appendChild(user_pref);
    user_pref.appendChild(createPropTag("info.playerName", MyConfiguration->getPlayerName()));
    user_pref.appendChild(createPropTag("info.playGroup", ""));
    user_pref.appendChild(createPropTag("info.playGroupMaster", ""));
    user_pref.appendChild(createPropTag("system.locale", QLocale::system().name()));

    user_pref.appendChild(createPropTag("content.serverUrl", MyConfiguration->getIndexUri()));
}

void Reporting::CreateSystemReport::createHardwareInfo()
{
    hardware_info = document.createElement("hardwareInfo");
    player.appendChild(hardware_info);
    QDomElement hardware = document.createElement("hardware");
    hardware_info.appendChild(hardware);

    // TODO integrate TScreen Class into lib
    hardware.setAttribute("id", "display:0");
    hardware.appendChild(createPropTag("modelName", ""));
    hardware.appendChild(createPropTag("product_id", ""));
    hardware.appendChild(createPropTag("serialNumber", ""));
    hardware.appendChild(createPropTag("vendorId", ""));
}


void Reporting::CreateSystemReport::appendNetworkChilds()
{
    net_interface = document.createElement("interface");
    net_interface.setAttribute("id", MyNetwork->getInterfaceId());
    network.appendChild(net_interface);
    net_interface.appendChild(createTagWithTextValue("mac", MyNetwork->getMac()));
    net_interface.appendChild(createTagWithTextValue("type", MyNetwork->getType()));
    for(int i = 0; i < MyNetwork->countAddresses(); i++)
    {
        appendNetworkAddressChilds();
        MyNetwork->nextAddress();
    }
}

void Reporting::CreateSystemReport::appendNetworkAddressChilds()
{
    switch (MyNetwork->getProtocol())
    {
        case QAbstractSocket::IPv4Protocol:
            net_interface.appendChild(createTagWithTextValue("ip_v4", MyNetwork->getIP()));
            net_interface.appendChild(createTagWithTextValue("netmask_v4", MyNetwork->getNetMask()));
            net_interface.appendChild(createTagWithTextValue("broadcast_v4", MyNetwork->getBroadcast()));
            break;
        case QAbstractSocket::IPv6Protocol:
            net_interface.appendChild(createTagWithTextValue("ip_v6", MyNetwork->getIP()));
            net_interface.appendChild(createTagWithTextValue("netmask_v6", MyNetwork->getNetMask()));
            break;
        case QAbstractSocket::AnyIPProtocol:
            net_interface.appendChild(createTagWithTextValue("ip_both", MyNetwork->getIP()));
            net_interface.appendChild(createTagWithTextValue("netmask_both", MyNetwork->getNetMask()));
            net_interface.appendChild(createTagWithTextValue("broadcast_both", MyNetwork->getBroadcast()));
            break;
         default:
            break;

    }
}
