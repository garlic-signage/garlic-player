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
#include "base_controller.h"

RestApi::V2::BaseController::BaseController(QObject *parent) : QObject(parent)
{

}

void RestApi::V2::BaseController::setLibFacade(LibFacade *lf)
{
    MyLibfacade = lf;
}

bool RestApi::V2::BaseController::validateToken(QString access_token)
{
    QString debug = MyLibfacade->getConfiguration()->getApiAccessToken();
    QDateTime expired = QDateTime::fromString(MyLibfacade->getConfiguration()->getApiAccessTokenExpire(), Qt::ISODate);

    if (debug != access_token || QDateTime::currentDateTime() > expired)
        return false;

    return true;
}

QString RestApi::V2::BaseController::respondJSONError(QString error_text)
{
    JsonResponse.reset();
    JsonResponse.insertStringValuePair("error", error_text);
    return JsonResponse.asString(false).toUtf8();
}
