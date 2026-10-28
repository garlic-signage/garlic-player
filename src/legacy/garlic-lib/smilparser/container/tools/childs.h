#ifndef CHILDS_H
#define CHILDS_H

#include <QList>
#include "base_timings.h"

namespace Container
{
    class Childs : public QObject
    {
            Q_OBJECT
        public:
            explicit Childs(QObject *parent = nullptr);
            void         removeChild(BaseTimings *element);
            void         removeAllChilds();
        private:
            QList<BaseTimings *>    activated_childs;
            BaseTimings            *current_activated_element = Q_NULLPTR;

        signals:

    };
}
#endif // CHILDS_H
