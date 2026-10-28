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

#ifndef TCLOCKVALUE_H
#define TCLOCKVALUE_H
#include <QObject>

/**
 * @brief The TClockValue class calculate timings for dur, min and max attributes.
 *        also as offset value for begin and end attributes
 *        https://www.w3.org/TR/REC-smil/smil-timing.html#q23
 */
class ClockValue : public QObject
{
    Q_OBJECT
public:
    explicit ClockValue(QObject *parent = Q_NULLPTR);
    void     parse(QString attr_value);
    QString  getStatus();
    qint64   getTriggerInMSec();
protected:
    qint64    ms          = 100;
    qint64    calculateMilliseconds(QString dur);
    QString   separateSign(QString dur);
    QChar     sign;
signals:

public slots:
};

#endif // TCLOCKVALUE_H
