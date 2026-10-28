#ifndef APP_H
#define APP_H

#include "base_controller.h"
namespace RestApi
{
    namespace V2
    {
        class App : public BaseController
        {
                Q_OBJECT
            public:
                explicit App(QObject *parent = nullptr);
                QString startJson(QString json_string);
                QString start(QString uri, QString package_name, QString class_name, QString action, QString type);
                QString execJson(QString json_string);
                QString exec(QString uri, QString package_name, QString class_name, QString action, QString type);
                QString switchJson(QString json_string);
                QString switchByMode(QString mode);
            signals:

        };
    }
}

#endif // APP_H
