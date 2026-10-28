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
#include "request_mapper.h"

RestApi::RequestMapper::RequestMapper(LibFacade *lf, QSettings *HttpdSettings, QObject* parent) : HttpRequestHandler(parent)
{
    MyLibFacade          = lf;
    MyStaticFileController = new  StaticFileController(HttpdSettings, parent);
}

void RestApi::RequestMapper::service(HttpRequest& request, HttpResponse& response)
{
    QStringList sl  = QString(request.getPath()).split('/');
    qDebug() << sl;
    if (sl.size() < 4 || (sl.at(1) != "v2" && sl.at(1) != "cache"))
    {
        responseNotFound(response);
        return;
    }

    if (sl.at(1) == "cache")
    {
        queryPlayerCache(request, response);
        return;
    }


    if (sl.at(2) == "oauth2")
    {
        queryOauth(request, response);
    }
    else if (sl.at(2) == "system")
    {
        querySystem(request, response, sl);
    }
    else if (sl.at(2) == "files")
    {
        queryFiles(request, response, sl);
    }
    else if (sl.at(2) == "app")
    {
        queryApp(request, response, sl);
    }
    else if (sl.at(2) == "task")
    {
        queryTask(request, response, sl);
    }
    else
        responseNotFound(response);

}

void RestApi::RequestMapper::queryOauth(HttpRequest &request, HttpResponse &response)
{
    QByteArray path = request.getPath();
    QString json_response;
    if (path ==  "/v2/oauth2/token")
    {
        MyAuthController.setLibFacade(MyLibFacade);
        if (request.getHeader("content-type") == "application/json")
            json_response = MyAuthController.determineJsonResponse(request.getBody());
        else
            json_response = MyAuthController.determineResponse(request.getParameter("grant_type"),
                                                               request.getParameter("username"),
                                                               request.getParameter("password"));
        respond(response, json_response);
    }
    else
        responseNotFound(response);
}

void RestApi::RequestMapper::querySystem(HttpRequest& request, HttpResponse &response, QStringList path)
{
    MySIController.setLibFacade(MyLibFacade);
    if (!MySIController.validateToken(request.getParameter("access_token")))
    {
        responseAccessViolation(response);
        return;
    }

    if (path.at(3)  ==  "firmwareInfo")
    {
        QString res = MySIController.responseFirmwareInfo();
        respond(response, res);
    }
    else if (path.at(3) ==  "modelInfo")
    {
        QString res = MySIController.responseModelInfo();
        respond(response, res);
    }
    else
        responseNotFound(response);
}

void RestApi::RequestMapper::queryFiles(HttpRequest &request, HttpResponse &response, QStringList path)
{
    MyFilesController.setLibFacade(MyLibFacade);
    if (!MyFilesController.validateToken(request.getParameter("access_token")))
    {
        responseAccessViolation(response);
        return;
    }

    QString json_response;
    if (path.at(3)  ==  "new")
    {
       json_response = MyFilesController.createNew(request.getUploadedFile("data"),
                                                    request.getParameter("fileSize").toLong(),
                                                    request.getParameter("downloadPath"),
                                                    request.getParameter("etag"),
                                                    request.getParameter("mimeType"),
                                                    request.getParameter("modifiedDate")
                                                    );
    }
    else if (path.at(3)  ==  "find")
    {
        if (request.getHeader("content-type") == "application/json")
            json_response = MyFilesController.findPaginatedJson(request.getBody());
        else
            json_response = MyFilesController.findPaginated(request.getParameter("maxResults").toInt(), request.getParameter("pageToken").toInt());
    }
    else if (path.at(3)  ==  "delete")
    {
        if (request.getHeader("content-type") == "application/json")
            json_response = MyFilesController.removeByIdJson(request.getBody());
        else
            json_response = MyFilesController.removeById(request.getParameter("id"));
    }
    else
    {
        if (request.getMethod() == "GET")
            json_response = MyFilesController.findInfoByID(path.at(3));
        else if (request.getMethod() == "POST")
            json_response = MyFilesController.modifyByID(path.at(3),
                                                       request.getParameter("seek").toLong(),
                                                       request.getUploadedFile("data"),
                                                       request.getParameter("fileSize").toLong(),
                                                       request.getParameter("downloadPath"),
                                                       request.getParameter("etag"),
                                                       request.getParameter("mimeType"),
                                                       request.getParameter("modifiedDate"));
    }
    respond(response, json_response);
}

