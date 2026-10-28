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
#include <gtest/gtest.h>

/**
 * To create a testsuite like normal TestsUnits we use the approach of
 * https://alexhuszagh.github.io/2016/using-qttest-effectively/
 *
 * with some changes to use cmake and automatic including of test cases
 * instead of doing this manually
 *
 * @brief main
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    int status = 0;
    auto runTest = [&status, argc, argv](QObject* obj)
    {
        status |= QTest::qExec(obj, argc, argv);
    };

    // run suite
    auto &suite = BaseTestSuite::suite();
    for (auto it = suite.begin(); it != suite.end(); ++it)
    {
        runTest(*it);
    }

    return status;

}
