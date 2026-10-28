#ifndef ENDTIMER_H
#define ENDTIMER_H

#include "enhanced_timer.h"
#include <QObject>

namespace Timings
{
    class EndTimer : public Timings::EnhancedTimer
    {
            Q_OBJECT
        public:
            EndTimer(QObject *parent = nullptr);
            qint64 getNegativeTrigger();
            qint64 getPositiveTrigger();
            qint64 hasFireImmediately();
    };
}
#endif // ENDTIMER_H
