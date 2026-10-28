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
#include <QString>
#include <QtTest>

#include "smilparser/element_factory.h"

/**
 * @brief To save a test file this will test TTiming and TClockvalue which is inherited from TTiming
 */

class TestElementFactory : public QObject
{
    Q_OBJECT
public:
    TestElementFactory(){}

private Q_SLOTS:
    void test_createBase();
    void test_createBaseForRef();
};

void TestElementFactory::test_createBase()
{
    QDomDocument  document;
    QDomElement   element;
    BaseTimings  *MyBase;
    element = document.createElement(QString("img"));
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TImage");

    element = document.createElement(QString("video"));
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TVideo");

    element = document.createElement(QString("audio"));
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TAudio");

    element = document.createElement(QString("text"));
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TWeb");

    element = document.createElement(QString("seq"));
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TSeq");

    element = document.createElement(QString("par"));
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TPar");

    element = document.createElement(QString("excl"));
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TExcl");

    element = document.createElement(QString("prefetch"));
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TPrefetch");

/*    element = document.createElement(QString("body"));
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TBody");
*/
    element = document.createElement(QString("heidewitzka"));
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "Unknown");
}

void TestElementFactory::test_createBaseForRef()
{
    QDomDocument document;
    QDomElement element = document.createElement(QString("ref"));
    QCOMPARE(element.tagName(), QString("ref"));
    BaseTimings  *MyBase;

    element.setAttribute("type", "image/png");
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TImage");

    element.setAttribute("type", "video/mp4");
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TVideo");

    element.setAttribute("type", "audio/wav");
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TAudio");

    element.setAttribute("type", "text/html");
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TWeb");

    element.setAttribute("type", "application/widget");
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "TWidget");

    element.setAttribute("type", "dsbfkdsh/ewjd");
    MyBase  = ElementFactory::createBase(element, Q_NULLPTR, Q_NULLPTR);
    QCOMPARE(MyBase->objectName(), "Unknown");
}

QTEST_APPLESS_MAIN(TestElementFactory)

#include "tst_element_factory.moc"
