#include "timer.hpp"

Timer::Timer(QObject *parent) : QObject{parent}
{
    connect(&RebootTimer, &QTimer::timeout, this, &Timer::emitReboot);
}

void Timer::setRebootTime(const qint64 &reboot_time)
{
    if (reboot_time < 0)
        return;

    RebootTimer.setSingleShot(true);
    RebootTimer.start(reboot_time);

}

void Timer::stopTimer()
{
    if (RebootTimer.isActive())
        RebootTimer.stop();
}

void Timer::emitReboot()
{
    emit reboot("DailyReboot");
}

