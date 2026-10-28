#ifndef TOUCHMANAGER_H
#define TOUCHMANAGER_H

#include <QObject>

class TouchManager : public QObject
{
    Q_OBJECT
public:
    explicit TouchManager(QObject *parent = nullptr);

signals:

};

#endif // TOUCHMANAGER_H
