#ifndef ELEMENTSCONTAINER_H
#define ELEMENTSCONTAINER_H

#include <QObject>
#include <QHash>
#include "base_timings.h"

class ElementsContainer : public QObject
{
    Q_OBJECT
public:
    explicit ElementsContainer(QObject *parent = nullptr);
    ~ElementsContainer();
    BaseTimings  *findSmilElementByDom(QDomElement dom_element);
    BaseTimings  *findSmilElementById(QString id);
    void          insertSmilElement(BaseTimings *MyBaseTimings);
    void          distributeTriggers();
    BaseTimings  *findAccessKeysForBegin(QChar key);
    BaseTimings  *findAccessKeysForEnd(QChar key);
    BaseTimings  *findNotifyForBegin(QString key);
    BaseTimings  *findNotifyForEnd(QString key);
private:
    QHash<QString, BaseTimings *>       elements_list;
    QMap<QChar, BaseTimings *>          begin_accesskey_list;
    QMap<QChar, BaseTimings *>          end_accesskey_list;
    QMap<QString, BaseTimings *>        begin_notify_list;
    QMap<QString, BaseTimings *>        end_notify_list;
    void  distributeBeginTrigger(BaseTimings *bt_listener);
    void  distributeEndTrigger(BaseTimings *bt_listener);

signals:

};

#endif // ELEMENTSCONTAINER_H
