#ifndef REQUEST_MAPPER_H
#define REQUEST_MAPPER_H

#include <httprequesthandler.h>
#include "lib_facade.h"
#include "v2/oauth.h"
#include "v2/system_info.h"
#include "v2/files.h"
#include "v2/app.h"
#include "v2/task.h"
#include <staticfilecontroller.h>

using namespace stefanfrings;

namespace RestApi
{
    class RequestMapper : public HttpRequestHandler
    {
            Q_OBJECT
        public:
            RequestMapper(LibFacade *lf, QSettings *HttpdSettings, QObject* parent = Q_NULLPTR);
            void service(HttpRequest& request, HttpResponse& response);
        private:
            LibFacade               *MyLibFacade;
            RestApi::V2::OAuth       MyAuthController;
            RestApi::V2::SystemInfo  MySIController;
            RestApi::V2::Files       MyFilesController;
            RestApi::V2::App         MyAppController;
            RestApi::V2::Task        MyTaskController;
            StaticFileController*    MyStaticFileController;
            void queryOauth(HttpRequest &request, HttpResponse &response);
            void querySystem(HttpRequest &request, HttpResponse &response, QStringList path);
            void queryFiles(HttpRequest &request, HttpResponse &response, QStringList path);
            void queryApp(HttpRequest &request, HttpResponse &response, QStringList path);
            void queryTask(HttpRequest &request, HttpResponse &response, QStringList path);
            void queryPlayerCache(HttpRequest &request, HttpResponse &response);

            void respond(HttpResponse& response, QString json);
            void respondImage(HttpResponse& response, QString file_path);
            void responseNotFound(HttpResponse& response);
            void responseAccessViolation(HttpResponse& response);
        signals:
            void               playIndex(QString *path);
            void               changeIndex(QString *path);
            void               changeConfig();
    };
}

#endif // REQUEST_MAPPER_H
