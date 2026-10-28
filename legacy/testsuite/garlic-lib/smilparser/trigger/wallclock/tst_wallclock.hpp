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
#include "base_testsuite.hpp"
#include <QtTest/QtTest>

class TestWallClock : public BaseTestSuite
{
        Q_OBJECT
    public:
        TestWallClock(){}

    private Q_SLOTS:
        void test_parse1();
        void test_parse2();
        void test_parse3();
        void test_parse4();
        void test_parseDateWithWeekDay();
        void test_getTimerTriggerWithDayPeriod();
        void test_getTimerTriggerWithHourPeriod();
        void test_getTimerTriggerWithSecondPeriod();
        void test_getTimerTriggerWithoutPeriod();
        void test_getTimerTriggerWithoutPeriodAndLeapYear();
        void test_getTimerTriggerWithWeekPeriod1();
        void test_getTimerTriggerWithWeekPeriod2();
        void test_getTimerTriggerWithWeekPeriod3();
        void test_getTimerTriggerWithMinutePeriodRepeated();
        void test_getTimerTriggerWithSecondsPeriodRepeated();
};
