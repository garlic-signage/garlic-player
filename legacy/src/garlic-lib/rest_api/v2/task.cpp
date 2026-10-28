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
#include "task.h"

RestApi::V2::Task::Task(QObject *parent) : BaseController{parent}
{

}

QString RestApi::V2::Task::fireNotifyViaJson(QString json_string)
{
    if (!JsonRequest.readFromString(json_string))
    {
        return respondJSONError("No JSON found");
     }

   return fireNotify(JsonRequest.getStringValueByKey("smilEvent"));
}

QString RestApi::V2::Task::fireNotify(QString smil_event)
{
    JsonResponse.reset();
    if (!smil_event.isEmpty())
        MyLibfacade->transferNotify(smil_event);
    else
        JsonResponse.insertStringValuePair("error", " No SmilEvent exists");

    return JsonResponse.asString(false).toUtf8();
}

void RestApi::V2::Task::takeScreenShot(QString file_path)
{
    JsonResponse.reset();
    MyLibfacade->takeScreenshot(file_path);
}

void RestApi::V2::Task::reboot()
{
    MyLibfacade->reboot("httpd: " + QDateTime::currentDateTime().toString(Qt::ISODate));
}
