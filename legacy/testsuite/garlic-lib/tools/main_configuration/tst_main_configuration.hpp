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
#include <gmock/gmock.h>
#include "tools/i_settings.hpp"

class MockSettings : public ISettings
{
    public:
        MOCK_METHOD(QString, value, (const QString &key), (const, override));
        MOCK_METHOD(void, setValue, (const QString &key, const QVariant &value), (override));
        MOCK_METHOD(QSettings*, getOriginal, (), (const, override));
};

class TestMainConfiguration : public BaseTestSuite
{
        Q_OBJECT
    public:
        TestMainConfiguration(){}

    private Q_SLOTS:
        void test_init();
        void test_determineUserAgent();
        void test_determineBasePath();
        void test_validateContentUrl();
        void test_determineIndexUriWithParameter();
        void test_determineIndexUriNoParam();
        void test_determineIndexUriEmpty();

    private:
        MockSettings      mockSettings;

};