void RestApi::RequestMapper::queryApp(HttpRequest &request, HttpResponse &response, QStringList path)
{
    QString json_response;
    MyAppController.setLibFacade(MyLibFacade);
    if (!MyAppController.validateToken(request.getParameter("access_token")))
    {
        responseAccessViolation(response);
        return;
    }
    if (path.at(3)  ==  "exec") // start uri but do not change Content-Url
    {
        if (request.getHeader("content-type") == "application/json")
            json_response = MyAppController.execJson(request.getBody());
        else
            json_response = MyAppController.exec(request.getParameter("uri"),
                                                request.getParameter("packageName"),
                                                request.getParameter("className"),
                                                request.getParameter("Action"),
                                                request.getParameter("Type")
                                                );

        respond(response, json_response);
    }
    else if (path.at(3)  ==  "start") // change Content-Url and switch to it
    {
        if (request.getHeader("content-type") == "application/json")
            json_response = MyAppController.startJson(request.getBody());
        else
            json_response = MyAppController.start(request.getParameter("uri"),
                                                request.getParameter("packageName"),
                                                request.getParameter("className"),
                                                request.getParameter("Action"),
                                                request.getParameter("Type")
                                                );

        respond(response, json_response);
    }
    else if (path.at(3)  ==  "switch") // switch and play current Content-Url
    {
        if (request.getHeader("content-type") == "application/json")
            json_response = MyAppController.switchJson(request.getBody());
        else
            json_response = MyAppController.switchByMode(request.getParameter("mode"));

        respond(response, json_response);
    }
    else
        responseNotFound(response);

}

void RestApi::RequestMapper::queryTask(HttpRequest &request, HttpResponse &response,  QStringList path)
{
    MyTaskController.setLibFacade(MyLibFacade);
    QString json_response;
    if (!MyTaskController.validateToken(request.getParameter("access_token")))
    {
        responseAccessViolation(response);
        return;
    }
    if (path.at(3)  ==  "notify")
    {
        if (request.getHeader("content-type") == "application/json")
          json_response = MyTaskController.fireNotifyViaJson(request.getBody());
        else
          json_response = MyTaskController.fireNotify(request.getParameter("smilEvent"));

        respond(response, json_response);
    }
    else if (path.at(3)  ==  "reboot")
    {
        MyTaskController.reboot();
        respond(response, "");
    }
    else if (path.at(3)  ==  "screenshot")
    {
        QString file_path = MyLibFacade->getConfiguration()->getPaths("logs") + QDateTime::currentDateTime().toString("yyyyMMddhhmmss") +".jpg";
        MyTaskController.takeScreenShot(file_path);
        respondImage(response, file_path);
    }
 /*   else if (path.at(3)  ==  "commitConfiguration")
    {

    }
    else if (path.at(3)  ==  "exportConfiguration")
    {

    }
*/    else
        responseNotFound(response);
}

void RestApi::RequestMapper::queryPlayerCache(HttpRequest &request, HttpResponse &response)
{
    MyStaticFileController->service(request,response);
}

void RestApi::RequestMapper::respond(HttpResponse& response, QString json)
{
    response.setHeader("Cache-Control", "no-cache,no-store");
    response.setHeader("Pragma", "no-cache");
    response.setHeader("Content-Type", "application/json; charset=utf-8");
    response.write(json.toUtf8(), true);
}

void RestApi::RequestMapper::respondImage(HttpResponse& response, QString file_path)
{
    // the quick and Dirty solution as Image is saved in another thread and we have to wait.
    // Alternative send Signals.
    QTime dieTime = QTime::currentTime().addSecs(1);
    while (QTime::currentTime() < dieTime)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    }

    QFile f(file_path);
    if (!f.exists())
    {
        response.write("file not created.",true);
        return;
    }
    if (!f.open(QFile::ReadOnly))
    {
        response.write("file not readable.",true);
        return;
    }
    response.setHeader("Content-Type", "image/jpeg");
    response.setHeader("Content-Length", f.size());
    response.write(f.readAll(), true);
    f.close();
}

void RestApi::RequestMapper::responseNotFound(HttpResponse& response)
{
    response.setStatus(404,"Not found");
    response.write("The URL is wrong, no such document.",true);
}

void RestApi::RequestMapper::responseAccessViolation(HttpResponse& response)
{
    response.setStatus(403,"Access violation");
    response.write("{'error':'ACCESSVIOLATION'}",true);
}
