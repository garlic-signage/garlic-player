#include "childs.h"

Container::Childs::Childs(QObject *parent) : QObject(parent)
{

}

void Container::Childs::removeChild(BaseTimings *element)
{
    qDebug() << "remove: " + element->getID() + " parent: " + element->getParentContainer()->getID();
    if (activated_childs.indexOf(element) >= 0)
        activated_childs.removeOne(element);
}

void Container::Childs::removeAllChilds()
{
    activated_childs.clear();
}
