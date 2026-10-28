/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2024 Nikolaos Saghiadinos <ns@smil-control.com>
    This file is part of the garlic-player source code

    This program is free software: you can redistribute it and/or  modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************/
#include "scheduler.hpp"
#include <QDebug>
#include <QList>

Scheduler::Scheduler(IMainConfiguration *config, IWeekdayConverter *converter, QObject *parent) : QObject{parent}, MyMainConfiguration{config}, MyWeekdayConverter{converter}
{
}

void Scheduler::determineNextReboot(QDateTime current)
{
    reboot_days = MyMainConfiguration->getRebootDays().split(" ");

    if (reboot_days.size() == 0)
        return;

    reboot_time = QTime::fromString(MyMainConfiguration->getRebootTime());
    if (!reboot_time.isValid())
        reboot_time = QTime::fromString("03:00:00");

    now           = current;
    today         = current.date();
    current_time  = current.time();

    calculateNextReboot();
}

void Scheduler::calculateNextReboot()
{
    next_reboot_datetime = findEarliestReboot();

    qDebug() << "Next available appointment is on" << next_reboot_datetime.toString("dddd, yyyy-MM-dd HH:mm");
}


QDateTime Scheduler::getNextDatetime() const
{
    return next_reboot_datetime;
}

qint64 Scheduler::getNextDatetimeInMSecs() const
{
    return now.msecsTo(next_reboot_datetime);
}

QDateTime Scheduler::findEarliestReboot() const
{
    // as initialisation with max QDateime fails somehow we uyse one moth ahead.
    QDateTime maximum = QDateTime::currentDateTime().addMonths(1);
    QDateTime earliest_reboot = maximum;

    foreach (const QDateTime &reboot_time, computePotentialRebootTimes())
    {
        if (reboot_time < earliest_reboot)
        {
            earliest_reboot = reboot_time;
        }
    }

    if (earliest_reboot < maximum)
        return earliest_reboot;
    else
        return QDateTime();
}

QList<QDateTime> Scheduler::computePotentialRebootTimes() const
{
    QList<QDateTime> potential_reboot_times;

    foreach (const QString &day_str, reboot_days)
    {
        int day_of_week = MyWeekdayConverter->convertWeekDay(day_str);
        if (day_of_week > 0)
        {
            int days_to_add = calculateDaysToAdd(day_of_week);
            QDateTime potential_datetime(today.addDays(days_to_add), reboot_time);
            potential_reboot_times.append(potential_datetime);
        }
    }

    return potential_reboot_times;
}

int Scheduler::calculateDaysToAdd(int day_of_week) const
{
    int days_to_add = (day_of_week - today.dayOfWeek() + 7) % 7;

    if (days_to_add == 0 && current_time > reboot_time)
    {
        days_to_add = 7; // in case weekday is today but time passed
    }

    return days_to_add;
}

