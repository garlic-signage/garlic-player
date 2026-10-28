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
#include "tst_disc_space.hpp"

void noMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg){Q_UNUSED(type); Q_UNUSED(context); Q_UNUSED(msg)}

using ::testing::Exactly;
using ::testing::Return;

void TestDiscSpace::testInit()
{
    QString test_path      = "/test/path";
    qint64 total_bytes     = 100; // => 96 => 4 lockked
    qint64 available_bytes = 50;

    MyDiscSpace = new DiscSpace(&mockStorageInfo, nullptr);

    EXPECT_CALL(mockStorageInfo, setPath(test_path)).Times(Exactly(1));
    EXPECT_CALL(mockStorageInfo, bytesTotal()).Times(Exactly(2)).WillRepeatedly(Return(total_bytes));
    EXPECT_CALL(mockStorageInfo, bytesAvailable()).Times(Exactly(1)).WillOnce(Return(available_bytes));

    MyDiscSpace->init(test_path);

    QCOMPARE(MyDiscSpace->getBytesTotal(), 96);
    QCOMPARE(MyDiscSpace->getBytesLocked(), 4);
    QCOMPARE(MyDiscSpace->getBytesFree(), 46);
}

void TestDiscSpace::testRecalculate()
{
    qint64 available_bytes = 50;

    MyDiscSpace = new DiscSpace(&mockStorageInfo, nullptr);

    EXPECT_CALL(mockStorageInfo, bytesAvailable()).Times(Exactly(1)).WillOnce(Return(available_bytes));
    EXPECT_CALL(mockStorageInfo, refresh()).Times(Exactly(1));

    MyDiscSpace->recalculate();

    QCOMPARE(MyDiscSpace->getBytesTotal(), 0);
    QCOMPARE(MyDiscSpace->getBytesLocked(), 0);
    QCOMPARE(MyDiscSpace->getBytesFree(), 50);
}

static TestDiscSpace TEST_DISKSPACE;
