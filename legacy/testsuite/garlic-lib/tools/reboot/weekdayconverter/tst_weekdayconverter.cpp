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
#include "tst_weekdayconverter.hpp"
#include "tools/reboot/weekdayconverter.hpp"

void TestWeekdayConverter::test_ConvertWeekDay()
{
    WeekdayConverter TestClass;

   QCOMPARE(TestClass.convertWeekDay("MonDay"), 1);
   QCOMPARE(TestClass.convertWeekDay("tuesday"), 2);
   QCOMPARE(TestClass.convertWeekDay("Wednesday"), 3);
   QCOMPARE(TestClass.convertWeekDay("thursday"), 4);
   QCOMPARE(TestClass.convertWeekDay("Friday"), 5);
   QCOMPARE(TestClass.convertWeekDay("saturday"), 6);
   QCOMPARE(TestClass.convertWeekDay("sunday"), 7);
   QCOMPARE(TestClass.convertWeekDay("notexistsday"), 0);
}

static TestWeekdayConverter TEST_WEEKDAYCONVERTER;
