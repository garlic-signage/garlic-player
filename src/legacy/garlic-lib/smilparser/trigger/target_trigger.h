#ifndef TARGETTRIGGER_H
#define TARGETTRIGGER_H

#include <QObject>
#include <QMultiMap>
/**
 * @brief Container to handle and store the TargetTrigger
 * for an BasTiming Object
 */
class TargetTrigger : public QObject
{
        Q_OBJECT
    public:
        explicit     TargetTrigger(QObject *parent = nullptr);
        explicit     TargetTrigger();
        void         insert(QString trigger, QString listener_id);
        QStringList findListenerIDsByTriggerList(QStringList trigger_list);
        QStringList findListenersByTrigger(QString trigger);
    private:
        QMultiMap<QString, QString> trigger_list = {};
    signals:

};

#endif // TARGETTRIGGER_H
