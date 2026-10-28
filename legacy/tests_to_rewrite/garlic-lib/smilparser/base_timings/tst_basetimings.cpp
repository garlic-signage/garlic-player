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

#include "smilparser/base_timings.h"

class InhertitedBaseTimings : public BaseTimings
{
    Q_OBJECT
public:
    InhertitedBaseTimings(QObject * parent = 0){Q_UNUSED(parent);setObjectName("TestBaseTimings");}
    QString       getType(){return "test timings for elements";}
    void          preloadParse(QDomElement element) {root_element = element;}
    void          emitfinished(){emit signal_finished();}
    void          play(){status = _playing;}                // virtual
    void          pause(){status = _paused;}                // virtual
    void          stop(){status = _stopped;}                // virtual
    void          resume(){status = _playing;}              // virtual
    QString       getBaseType(){return  "test base";}       // virtual
    BaseTimings  *getChildElementFromList(){ return this;}  // virtual
    bool          hasPlayingChilds(){return true;}          // virtual

    // for Test
    void          setBeginTimer(){ BeginTimer = new Timings::EnhancedTimer(this);}
    void          setEndTimer(){ EndTimer = new Timings::EnhancedTimer(this);}
    void          setDurTimer(){ DurTimer = new Timings::SimpleTimer(this);}
    // set indefinite for gettin true when ask for resume
    void          setBeginTimerResume(){ BeginTimer->parse("");}
    void          startEndTimer(){ EndTimer->parse("10s"); EndTimer->start();}
    void          startDurTimer(){ DurTimer->parse("5s"); DurTimer->start();}

    bool          isResumed(){ return is_resumed;}

    void          test_setBaseAttributes(){setBaseAttributes();}
    bool          test_checkRepeatCountStatus(){return checkRepeatCountStatus();}
    void          setForRepeatCountCheck(int rC, int r_c, bool in){repeatCount=rC;internal_count=r_c;indefinite=in;}
    int           getRepeatCount(){return repeatCount;}
    bool          getIndefinite(){return indefinite;}
protected:
    void       prepareDurationTimerBeforePlay() // behaves like TImage and TWeb
    {
        play();
        emit signal_prepareDuration();
    }
signals:
    void signal_prepareDuration();
    void signal_finished();
};


class TestTBaseTiming : public QObject
{
    Q_OBJECT

public:
    TestTBaseTiming(){}

private Q_SLOTS:
    void test_checkRepeatCountStatus();
    void test_prepareTimingsBeforePlayingWithOutTimer();
    void test_prepareTimingsBeforePlayingWithBeginTimer();
    void test_prepareTimingsBeforePlayingWithEndTimer();
    void test_prepareTimingsBeforeResumeWithOutTimer();
    void test_prepareTimingsBeforeResumeWithBeginTimer();
    void test_prepareTimingsBeforeResumeWithEndTimer();
    void test_finishedNotFound();
    void test_finishedNotFoundNothing();
    void test_finishedSimpleDuration();
    void test_finishedSimpleDurationWithEndTimer();
};

void TestTBaseTiming::test_prepareTimingsBeforePlayingWithOutTimer()
{
    InhertitedBaseTimings *MyTest = new InhertitedBaseTimings(NULL);
    qRegisterMetaType<QString>();
    QSignalSpy spy(MyTest, SIGNAL(signal_prepareDuration()));
    QVERIFY(spy.isValid());

    MyTest->prepareTimingsBeforePlaying();
    QCOMPARE(MyTest->getStatus(),  MyTest->_playing);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(MyTest->isResumed(), false);
}

void TestTBaseTiming::test_prepareTimingsBeforePlayingWithBeginTimer()
{
    InhertitedBaseTimings *MyTest = new InhertitedBaseTimings(NULL);
    MyTest->setBeginTimer();
    qRegisterMetaType<QString>();
    QSignalSpy spy(MyTest, SIGNAL(signal_prepareDuration()));
    QVERIFY(spy.isValid());

    MyTest->prepareTimingsBeforePlaying();
    QCOMPARE(MyTest->getStatus(),  MyTest->_waiting);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(MyTest->isResumed(), false);
}

