#ifndef BASECONTROLLER_H
#define BASECONTROLLER_H

#include <QObject>
#include "lib_facade.h"
#include "json_model.h"

namespace RestApi
{
    namespace V2
    {
        class BaseController : public QObject
        {
                Q_OBJECT
            public:
                explicit BaseController(QObject *parent = nullptr);
                void     setLibFacade(LibFacade *lf);
                bool     validateToken(QString access_token);
            protected:
                LibFacade         *MyLibfacade;
                JsonModel JsonRequest, JsonResponse;
                QString  respondJSONError(QString error_text);
        };
    }
}

#endif // BASECONTROLLER_H
