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
#include "tst_adapi_wrapper.hpp"
#include "smilparser/conditional/adapi_wrapper.hpp"

void TestAdapiWrapper::testReplaceSmilPlayerId()
{
    AdapiWrapper wrapper;
    QString result = wrapper.replaceAdapiFunctions("smil-playerId()");
    QCOMPARE(result, QString(""));
}

void TestAdapiWrapper::testReplaceSmilPlayerName()
{
    AdapiWrapper wrapper;
    QString result = wrapper.replaceAdapiFunctions("smil-playerName()");
    QCOMPARE(result, QString(""));
}

void TestAdapiWrapper::testReplaceDate()
{
    AdapiWrapper wrapper;
    QString currentDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    QString result = wrapper.replaceAdapiFunctions("adapi-date()");
    QCOMPARE(result, "'" + currentDate + "'");
}

void TestAdapiWrapper::testReplaceGmDate()
{
    AdapiWrapper wrapper;
    QString currentGmDate = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QString result = wrapper.replaceAdapiFunctions("adapi-gmdate()");
    QCOMPARE(result, "'" + currentGmDate + "'");
}

void TestAdapiWrapper::testReplaceWeekday()
{
    AdapiWrapper wrapper;
    int currentWeekday = QDateTime::currentDateTime().date().dayOfWeek();
    QString expectedWeekday = QString::number(currentWeekday == 7 ? 0 : currentWeekday);
    QString result = wrapper.replaceAdapiFunctions("adapi-weekday()");
    QCOMPARE(result, expectedWeekday);
}

void TestAdapiWrapper::testReplaceGmWeekday()
{
    AdapiWrapper wrapper;
    int gmWeekday = QDateTime::currentDateTimeUtc().date().dayOfWeek();
    QString expectedGmWeekday = QString::number(gmWeekday == 7 ? 0 : gmWeekday);
    QString result = wrapper.replaceAdapiFunctions("adapi-weekday()");
    QCOMPARE(result, expectedGmWeekday);
}

void TestAdapiWrapper::testReplaceCompare()
{
    AdapiWrapper wrapper;
    QString result = wrapper.replaceAdapiFunctions("adapi-compare");
    QCOMPARE(result, QString("compare"));
}

void TestAdapiWrapper::testReplaceEncoded()
{
    AdapiWrapper wrapper;
    QString result = wrapper.replaceAdapiFunctions("&gt; &lt;");
    QCOMPARE(result, QString("> <"));
}

static TestAdapiWrapper TEST_ADAPI_WRAPPER;