void TestTBaseTiming::test_prepareTimingsBeforePlayingWithEndTimer()
{
    InhertitedBaseTimings *MyTest = new InhertitedBaseTimings(NULL);
    MyTest->setEndTimer();
    qRegisterMetaType<QString>();
    QSignalSpy spy(MyTest, SIGNAL(signal_prepareDuration()));
    QVERIFY(spy.isValid());

    MyTest->prepareTimingsBeforePlaying();
    QCOMPARE(MyTest->getStatus(),  MyTest->_playing);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(MyTest->isResumed(), false);
}

void TestTBaseTiming::test_prepareTimingsBeforeResumeWithOutTimer()
{
    InhertitedBaseTimings *MyTest = new InhertitedBaseTimings(NULL);
    qRegisterMetaType<QString>();
    QSignalSpy spy1(MyTest, SIGNAL(signal_prepareDuration()));
    QSignalSpy spy2(MyTest, SIGNAL(signal_finished()));
    QVERIFY(spy1.isValid());
    QVERIFY(spy2.isValid());

    MyTest->prepareTimingsBeforeResume();
    QCOMPARE(MyTest->getStatus(),  MyTest->_playing);
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(MyTest->isResumed(), true);
}

void TestTBaseTiming::test_prepareTimingsBeforeResumeWithBeginTimer()
{
    InhertitedBaseTimings *MyTest = new InhertitedBaseTimings(NULL);
    MyTest->setBeginTimer();
    MyTest->setBeginTimerResume();
    qRegisterMetaType<QString>();
    QSignalSpy spy1(MyTest, SIGNAL(signal_prepareDuration()));
    QSignalSpy spy2(MyTest, SIGNAL(signal_finished()));
    QVERIFY(spy1.isValid());
    QVERIFY(spy2.isValid());

    MyTest->prepareTimingsBeforeResume();
    QCOMPARE(MyTest->getStatus(),  MyTest->_waiting);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
    QCOMPARE(MyTest->isResumed(), true);
}

void TestTBaseTiming::test_prepareTimingsBeforeResumeWithEndTimer()
{
    InhertitedBaseTimings *MyTest = new InhertitedBaseTimings(NULL);
    MyTest->setEndTimer();
    qRegisterMetaType<QString>();
    QSignalSpy spy1(MyTest, SIGNAL(signal_prepareDuration()));
    QSignalSpy spy2(MyTest, SIGNAL(signal_finished()));
    QVERIFY(spy1.isValid());
    QVERIFY(spy2.isValid());

    MyTest->prepareTimingsBeforeResume();
    QCOMPARE(MyTest->getStatus(), MyTest->_stopped);
    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 1);
    QCOMPARE(MyTest->isResumed(), true);
}

void TestTBaseTiming::test_finishedNotFound()
{
    InhertitedBaseTimings *MyTest = new InhertitedBaseTimings(NULL);
    MyTest->setEndTimer();
    MyTest->startEndTimer();
    MyTest->setDurTimer();
    MyTest->startDurTimer();
    qRegisterMetaType<QString>();
    QSignalSpy spy1(MyTest, SIGNAL(signal_prepareDuration()));
    QSignalSpy spy2(MyTest, SIGNAL(signal_finished()));
    QVERIFY(spy1.isValid());
    QVERIFY(spy2.isValid());

    MyTest->finishedNotFound();
    QCOMPARE(spy1.count(), 0);
    int i = 0;
    while (spy2.count() == 0 && i < 300)
    {
         QTest::qWait(200);
         i +=200;
    }

    QCOMPARE(spy2.count(), 1);
}

void TestTBaseTiming::test_finishedNotFoundNothing()
{
    InhertitedBaseTimings *MyTest = new InhertitedBaseTimings(NULL);
    qRegisterMetaType<QString>();
    QSignalSpy spy1(MyTest, SIGNAL(signal_prepareDuration()));
    QSignalSpy spy2(MyTest, SIGNAL(signal_finished()));
    QVERIFY(spy1.isValid());
    QVERIFY(spy2.isValid());
    MyTest->finishedNotFound();

    int i = 0;
    while (spy2.count() == 0 && i < 300)
    {
         QTest::qWait(200);
         i +=200;
    }

    QCOMPARE(spy1.count(), 0);
    QCOMPARE(spy2.count(), 0);
}

void TestTBaseTiming::test_finishedSimpleDuration()
{
    InhertitedBaseTimings *MyTest = new InhertitedBaseTimings(NULL);
    qRegisterMetaType<QString>();
    QSignalSpy spy(MyTest, SIGNAL(signal_finished()));
    QVERIFY(spy.isValid());
    MyTest->finishedSimpleDuration();
    QCOMPARE(spy.count(), 1);
}

