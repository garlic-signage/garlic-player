#ifndef TIMER_HPP
#define TIMER_HPP

#include <QObject>
#include <QTimer>
#include <QDateTime>

class Timer : public QObject
{
        Q_OBJECT
    public:
        explicit Timer(QObject *parent = nullptr);
        void setRebootTime(const qint64 &reboot_time);
        void stopTimer();

    private slots:
        void emitReboot();

    private:
        QTimer RebootTimer;
        QDateTime reboot_time;
    signals:
        void reboot(QString);


};

#endif // TIMER_HPP
