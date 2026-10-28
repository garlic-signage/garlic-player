#include "target_trigger.h"

TargetTrigger::TargetTrigger(QObject *parent) : QObject(parent)
{

}

TargetTrigger::TargetTrigger()
{
    trigger_list.clear();
}

void TargetTrigger::insert(QString trigger, QString listener_id)
{
    if (!trigger.isEmpty() && !listener_id.isEmpty())
        trigger_list.insert(trigger, listener_id);
}

QStringList TargetTrigger::findListenerIDsByTriggerList(QStringList trigger_list)
{
    QStringList sl = {};
    for (const QString &s: qAsConst(trigger_list))
    {
        sl.append(findListenersByTrigger(s));
    }

    return sl;
}

QStringList TargetTrigger::findListenersByTrigger(QString trigger)
{
    QStringList sl = {};
    if (trigger_list.isEmpty())
        return sl;

    QMultiMap<QString, QString>::iterator i =  trigger_list.find(trigger);

    while (i != trigger_list.end() && i.key() == trigger)
    {
        sl.append(i.value());
        i++;
    }

    return sl;
}
