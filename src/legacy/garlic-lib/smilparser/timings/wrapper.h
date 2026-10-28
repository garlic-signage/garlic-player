#ifndef WRAPPER_H
#define WRAPPER_H

#include <QObject>

namespace Timings
{
    class Wrapper : public QObject
    {
            Q_OBJECT
        public:
            explicit Wrapper(QObject *parent = nullptr);

        signals:

    };
}
#endif // WRAPPER_H
