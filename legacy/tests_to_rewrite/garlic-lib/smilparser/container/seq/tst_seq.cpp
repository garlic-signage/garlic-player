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
#include "smilparser/seq.h"

class InhertitedTSeq : public TSeq
{
    Q_OBJECT
public:
    InhertitedTSeq(TContainer *pc, QObject *parent = 0)  : TSeq(pc, parent)
    {
        Q_UNUSED(parent);setObjectName("TSeq");
    }
    QDomElement test_getNextItem(){return getNextItem();}
    bool        test_canGetNextItem(){return canGetNextItem();}
};


class TestTSeq : public QObject
{
    Q_OBJECT

public:
    TestTSeq(){}

private Q_SLOTS:
    void testEmptyWithDuration();
    void testNormalPlay();
    void testRepeatedPlay();
};


void TestTSeq::testEmptyWithDuration()
{
    QDomDocument document;
    document.setContent(QString("<seq xml:id=\"seq_time\" dur=\"0.5\" />"));
    QDomElement element = document.firstChild().toElement();
    TSeq       *MySeq   = new TSeq(Q_NULLPTR);
    MySeq->parse(element);
    MySeq->prepareTimerBeforePlaying();
    MySeq->play();
    QSignalSpy spy1(MySeq, SIGNAL(finishedContainer(TContainer * , TBaseTiming *)));
    int i = 0;
    while (spy1.count() == 0  && i < 2000)
    {
        QTest::qWait(100);
        i += 100;
    }
    QCOMPARE(1, spy1.count());
    QVERIFY(i > 499);
    QVERIFY(i < 601);
    return;
}

void TestTSeq::testNormalPlay()
{
    QDomDocument document;
    document.setContent(QString("<seq xml:id=\"normal\"><img xml:id=\"img0001\" /><img xml:id=\"img0002\" /><img xml:id=\"img0003\" /><img xml:id=\"img0004\" /><img xml:id=\"img0005\" /></seq>"));
    QDomElement     element = document.firstChild().toElement();
    InhertitedTSeq *MySeq   = new InhertitedTSeq(Q_NULLPTR);
    MySeq->parse(element);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0001"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0002"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0003"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0004"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0005"));
    QCOMPARE(MySeq->test_canGetNextItem(), false);
    QSignalSpy spy1(MySeq, SIGNAL(finishedContainer(TContainer *, TBaseTiming *)));
    MySeq->next(MySeq);
    int i = 0;
    while (spy1.count() == 0 && i < 2000)
    {
        QTest::qWait(100);
        i += 100;
    }
    QCOMPARE(1, spy1.count());
    return;
}

void TestTSeq::testRepeatedPlay()
{
    QDomDocument document;
    document.setContent(QString("<seq xml:id=\"repeat\" repeatCount=\"3\"><img xml:id=\"img0001\" /><img xml:id=\"img0002\" /><img xml:id=\"img0003\" /><img xml:id=\"img0004\" /><img xml:id=\"img0005\" /></seq>"));
    QDomElement     element = document.firstChild().toElement();
    InhertitedTSeq *MySeq   = new InhertitedTSeq(Q_NULLPTR);
    MySeq->parse(element);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0001"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0002"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0003"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0004"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0005"));
    QCOMPARE(MySeq->test_canGetNextItem(), false);
    MySeq->next(MySeq);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0002"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0003"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0004"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0005"));
    QCOMPARE(MySeq->test_canGetNextItem(), false);
    MySeq->next(MySeq);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0002"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0003"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0004"));
    QCOMPARE(MySeq->test_canGetNextItem(), true);
    QCOMPARE(MySeq->parseID(MySeq->test_getNextItem()), QString("img0005"));
    QCOMPARE(MySeq->test_canGetNextItem(), false);
    QSignalSpy spy1(MySeq, SIGNAL(finishedContainer(TContainer *, TBaseTiming *)));
    MySeq->next(MySeq);
    int i = 0;
    while (spy1.count() == 0 && i < 2000)
    {
        QTest::qWait(100);
        i += 100;
    }
    QCOMPARE(1, spy1.count());
    return;
}


QTEST_MAIN(TestTSeq)

#include "tst_seq.moc"
