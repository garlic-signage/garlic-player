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
#include "base_testsuite.hpp"
#include <QtTest/QtTest>
#include <gmock/gmock.h>
#include "system_infos/disc_space.hpp"

using namespace SystemInfos;

class MockStorageInfo : public IStorageInfo
{
    public:
        MOCK_METHOD(quint64, bytesAvailable, (), (const, override));
        MOCK_METHOD(quint64, bytesTotal, (), (const, override));
        MOCK_METHOD(void, refresh, (), (override));
        MOCK_METHOD(void, setPath, (const QString &), (override));
};

class TestDiscSpace : public BaseTestSuite
{
    Q_OBJECT
    public:
        TestDiscSpace(){}
        QDir cache_dir;

    private Q_SLOTS:
        void testInit();
        void testRecalculate();

    private:
        DiscSpace *MyDiscSpace;
        MockStorageInfo mockStorageInfo;
};
