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
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <QObject>
#include <QDateTime>
#include "i_main_configuration.hpp"
#include "i_weekdayconverter.hpp"

class Scheduler : public QObject
{
        Q_OBJECT
    public:
        explicit Scheduler(IMainConfiguration *config, IWeekdayConverter *converter, QObject *parent = nullptr);
        void      determineNextReboot(QDateTime current);
        QDateTime getNextDatetime() const;
        qint64    getNextDatetimeInMSecs() const;

    private:
        IMainConfiguration *MyMainConfiguration;
        IWeekdayConverter  *MyWeekdayConverter;

        QDateTime          next_reboot_datetime;
        QStringList        reboot_days;
        QTime              reboot_time;
        QDateTime          now;
        QDate              today;
        QTime              current_time;

        void calculateNextReboot();
        int calculateDaysToAdd(int day_of_week) const;
        QDateTime findEarliestReboot() const;
        QList<QDateTime> computePotentialRebootTimes() const;

};

#endif // SCHEDULER_H
