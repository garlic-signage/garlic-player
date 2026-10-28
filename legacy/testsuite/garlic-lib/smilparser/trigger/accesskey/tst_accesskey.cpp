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
#include "tst_accesskey.hpp"


void TestAccessKey::testParse_data()
{
	QTest::addColumn<QString>("input");
	QTest::addColumn<bool>("expectedResult");
	QTest::addColumn<QChar>("expectedCharSymbol");

	QTest::newRow("Valid character 'a'") << "accesskey(a)" << true << 'a';
	QTest::newRow("Valid character '7'") << "accesskey(7)" << true << '7';
	QTest::newRow("Invalid empty input") << "" << false << QChar();
	QTest::newRow("Invalid multi-character input") << "accesskey(ab)" << false << QChar();
	QTest::newRow("Invalid non-alphanumeric character input") << "accesskey($" << false << QChar();
}

void TestAccessKey::testParse()
{
	QFETCH(QString, input);
	QFETCH(bool, expectedResult);
	QFETCH(QChar, expectedCharSymbol);

	AccessKey accessKey;
	bool result = accessKey.parse(input);

	QCOMPARE(result, expectedResult);
	if (result) {
		QCOMPARE(accessKey.getCharSymbol(), expectedCharSymbol);
	}
}
