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
#include "oauth.h"

RestApi::V2::OAuth::OAuth(QObject *parent): BaseController(parent)
{

}


QString RestApi::V2::OAuth::determineJsonResponse(QString json_string)
{
    if (!JsonRequest.readFromString(json_string))
        return respondJSONError("No JSON found");

    return determineResponse(JsonRequest.getStringValueByKey("grant_type"),
                                 JsonRequest.getStringValueByKey("username"),
                                 JsonRequest.getStringValueByKey("password")
    );
}

QString RestApi::V2::OAuth::determineResponse(QString grant_type, QString username, QString password)
{
    if (grant_type != "password")
        return respondJSONError("wrong grant_type");

    // Todo implement real username password
    if (username != "admin")
        return respondJSONError("wrong username");

    if (password != "")
        return respondJSONError("wrong password");

    return createJsonResponse();
}

QString RestApi::V2::OAuth::createJsonResponse()
{
    JsonResponse.reset();
    JsonResponse.insertStringValuePair("access_token", MyLibfacade->getConfiguration()->determineApiAccessToken(username, password));
    JsonResponse.insertStringValuePair("token_type","Bearer");
    JsonResponse.insertStringValuePair("expires_in", MyLibfacade->getConfiguration()->getApiAccessTokenExpire());
    return JsonResponse.asString(false).toUtf8();
}


