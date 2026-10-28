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

#include "tst_base.hpp"

void TestTBase::initTestCase()
{
    TestBase = new DerivedBase(nullptr);
}

void TestTBase::cleanupTestCase()
{
    delete TestBase;
}

void TestTBase::testDefault()
{
    QCOMPARE(TestBase->getID(), QString(""));
    QCOMPARE(TestBase->getTitle(), QString(""));
    QCOMPARE(TestBase->getClass(), QString(""));
    QCOMPARE(TestBase->getLang(), QString(""));

}

void TestTBase::testParseID()
{
    QDomDocument doc;
    QDomElement element = doc.createElement("test");
    element.setAttribute("id", "test_id");
    QCOMPARE(TBase::parseID(element), QString("test_id"));
}

void TestTBase::testParseIDWhenNOIDSetted()
{
    QDomDocument doc;
    QDomElement element = doc.createElement("test");
    QCOMPARE(TBase::parseID(element), QString("test_-1_-1"));
}

void TestTBase::testGetParentContainer()
{
    QCOMPARE(TestBase->getParentContainer(), nullptr);
}


void TestTBase::testPreloadParse()
{
    QDomDocument doc;
    QDomElement element = doc.createElement("test");
    TestBase->preloadParse(element);
    QCOMPARE(TestBase->getRootElement().tagName(), QString("test"));
}

void TestTBase::testSetBaseAttributesSMIL2()
{
    // test with old SMIL 2.0 id
    QDomDocument document;
    document.setContent(QString("<par id=\"old id\" title=\"this is a title\" class=\"this is a class\" xml:lang=\"el\" />"));
    QDomElement element = document.firstChild().toElement();
    QCOMPARE(element.tagName(), QString("par"));
    TestBase->setActiveElement(element);
    TestBase->test_setBaseAttributes();
    QCOMPARE(TestBase->getID(), QString("old id"));
    QCOMPARE(TestBase->getTitle(), QString("this is a title"));
    QCOMPARE(TestBase->getClass(), QString("this is a class"));
    QCOMPARE(TestBase->getLang(), QString("el"));
}

void TestTBase::testSetBaseAttributesSMIL3()
{
    // test with new SMIL 3.0 xml:id and rest of supporteed values

    QDomDocument document;
    document.setContent(QString("<seq id=\"old id\" xml:id=\"xml id\" title=\"test seq\" />"));
    QDomElement element = document.firstChild().toElement();
    QCOMPARE(element.tagName(), QString("seq"));
    TestBase->setActiveElement(element);
    TestBase->test_setBaseAttributes();
    QCOMPARE(TestBase->getID(), QString("xml id"));
    QCOMPARE(TestBase->getTitle(), QString("test seq"));
}

static TestTBase TEST_TBASE;
