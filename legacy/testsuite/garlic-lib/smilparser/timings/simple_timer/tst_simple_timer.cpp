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
#include "tst_simple_timer.hpp"
#include "smilparser/timings/simple_timer.hpp"

void TestSimpleTimer::testParse()
{
    Timings::SimpleTimer timer;
    timer.parse("indefinite");
    QCOMPARE(timer.getType(), Timings::SimpleTimer::TYPE_INDEFINITE);

    timer.parse("media");
    QCOMPARE(timer.getType(), Timings::SimpleTimer::TYPE_MEDIA);

    timer.parse("00:01:00");
    QCOMPARE(timer.getType(), Timings::SimpleTimer::TYPE_CLOCKVALUE);
    QCOMPARE(timer.getParsedClock(), 60000);

}

void TestSimpleTimer::testIsActive()
{
    Timings::SimpleTimer timer;
    timer.parse("indefinite");
    QVERIFY(timer.isActive());

    timer.parse("media");
    QVERIFY(!timer.isActive());

    timer.parse("00:00:01");
    QVERIFY(!timer.isActive());
}

void TestSimpleTimer::testStart()
{
    Timings::SimpleTimer timer;
    timer.parse("indefinite");
    QVERIFY(timer.start());

    timer.parse("media");
    QVERIFY(!timer.start());

    timer.parse("00:00:01");
    QSignalSpy spy(&timer, &Timings::SimpleTimer::timeout);
    QVERIFY(spy.isValid());

    QVERIFY(timer.start());
    QTest::qWait(1001);

    QCOMPARE(spy.count(), 1);  // Verify that the timeout signal was emitted once
    QVERIFY(!timer.isActive());
}

void TestSimpleTimer::testPauseResume()
{
    Timings::SimpleTimer timer;
    timer.parse("00:00:10");

    timer.start();
    QTest::qWait(500);
    timer.pause();

    int remaining = timer.getRemaining();
    QVERIFY(remaining > 0);

    timer.resume();
    QVERIFY(timer.isActive());

}

void TestSimpleTimer::testStop()
{
    Timings::SimpleTimer timer;
    timer.parse("00:00:10");

    timer.start();
    QVERIFY(timer.isActive());

    timer.stop();
    QVERIFY(!timer.isActive());
    QCOMPARE(timer.getRemaining(), 0);
}

void TestSimpleTimer::testRecalculateTimeClock()
{
    Timings::SimpleTimer timer;
    timer.parse("00:00:10");
    QCOMPARE(timer.getParsedClock(), 10000);

    timer.recalculateTimeClock(5000);
    QCOMPARE(timer.getParsedClock(), 10000);
    QCOMPARE(timer.getClockInMs(), 10000);

    timer.recalculateTimeClock(-5000);
    QCOMPARE(timer.getClockInMs(), 5000);
    QCOMPARE(timer.getParsedClock(), 10000);
}


static TestSimpleTimer TEST_SIMPLE_TIMER;
