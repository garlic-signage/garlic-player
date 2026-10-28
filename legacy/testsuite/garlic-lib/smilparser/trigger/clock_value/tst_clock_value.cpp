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

#include "tst_clock_value.hpp"

#include "smilparser/trigger/clock_value.hpp"

void TestClockValue::test_parse()
{
    ClockValue MyClockValue;
    // Full clock
    MyClockValue.parse("02:31:05.5");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(9065500));
    MyClockValue.parse("5.02s");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(5020));
    MyClockValue.parse("5.02");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(5020));
    MyClockValue.parse("2h");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(7200000));
    MyClockValue.parse("2.213h");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(7966800));
    MyClockValue.parse("12.7713h");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(45976680));
    MyClockValue.parse("11min");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(660000));
    MyClockValue.parse("4.15min");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(249000));
    MyClockValue.parse("some waste");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(0));
    return;
}


void TestClockValue::test_negativeValues()
{
    ClockValue MyClockValue;
    // Full clock
    MyClockValue.parse("-02:31:05.5");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(-9065500));
    MyClockValue.parse("-5.02s");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(-5020));
    MyClockValue.parse("-5.02");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(-5020));
    MyClockValue.parse("-2h");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(-7200000));
    MyClockValue.parse("-2.213h");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(-7966800));
    MyClockValue.parse("-12.7713h");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(-45976680));
    MyClockValue.parse("-11min");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(-660000));
    MyClockValue.parse("-4.15min");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(-249000));
    return;
}

void TestClockValue::test_RubbishSigned()
{
    ClockValue MyClockValue;
    // Full clock
    MyClockValue.parse("p02:31:05.5");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(9065500));
    MyClockValue.parse("a5.02s");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(5020));
    MyClockValue.parse("r5.02");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(5020));
    MyClockValue.parse("s2h");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(7200000));
    MyClockValue.parse("ererer2.213h");
    QCOMPARE(MyClockValue.getTriggerInMSec(), qint64(0));
    return;
}

static TestClockValue TEST_CLOCK_VALUE;
