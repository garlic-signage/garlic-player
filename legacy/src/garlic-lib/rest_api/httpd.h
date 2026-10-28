#ifndef HTTPD_H
#define HTTPD_H

#include "lib_facade.h"
#include <QCoreApplication>

namespace RestApi
{
    class Httpd : public QObject
    {
            Q_OBJECT
        public:
            explicit Httpd(LibFacade *lf, QObject *parent = nullptr);
            void init(QCoreApplication *app);
        private:
            void        determineHttpdSettings(QSettings *all_settings);
            void        initDefaultSettings();
            QSettings   HttpdSettings;
            LibFacade   *MyLibFacade;
    };
}
#endif // HTTPD_H
