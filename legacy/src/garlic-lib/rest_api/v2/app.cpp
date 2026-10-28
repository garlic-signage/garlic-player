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
#include "app.h"

RestApi::V2::App::App(QObject *parent) : BaseController{parent}
{

}

QString RestApi::V2::App::startJson(QString json_string)
{
    if (!JsonRequest.readFromString(json_string))
        return respondJSONError("No JSON found");

    return start(JsonRequest.getStringValueByKey("uri"),
                 JsonRequest.getStringValueByKey("packageName"),
                 JsonRequest.getStringValueByKey("className"),
                 JsonRequest.getStringValueByKey("Action"),
                 JsonRequest.getStringValueByKey("Type")
                );
}

QString RestApi::V2::App::start(QString uri, QString package_name, QString class_name, QString action, QString type)
{
    Q_UNUSED(action);Q_UNUSED(type);
    if (!MyLibfacade->getConfiguration()->validateContentUrl(uri))
        return respondJSONError(MyLibfacade->getConfiguration()->getErrorText());

    MyLibfacade->getConfiguration()->determineIndexUri(uri);
    MyLibfacade->initParser();

    JsonResponse.reset();
    JsonResponse.insertStringValuePair("uri", uri);
    JsonResponse.insertStringValuePair("packageName", package_name);
    JsonResponse.insertStringValuePair("className", class_name);
    JsonResponse.insertStringValuePair("data", uri);
    return JsonResponse.asString(false).toUtf8();
}

QString RestApi::V2::App::execJson(QString json_string)
{
    if (!JsonRequest.readFromString(json_string))
        return respondJSONError("No JSON found");

    return exec(JsonRequest.getStringValueByKey("uri"),
                 JsonRequest.getStringValueByKey("packageName"),
                 JsonRequest.getStringValueByKey("className"),
                 JsonRequest.getStringValueByKey("Action"),
                 JsonRequest.getStringValueByKey("Type")
                );
}

QString RestApi::V2::App::exec(QString uri, QString package_name, QString class_name, QString action, QString type)
{
    Q_UNUSED(action);Q_UNUSED(type);
    if (!MyLibfacade->getConfiguration()->validateContentUrl(uri))
        return respondJSONError(MyLibfacade->getConfiguration()->getErrorText());

    MyLibfacade->initParserWithTemporaryFile(uri);

    JsonResponse.reset();
    JsonResponse.insertStringValuePair("uri", uri);
    JsonResponse.insertStringValuePair("packageName", package_name);
    JsonResponse.insertStringValuePair("className", class_name);
    JsonResponse.insertStringValuePair("data", uri);
    return JsonResponse.asString(false).toUtf8();
}

QString RestApi::V2::App::switchJson(QString json_string)
{
    if (!JsonRequest.readFromString(json_string))
        return respondJSONError("No JSON found");

    return switchByMode(JsonRequest.getStringValueByKey("mode"));
}

QString RestApi::V2::App::switchByMode(QString mode)
{
    if (mode != "start")
        return respondJSONError("wrong mode");

    MyLibfacade->initParser();

    JsonResponse.reset();
    JsonResponse.insertStringValuePair("uri", MyLibfacade->getConfiguration()->getIndexUri());
    JsonResponse.insertStringValuePair("packageName", "");
    JsonResponse.insertStringValuePair("className", "");
    JsonResponse.insertStringValuePair("data", MyLibfacade->getConfiguration()->getIndexUri());
    return JsonResponse.asString(false).toUtf8();
}


