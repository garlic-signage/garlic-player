#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include <QObject>
#include "base_timings.h"

class PriorityQueue : public QObject
{
        Q_OBJECT
    public:
        explicit PriorityQueue(QObject *parent = nullptr);
        void                          insertQueue(int priority_id, BaseTimings *element);
        void                          removeQueuedElements();
        int                           countQueue();
        BaseTimings                  *getHighestPriorityFromQueue();

    private:
        QMap<int, QStack<BaseTimings *> *>  MapQueue;

signals:

};

#endif // PRIORITY_QUEUE_H
