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
#include "tst_wallclock.hpp"
#include "smilparser/trigger/wallclock.hpp"

void TestWallClock::test_parse1()
{
    WallClock MyWallClock;
    MyWallClock.parse("R/2001-04-05T12:13:01/P1D");
    IsoPeriod period = MyWallClock.getPeriod();
    QCOMPARE(MyWallClock.getRepeats(), int(-1));
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2001-04-05 12:13:01", "yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(period.years, int(0));
    QCOMPARE(period.months, int(0));
    QCOMPARE(period.days, qint64(1));
    QCOMPARE(period.hours, qint64(0));
    QCOMPARE(period.minutes, qint64(0));
    QCOMPARE(period.seconds, qint64(0));
}

void TestWallClock::test_parse2()
{
    WallClock MyWallClock;
    MyWallClock.parse("R31/2001-04-05/P1Y2W1DT6H2S");
    IsoPeriod period = MyWallClock.getPeriod();
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2001-04-05 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(MyWallClock.getRepeats(), int(31));
    QCOMPARE(period.years, int(1));
    QCOMPARE(period.months, int(0));
    QCOMPARE(period.days, qint64(15));
    QCOMPARE(period.hours, qint64(6));
    QCOMPARE(period.minutes, qint64(0));
    QCOMPARE(period.seconds, qint64(2));
}

void TestWallClock::test_parse3()
{
    WallClock MyWallClock;
    MyWallClock.parse("/2001-04-05T12:13:01/guas");
    IsoPeriod period = MyWallClock.getPeriod();
    QCOMPARE(MyWallClock.getRepeats(), int(0));
    QCOMPARE(period.years, int(0));
    QCOMPARE(period.months, int(0));
    QCOMPARE(period.days, qint64(0));
    QCOMPARE(period.hours, qint64(0));
    QCOMPARE(period.minutes, qint64(0));
    QCOMPARE(period.seconds, qint64(0));
}

void TestWallClock::test_parse4()
{
    WallClock MyWallClock;
    MyWallClock.parse("RZZ/2001-04-05T12:13:01/");
    IsoPeriod period = MyWallClock.getPeriod();
    QCOMPARE(MyWallClock.getRepeats(), int(0));
    QCOMPARE(period.years, int(0));
    QCOMPARE(period.months, int(0));
    QCOMPARE(period.days, qint64(0));
    QCOMPARE(period.hours, qint64(0));
    QCOMPARE(period.minutes, qint64(0));
    QCOMPARE(period.seconds, qint64(0));
}

void TestWallClock::test_parseDateWithWeekDay()
{
    WallClock MyWallClock;
    // day of week after
    MyWallClock.parse("2021-04-01+w1T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-04-05 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01+w2T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-04-06 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01+w3T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-04-07 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01+w4T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-04-08 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01+w5T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-04-02 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01+w6T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-04-03 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01+w7T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-04-04 00:00:00", "yyyy-MM-dd HH:mm:ss"));

    // day of week after without +
    MyWallClock.parse("2021-02-01w1T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-02-08 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-02-01w2T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-02-02 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-02-01w3T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-02-03 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-02-01w4T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-02-04 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-02-01w5T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-02-05 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-02-01w6T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-02-06 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-02-01w7T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-02-07 00:00:00", "yyyy-MM-dd HH:mm:ss"));

    // day of week before
    MyWallClock.parse("2021-04-01-w1T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-03-29 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01-w2T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-03-30 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01-w3T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-03-31 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01-w4T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-03-25 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01-w5T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-03-26 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01-w6T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-03-27 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    MyWallClock.parse("2021-04-01-w7T00:00");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2021-03-28 00:00:00", "yyyy-MM-dd HH:mm:ss"));
}

