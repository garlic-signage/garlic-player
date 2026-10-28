#ifndef AUTHCONTROLLER_H
#define AUTHCONTROLLER_H

#include "base_controller.h"

namespace RestApi
{
    namespace V2
    {
        class OAuth : public BaseController
        {
                Q_OBJECT
            public:
                OAuth(QObject* parent = Q_NULLPTR);
                QString determineJsonResponse(QString json_string);
                QString determineResponse(QString grant_type, QString username, QString password);
            private:
                QString createJsonResponse();
                QString username, password;
        };
    }
}
#endif // AUTHCONTROLLER_H
