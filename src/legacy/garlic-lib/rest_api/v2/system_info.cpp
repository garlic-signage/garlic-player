/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2024 Nikolaos Saghiadinos <ns@smil-control.com>
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
#include "system_info.h"

RestApi::V2::SystemInfo::SystemInfo(QObject *parent) : BaseController{parent}
{

}

QString RestApi::V2::SystemInfo::responseFirmwareInfo()
{
    JsonResponse.reset();
    JsonResponse.insertStringValuePair("firmwareVersion", MyLibfacade->getConfiguration()->getVersion());
    JsonResponse.insertStringValuePair("family", MyLibfacade->getConfiguration()->getAppName() + "-" + MyLibfacade->getConfiguration()->getOS());
    return JsonResponse.asString(false).toUtf8();
}

QString RestApi::V2::SystemInfo::responseModelInfo()
{
    JsonResponse.reset();
    JsonResponse.insertStringValuePair("modelDescription", "");
    JsonResponse.insertStringValuePair("modelName", MyLibfacade->getConfiguration()->getAppName());
    JsonResponse.insertStringValuePair("modelURL", "");
    JsonResponse.insertStringValuePair("manufacturer", "Sagiadinos");
    JsonResponse.insertStringValuePair("licenseModel", MyLibfacade->getConfiguration()->getOS());
    JsonResponse.insertStringValuePair("PCBRevision", "");
    JsonResponse.insertStringValuePair("manufacturerURL", "https://garlic-player.com");
    JsonResponse.insertStringValuePair("PCB", MyLibfacade->getConfiguration()->getAppName());
    JsonResponse.insertStringValuePair("options", "");
    return JsonResponse.asString(false).toUtf8();
}
