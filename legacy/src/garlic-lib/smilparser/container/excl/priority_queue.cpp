#include "priority_queue.h"

PriorityQueue::PriorityQueue(QObject *parent) : QObject(parent)
{

}

void PriorityQueue::insertQueue(int priority_id, BaseTimings *element)
{
     QStack<BaseTimings *> *temp_stack;

   // check if there exist an queue with priority_id
   if (MapQueue.find(priority_id) == MapQueue.end())
   {
       temp_stack = new QStack<BaseTimings *>;
       temp_stack->push(element);
       MapQueue.insert(priority_id, temp_stack);
   }
   else
   {
       QMap<int, QStack<BaseTimings *> *>::iterator it = MapQueue.find(priority_id);
       temp_stack = it.value();
       temp_stack->push(element);
   }
}

void PriorityQueue::removeQueuedElements()
{
    for (QStack<BaseTimings *> *temp_stack : qAsConst(MapQueue))
    {
       for (int i = 0; i < temp_stack->size(); i++)
       {
           BaseTimings *bt = temp_stack->pop();
           bt->stopAllTimers();
       }
       delete temp_stack;
    }
    MapQueue.clear();
}

int PriorityQueue::countQueue()
{
    return MapQueue.size();
}

BaseTimings *PriorityQueue::getHighestPriorityFromQueue()
{
    BaseTimings *ret = Q_NULLPTR;

    if (MapQueue.size() == 0)
        return ret;

    QStack<BaseTimings *> *temp_stack = MapQueue.first();

    if (temp_stack->size() > 0)
        ret = temp_stack->pop();

    if (temp_stack->size() == 0)
    {
        MapQueue.erase(MapQueue.begin());
        delete temp_stack;
    }

    return ret;
}
