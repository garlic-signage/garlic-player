#ifndef SYSTEMINFORMATIONCONTROLLER_H
#define SYSTEMINFORMATIONCONTROLLER_H

#include "base_controller.h"
#include <QObject>

namespace RestApi
{
    namespace V2
    {
        class SystemInfo : public BaseController
        {
                Q_OBJECT
            public:
                explicit SystemInfo(QObject *parent = nullptr);
                QString responseFirmwareInfo();
                QString responseModelInfo();
        };
    }
}
#endif // SYSTEMINFORMATIONCONTROLLER_H
