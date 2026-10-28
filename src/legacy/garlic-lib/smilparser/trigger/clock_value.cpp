/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2016 Nikolaos Saghiadinos <ns@smil-control.com>
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

#include "clock_value.hpp"

ClockValue::ClockValue(QObject *parent) : QObject(parent)
{
    Q_UNUSED(parent);
}

void ClockValue::parse(QString attr_value)
{
    ms     = calculateMilliseconds(attr_value);
    if (sign == '-')
        ms = ms * (-1);
}

qint64 ClockValue::getTriggerInMSec()
{
    return ms;
}

/**
 * @brief ClockValue::calculateDuration converts clock or timecount values to milliseconds
 *
 * @param  dur
 * @return
 */
qint64 ClockValue::calculateMilliseconds(QString dur)
{
    int duration = 0;

    dur = separateSign(dur);

    int length = dur.length(); // must be after a legthshorten with + or -

    if (dur.contains(QChar(':'))) // Full clock or partial clock values
    {
        QStringList ar = dur.split(QChar(':'));
        if (ar.size() == 3) // Full
            duration = (ar[0].toInt()*3600000) + (ar[1].toInt()*60000) + (ar[2].toFloat()*1000);
        else if (ar.size() == 2) // partial
            duration = (ar[0].toInt()*60000) + (ar[1].toFloat()*1000);
        // size 1 not possible here, cause no ':' would be treated as timecount
    }
    else // Timecount values
    {
        if (dur[length-1] == QChar('s'))
            duration = dur.midRef(0, length-1).toFloat()*1000;
        else if (dur[length-1] == QChar('h'))
            duration = dur.midRef(0, length-1).toFloat()*3600000;
        else if (dur.contains("min"))
            duration = dur.midRef(0, length-3).toFloat()*60000;
        else if (dur.contains("ms"))
             duration = dur.midRef(0, length-2).toInt();
        else // no marker means seconds
            duration = dur.toFloat()*1000;
    }
    return duration;
}

QString ClockValue::separateSign(QString dur)
{
    sign = '+';

    if(dur.at(0).isDigit())
        return dur;

    QChar c = dur.at(0);
    dur.remove(0, 1);

    if (c == '+' || c == '-')
        sign = c;

    return dur;
}