void TestTBaseTiming::test_finishedSimpleDurationWithEndTimer()
{
    InhertitedBaseTimings *MyTest = new InhertitedBaseTimings(NULL);
    MyTest->setEndTimer();
    MyTest->startEndTimer();
    qRegisterMetaType<QString>();
    QSignalSpy spy(MyTest, SIGNAL(signal_finished()));
    QVERIFY(spy.isValid());
    MyTest->finishedSimpleDuration();
    int i = 0;
    while (spy.count() == 0 && i < 400)
    {
         QTest::qWait(200);
         i +=200;
    }

    QCOMPARE(spy.count(), 0);
}



/*
    Todo later This parts should go in the test for timers
    QDomDocument document;

    document.setContent(QString("<img id=\"test1\" src=\"\" />"));
    QDomElement element = document.firstChild().toElement();
    QCOMPARE(element.tagName(), QString("img"));

     document.setContent(QString("<img xml:id=\"test2\" begin=\"2s\" dur=\"1s\" end=\"4s\"/>"));
    element = document.firstChild().toElement();
    QCOMPARE(element.tagName(), QString("img"));
    // start, then pause and look on the remaining times
    InhertitedBaseTimings *MyTest2 = new InhertitedBaseTimings(NULL);
    MyTest2->preloadParse(element);
    MyTest2->prepareTimingsBeforePlaying();
    MyTest2->prepareTimingsBeforePausing();
    QCOMPARE(MyTest2->getID(), QString("test2"));
    QVERIFY(MyTest2->getRemainingBegin() > 1950); // Test with tolerances value should be 2000
    QVERIFY(MyTest2->getRemainingBegin() < 2050);
    QVERIFY(MyTest2->getRemainingEnd() > 3950); // Test with tolerances value should be 4000
    QVERIFY(MyTest2->getRemainingEnd() < 4050);
    QCOMPARE(MyTest2->getRemainingDur(), int(0));

    document.setContent(QString("<img xml:id =\"test3\" dur=\"2s\" end=\"4s\"/>"));
    element = document.firstChild().toElement();
    QCOMPARE(element.tagName(), QString("img"));
    // start, then pause and look on the remaining times
    InhertitedBaseTimings *MyTest3 = new InhertitedBaseTimings(NULL);
    MyTest3->preloadParse(element);
    MyTest3->prepareTimingsBeforePlaying();
    QCOMPARE(MyTest3->getID(), QString("test3"));
    QCOMPARE(MyTest3->getRemainingBegin(), int(0));
    QCOMPARE(MyTest3->getStatus(),  MyTest3->_playing);

    MyTest3->prepareTimingsBeforePausing();
    QVERIFY(MyTest3->getRemainingEnd() > 3950); // Test with tolerances value should be 4000
    QVERIFY(MyTest3->getRemainingEnd() < 4050);

    QVERIFY(MyTest3->getRemainingDur() > 1950); // Test with tolerances value should be 2000
    QVERIFY(MyTest3->getRemainingDur() < 2050);
*/


void TestTBaseTiming::test_checkRepeatCountStatus()
{
    // test with default
    InhertitedBaseTimings MyContainer;
    MyContainer.setForRepeatCountCheck(2, 0, true);
    QCOMPARE(MyContainer.test_checkRepeatCountStatus(), true);
    MyContainer.setForRepeatCountCheck(2, 0, false);
    QCOMPARE(MyContainer.test_checkRepeatCountStatus(), true);
    MyContainer.setForRepeatCountCheck(2, 2, false);
    QCOMPARE(MyContainer.test_checkRepeatCountStatus(), false);
    MyContainer.setForRepeatCountCheck(2, 1, false);
    QCOMPARE(MyContainer.test_checkRepeatCountStatus(), true);
    return;
}


//      document.setContent(QString("<img src=\"data/images/0001.jpg\" min=\"2s\" begin=\"0s\" dur=\"1s\" end=\"3s\"/>"));
//      document.setContent(QString("<img src=\"data/images/0001.jpg\" max=\"2s\" begin=\"0s\" dur=\"1s\" end=\"3s\"/>"));


QTEST_MAIN(TestTBaseTiming)

#include "tst_basetimings.moc"