void TestWallClock::test_getTimerTriggerWithDayPeriod()
{
    WallClock MyWallClock;
    MyWallClock.parse("R5/2016-01-13T01:01:01/P1D");

    // trigger is in the future
    MyWallClock.calculateCurrentTrigger(QDateTime::fromString("2016-01-13 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(MyWallClock.getNextTimerTrigger(), qint64(3661000));
    QCOMPARE(MyWallClock.getPreviousTimerTrigger(), qint64(0));

    MyWallClock.calculateCurrentTrigger(QDateTime::fromString("2016-01-14 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(MyWallClock.getNextTimerTrigger(), qint64(3661000));
    QCOMPARE(MyWallClock.getPreviousTimerTrigger(), qint64(-82739000));

    MyWallClock.calculateCurrentTrigger(QDateTime::fromString("2016-01-18 00:00:02", "yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(MyWallClock.getNextTimerTrigger(), qint64(3659000));
    QCOMPARE(MyWallClock.getPreviousTimerTrigger(), qint64(-82741000));

    MyWallClock.calculateCurrentTrigger(QDateTime::fromString("2016-01-19 01:00:00", "yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(MyWallClock.getNextTimerTrigger(), qint64(61000));
    QCOMPARE(MyWallClock.getPreviousTimerTrigger(), qint64(-86339000));

    MyWallClock.calculateCurrentTrigger(QDateTime::fromString("2017-01-19 01:00:00", "yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(MyWallClock.getNextTimerTrigger(), qint64(0));
}

void TestWallClock::test_getTimerTriggerWithHourPeriod()
{
    WallClock MyWallClock;
    MyWallClock.parse("R/2001-01-13T01:01:01/PT1H");

    QDateTime current = QDateTime::fromString("2002-01-13 02:01:01", "yyyy-MM-dd HH:mm:ss");
    QDateTime next_expected = QDateTime::fromString("2002-01-13 03:01:01", "yyyy-MM-dd HH:mm:ss");
    QDateTime prev_expected = QDateTime::fromString("2002-01-13 01:01:01", "yyyy-MM-dd HH:mm:ss");

    MyWallClock.calculateCurrentTrigger(current);

    QCOMPARE(current.addMSecs(MyWallClock.getNextTimerTrigger()), next_expected);
    QCOMPARE(current.addMSecs(MyWallClock.getPreviousTimerTrigger()), prev_expected);

    current = QDateTime::fromString("2005-01-14 02:01:00", "yyyy-MM-dd HH:mm:ss");
    next_expected = QDateTime::fromString("2005-01-14 02:01:01", "yyyy-MM-dd HH:mm:ss");
    prev_expected = QDateTime::fromString("2005-01-14 01:01:01", "yyyy-MM-dd HH:mm:ss");
    MyWallClock.calculateCurrentTrigger(current);
    QCOMPARE(current.addMSecs(MyWallClock.getNextTimerTrigger()), next_expected);
    QCOMPARE(current.addMSecs(MyWallClock.getPreviousTimerTrigger()), prev_expected);

    current = QDateTime::fromString("2007-09-23 18:39:32", "yyyy-MM-dd HH:mm:ss");
    next_expected = QDateTime::fromString("2007-09-23 19:01:01", "yyyy-MM-dd HH:mm:ss");
    prev_expected = QDateTime::fromString("2007-09-23 18:01:01", "yyyy-MM-dd HH:mm:ss");
    MyWallClock.calculateCurrentTrigger(current);
    QCOMPARE(current.addMSecs(MyWallClock.getNextTimerTrigger()), next_expected);
    QCOMPARE(current.addMSecs(MyWallClock.getPreviousTimerTrigger()), prev_expected);

}

void TestWallClock::test_getTimerTriggerWithSecondPeriod()
{
    WallClock MyWallClock;
    MyWallClock.parse("R/2001-01-13T01:01:01/PT10S");

    MyWallClock.calculateCurrentTrigger(QDateTime::fromString("2016-01-31 19:20:42", "yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(MyWallClock.getNextTimerTrigger(), qint64(9000));
    QCOMPARE(MyWallClock.getPreviousTimerTrigger(), qint64(-1000));
}

void TestWallClock::test_getTimerTriggerWithoutPeriod()
{
    WallClock MyWallClock;
    MyWallClock.parse("2015-01-13T01:01:01");

    MyWallClock.calculateCurrentTrigger(QDateTime::fromString("2016-01-13 02:02:02", "yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(MyWallClock.getNextTimerTrigger(), qint64(0));
    QCOMPARE(MyWallClock.getPreviousTimerTrigger(), qint64(-31539661000));

}

void TestWallClock::test_getTimerTriggerWithoutPeriodAndLeapYear()
{
    WallClock MyWallClock;
    MyWallClock.parse("2021-01-01T01:01:01");
    MyWallClock.calculateCurrentTrigger(QDateTime::fromString("2020-01-01 01:01:01", "yyyy-MM-dd HH:mm:ss"));
    QCOMPARE(MyWallClock.getNextTimerTrigger(), qint64(31622400000)); // leap year ;)
    QCOMPARE(MyWallClock.getPreviousTimerTrigger(), qint64(0));
}

void TestWallClock::test_getTimerTriggerWithWeekPeriod1()
{
    WallClock MyWallClock;
    MyWallClock.parse("R/2019-01-01+w7T00:00/P1W");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2019-01-06 00:00:00", "yyyy-MM-dd HH:mm:ss"));
    IsoPeriod period = MyWallClock.getPeriod();
    QCOMPARE(MyWallClock.getRepeats(), int(-1));
    QCOMPARE(period.years, int(0));
    QCOMPARE(period.months, int(0));
    QCOMPARE(period.days, qint64(7));
    QCOMPARE(period.hours, qint64(0));
    QCOMPARE(period.minutes, qint64(0));
    QCOMPARE(period.seconds, qint64(0));

    QDateTime current = QDateTime::fromString("2021-05-04 21:00:00", "yyyy-MM-dd HH:mm:ss");
    QDateTime next_expected = QDateTime::fromString("2021-05-09 00:00:00", "yyyy-MM-dd HH:mm:ss");
    QDateTime prev_expected = QDateTime::fromString("2021-05-02 00:00:00", "yyyy-MM-dd HH:mm:ss");

    MyWallClock.calculateCurrentTrigger(current);
    QCOMPARE(current.addMSecs(MyWallClock.getNextTimerTrigger()), next_expected);
    QCOMPARE(current.addMSecs(MyWallClock.getPreviousTimerTrigger()), prev_expected);

    current = QDateTime::fromString("2021-12-23 21:00:00", "yyyy-MM-dd HH:mm:ss");
    next_expected = QDateTime::fromString("2021-12-26 00:00:00", "yyyy-MM-dd HH:mm:ss");
    prev_expected = QDateTime::fromString("2021-12-19 00:00:00", "yyyy-MM-dd HH:mm:ss");

    MyWallClock.calculateCurrentTrigger(current);
    QCOMPARE(current.addMSecs(MyWallClock.getNextTimerTrigger()), next_expected);
    QCOMPARE(current.addMSecs(MyWallClock.getPreviousTimerTrigger()), prev_expected);
}

void TestWallClock::test_getTimerTriggerWithWeekPeriod2()
{
    WallClock MyWallClock;
    MyWallClock.parse("R/2010-01-01+w3T18:45/P1W");
    QCOMPARE(MyWallClock.getDateTime(), QDateTime::fromString("2010-01-06 18:45:00", "yyyy-MM-dd HH:mm:ss"));
    IsoPeriod period = MyWallClock.getPeriod();
    QCOMPARE(MyWallClock.getRepeats(), int(-1));
    QCOMPARE(period.years, int(0));
    QCOMPARE(period.months, int(0));
    QCOMPARE(period.days, qint64(7));
    QCOMPARE(period.hours, qint64(0));
    QCOMPARE(period.minutes, qint64(0));
    QCOMPARE(period.seconds, qint64(0));

    QDateTime current = QDateTime::fromString("2021-05-04 14:00:00", "yyyy-MM-dd HH:mm:ss");
    QDateTime next_expected = QDateTime::fromString("2021-05-05 18:45:00", "yyyy-MM-dd HH:mm:ss");
    QDateTime prev_expected = QDateTime::fromString("2021-04-28 18:45:00", "yyyy-MM-dd HH:mm:ss");

    MyWallClock.calculateCurrentTrigger(current);
    QCOMPARE(current.addMSecs(MyWallClock.getNextTimerTrigger()), next_expected);
    QCOMPARE(current.addMSecs(MyWallClock.getPreviousTimerTrigger()), prev_expected);

    current = QDateTime::fromString("2021-12-23 12:32:51", "yyyy-MM-dd HH:mm:ss");
    next_expected = QDateTime::fromString("2021-12-29 18:45:00", "yyyy-MM-dd HH:mm:ss");
    prev_expected = QDateTime::fromString("2021-12-22 18:45:00", "yyyy-MM-dd HH:mm:ss");

    MyWallClock.calculateCurrentTrigger(current);
    QCOMPARE(current.addMSecs(MyWallClock.getNextTimerTrigger()), next_expected);
    QCOMPARE(current.addMSecs(MyWallClock.getPreviousTimerTrigger()), prev_expected);

    // if it is Wednesday
    current = QDateTime::fromString("2021-10-06 12:32:51", "yyyy-MM-dd HH:mm:ss");
    next_expected = QDateTime::fromString("2021-10-06 18:45:00", "yyyy-MM-dd HH:mm:ss");
    prev_expected = QDateTime::fromString("2021-09-29 18:45:00", "yyyy-MM-dd HH:mm:ss");

    MyWallClock.calculateCurrentTrigger(current);
    QCOMPARE(current.addMSecs(MyWallClock.getNextTimerTrigger()), next_expected);
    QCOMPARE(current.addMSecs(MyWallClock.getPreviousTimerTrigger()), prev_expected);
}

void TestWallClock::test_getTimerTriggerWithWeekPeriod3()
{
    WallClock MyWallClock1;
    MyWallClock1.parse("R/2007-01-01+w6T00:00:00/P1W");

    QDateTime current = QDateTime::fromString("2021-07-17 21:36:00", "yyyy-MM-dd HH:mm:ss");
    QDateTime prev_expected1 = QDateTime::fromString("2021-07-17 00:00:00", "yyyy-MM-dd HH:mm:ss");

    MyWallClock1.calculateCurrentTrigger(current);
    QCOMPARE(current.addMSecs(MyWallClock1.getPreviousTimerTrigger()), prev_expected1);


    WallClock MyWallClock2;
    MyWallClock2.parse("R/2007-01-01+w6T23:59:59/P1W");

    QDateTime prev_expected2 = QDateTime::fromString("2021-07-10 23:59:59", "yyyy-MM-dd HH:mm:ss");
    MyWallClock2.calculateCurrentTrigger(current);
    QCOMPARE(current.addMSecs(MyWallClock2.getPreviousTimerTrigger()), prev_expected2);
}

void TestWallClock::test_getTimerTriggerWithMinutePeriodRepeated()
{
    WallClock MyWallClock1;
    MyWallClock1.parse("R/2021-07-22T13:49:30/PT1M");

    QDateTime current1 = QDateTime::fromString("2021-07-22 13:50:40", "yyyy-MM-dd HH:mm:ss");
    QDateTime prev_expected1 = QDateTime::fromString("2021-07-22 13:50:30", "yyyy-MM-dd HH:mm:ss");
    QDateTime next_expected1 = QDateTime::fromString("2021-07-22 13:51:30", "yyyy-MM-dd HH:mm:ss");

    MyWallClock1.calculateCurrentTrigger(current1);
    QCOMPARE(current1.addMSecs(MyWallClock1.getPreviousTimerTrigger()), prev_expected1);
    QCOMPARE(current1.addMSecs(MyWallClock1.getNextTimerTrigger()), next_expected1);

    QDateTime current2 = QDateTime::fromString("2021-07-22 13:51:40", "yyyy-MM-dd HH:mm:ss");
    QDateTime next_expected2 = QDateTime::fromString("2021-07-22 13:52:30", "yyyy-MM-dd HH:mm:ss");

    MyWallClock1.calculateNextTrigger(current2);
    QCOMPARE(current2.addMSecs(MyWallClock1.getNextTimerTrigger()), next_expected2);

    QDateTime current3 = QDateTime::fromString("2021-07-22 19:13:31", "yyyy-MM-dd HH:mm:ss");
    QDateTime next_expected3 = QDateTime::fromString("2021-07-22 19:14:30", "yyyy-MM-dd HH:mm:ss");

    MyWallClock1.calculateNextTrigger(current3);
    qint64 next = MyWallClock1.getNextTimerTrigger();
    QCOMPARE(current3.addMSecs(next), next_expected3);
}

void TestWallClock::test_getTimerTriggerWithSecondsPeriodRepeated()
{
    WallClock MyWallClock1;
    MyWallClock1.parse("R/2021-07-22T13:51:00/PT10S");

    QDateTime current1 = QDateTime::fromString("2021-07-23 00:29:44", "yyyy-MM-dd HH:mm:ss");
    QDateTime prev_expected1 = QDateTime::fromString("2021-07-23 00:29:40", "yyyy-MM-dd HH:mm:ss");
    QDateTime next_expected1 = QDateTime::fromString("2021-07-23 00:29:50", "yyyy-MM-dd HH:mm:ss");

    MyWallClock1.calculateCurrentTrigger(current1);
    QCOMPARE(current1.addMSecs(MyWallClock1.getPreviousTimerTrigger()), prev_expected1);
    QCOMPARE(current1.addMSecs(MyWallClock1.getNextTimerTrigger()), next_expected1);

}


static TestWallClock TEST_WALL_CLOCK;
