#include <QString>
#include <QtTest>
#include "smilparser/tools/shuffle.h"

class TestTShuffle : public QObject
{
    Q_OBJECT

public:
    TestTShuffle(){}

private Q_SLOTS:
    void testWithOnePickPerRound();
    void testWithThreePickPerRound();
    QList<QDomElement> createSeqList();
};


void TestTShuffle::testWithOnePickPerRound()
{
    TShuffle *MyShuffle = new TShuffle(createSeqList());
    QDomDocument document;
    document.setContent(QString("<metadata><meta name=\"adapi:pickingAlgorithm\" content=\"shuffle\"/><meta name=\"adapi:pickingBehavior\" content=\"pickN\"/><meta name=\"adapi:pickNumber\" content=\"1\"/></metadata>"));
    MyShuffle->parse(document.firstChild().toElement());
    QCOMPARE(1, MyShuffle->getInternalPick());
    QCOMPARE(MyShuffle->canGetNextItem(), true);
    QCOMPARE(1, MyShuffle->getInternalPick());
    QDomElement element1 = MyShuffle->getNextItem();
    QCOMPARE(0, MyShuffle->getInternalPick());
    QCOMPARE(false, MyShuffle->canGetNextItem());
    QCOMPARE(1, MyShuffle->getInternalPick());
    QCOMPARE(true, MyShuffle->canGetNextItem());
    QDomElement element2 = MyShuffle->getNextItem();
    QCOMPARE(0, MyShuffle->getInternalPick());
    QCOMPARE(false, MyShuffle->canGetNextItem());
    QCOMPARE(1, MyShuffle->getInternalPick());
    QCOMPARE(true, MyShuffle->canGetNextItem());
    QDomElement element3 = MyShuffle->getNextItem();
    QCOMPARE(0, MyShuffle->getInternalPick());
    QCOMPARE(false, MyShuffle->canGetNextItem());
    QCOMPARE(1, MyShuffle->getInternalPick());
    QCOMPARE(true, MyShuffle->canGetNextItem());
    QDomElement element4 = MyShuffle->getNextItem();
    QCOMPARE(0, MyShuffle->getInternalPick());
    QCOMPARE(false, MyShuffle->canGetNextItem());
    QCOMPARE(1, MyShuffle->getInternalPick());
    QCOMPARE(true, MyShuffle->canGetNextItem());
    QDomElement element5 = MyShuffle->getNextItem();
    QCOMPARE(0, MyShuffle->getInternalPick());
    QCOMPARE(false, MyShuffle->canGetNextItem());
    QCOMPARE(1, MyShuffle->getInternalPick());
    QCOMPARE(true, MyShuffle->canGetNextItem());
    QDomElement element6 = MyShuffle->getNextItem();
    QCOMPARE(0, MyShuffle->getInternalPick());
    QCOMPARE(false, MyShuffle->canGetNextItem());
    QCOMPARE(1, MyShuffle->getInternalPick());
    QCOMPARE(true, MyShuffle->canGetNextItem());
    // check if all elements are played in this pass
    QVERIFY(element1 != element2 && element1 != element3  && element1 != element4  && element1 != element5);
    QVERIFY(element2 != element1 && element2 != element3  && element2 != element4  && element2 != element5);
    QVERIFY(element3 != element1 && element3 != element2  && element3 != element4  && element3 != element5);
    QVERIFY(element4 != element1 && element4 != element2  && element4 != element3  && element2 != element5);
    QVERIFY(element5 != element1 && element5 != element2  && element5 != element3  && element5 != element4);
    QVERIFY(element1 == element6);
    return;
}

void TestTShuffle::testWithThreePickPerRound()
{
    TShuffle *MyShuffle = new TShuffle(createSeqList());
    QDomDocument document;
    document.setContent(QString("<metadata><meta name=\"adapi:pickingAlgorithm\" content=\"shuffle\"/><meta name=\"adapi:pickingBehavior\" content=\"pickN\"/><meta name=\"adapi:pickNumber\" content=\"3\"/></metadata>"));
    MyShuffle->parse(document.firstChild().toElement());
    QCOMPARE(3, MyShuffle->getInternalPick());
    QCOMPARE(MyShuffle->canGetNextItem(), true);
    MyShuffle->getNextItem();
    QCOMPARE(2, MyShuffle->getInternalPick());
    QCOMPARE(MyShuffle->canGetNextItem(), true);
    MyShuffle->getNextItem();
    QCOMPARE(1, MyShuffle->getInternalPick());
    QCOMPARE(MyShuffle->canGetNextItem(), true);
    MyShuffle->getNextItem();
    QCOMPARE(0, MyShuffle->getInternalPick());
    QCOMPARE(MyShuffle->canGetNextItem(), false);
    QCOMPARE(3, MyShuffle->getInternalPick());
    MyShuffle->getNextItem();
    QCOMPARE(MyShuffle->canGetNextItem(), true);
    QCOMPARE(2, MyShuffle->getInternalPick());
    MyShuffle->getNextItem();
    QCOMPARE(1, MyShuffle->getInternalPick());
    QCOMPARE(MyShuffle->canGetNextItem(), true);
    MyShuffle->getNextItem();
    QCOMPARE(0, MyShuffle->getInternalPick());
    QCOMPARE(MyShuffle->canGetNextItem(), false);
    QCOMPARE(3, MyShuffle->getInternalPick());
    MyShuffle->getNextItem();
    QCOMPARE(MyShuffle->canGetNextItem(), true);
    return;
}

QList<QDomElement> TestTShuffle::createSeqList()
{
    QList<QDomElement> list;
    QDomDocument document;
    document.setContent(QString("<img xml:id=\"img0001\" />"));
    list.append(document.firstChild().toElement());
    document.setContent(QString("<img xml:id=\"img0002\" />"));
    list.append(document.firstChild().toElement());
    document.setContent(QString("<img xml:id=\"img0003\" />"));
    list.append(document.firstChild().toElement());
    document.setContent(QString("<img xml:id=\"img0004\" />"));
    list.append(document.firstChild().toElement());
    document.setContent(QString("<img xml:id=\"img0005\" />"));
    list.append(document.firstChild().toElement());
    return list;
}

QTEST_APPLESS_MAIN(TestTShuffle)

#include "tst_shuffle.moc"
