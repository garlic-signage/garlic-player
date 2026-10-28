#ifndef BEGINTIMER_H
#define BEGINTIMER_H

#include "end_timer.h"
#include <QObject>

namespace Timings
{
    class BeginTimer : public Timings::EnhancedTimer
    {
            Q_OBJECT
        public:
            BeginTimer(QObject *parent = nullptr);
            qint64 getElapsedButActiveTimeTrigger();
            void   setDurTimer(SimpleTimer *dur);
            void   setEndTimer(Timings::EndTimer *end);
            bool   mustStartImmediately();
        protected:
        private:
            Timings::SimpleTimer *DurTimer = Q_NULLPTR;
            Timings::EndTimer *EndTimer = Q_NULLPTR;
            void resetNegativeTimeTrigger();
            bool allowFireImmediately();
            bool allowByDurTimer();

     };
}
#endif // BEGINTIMER_H
