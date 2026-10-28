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

#include "garlic-lib/smilparser/base/derived_base.hpp"

class TestTBase : public BaseTestSuite
{
        Q_OBJECT

    public:
        TestTBase(){}

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void testDefault();
        void testParseID();
        void testParseIDWhenNOIDSetted();
        void testGetParentContainer();
        void testPreloadParse();
        void testSetBaseAttributesSMIL2();
        void testSetBaseAttributesSMIL3();

    private:
        DerivedBase *TestBase;
};

