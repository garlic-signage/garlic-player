#include <QString>
#include <QtTest>
#include <gmock/gmock.h>
#include "files/free_disc_space.h"

using ::testing::DefaultValue;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::Return;

void noMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg){Q_UNUSED(type); Q_UNUSED(context); Q_UNUSED(msg)}


class DiscSpaceMocked : public SystemInfos::DiscSpace
{
    public:
        DiscSpaceMocked() : DiscSpace(nullptr) {}
        MOCK_METHOD(void,    init, (QString path));
        MOCK_METHOD(void,    recalculate, ());
        MOCK_METHOD(quint64, getBytesFree, ());
        MOCK_METHOD(quint64, getBytesTotal, ());
        MOCK_METHOD(quint64, getBytesLocked, ());
    protected:
        MOCK_METHOD(quint64, calculateFreeBytes, ());
};


class TestDiscSpace : public QObject
{
    Q_OBJECT

public:
    TestDiscSpace(){}
    QDir cache_dir;

private Q_SLOTS:
    void init();
    void cleanup();
    void testCalculateNeededDiscSpaceToFree();
    void testfreeDiscSpaceOldestFile();
    void testFreeDiscSpaceToMuch();
    void testClearPlayerCache();
    void testClearWebCache();
    void createFilesForLastReadDelete();
    void createFilesForDelete();
    void openCloseForLastRead(QString source, QString copy);
    int countFilesInDir(QFileInfo fi);
};

void TestDiscSpace::init()
{
    qInstallMessageHandler(noMessageOutput);
    cache_dir.setPath("./");
    if (cache_dir.mkpath("cache/test_dir"))
    {
        cache_dir.mkpath("cache/QtWebEngine");
        cache_dir.setPath("./cache");
    }
}

void TestDiscSpace::cleanup()
{
   cache_dir.setPath("./cache"); // for cases whem test breaks and did nor cleanup correct
   cache_dir.removeRecursively();
}

void TestDiscSpace::testCalculateNeededDiscSpaceToFree()
{
    NiceMock<DiscSpaceMocked>  MyDiscSpaceMock;
    FreeDiscSpace *MyFreeDiscSpace = new FreeDiscSpace(&MyDiscSpaceMock);

    EXPECT_CALL(MyDiscSpaceMock, getBytesFree).WillRepeatedly(Return(10000));

    QCOMPARE(MyFreeDiscSpace->calculateNeededDiscSpaceToFree(11000), 1010);
    QCOMPARE(MyFreeDiscSpace->calculateNeededDiscSpaceToFree(9568), 0);
    QCOMPARE(MyFreeDiscSpace->calculateNeededDiscSpaceToFree(9991), 1);
}

void TestDiscSpace::testfreeDiscSpaceOldestFile()
{
    createFilesForLastReadDelete();

    NiceMock<DiscSpaceMocked>  MyDiscSpaceMock;
    FreeDiscSpace *MyFreeDiscSpace = new FreeDiscSpace(&MyDiscSpaceMock);
    MyFreeDiscSpace->init(cache_dir.path());

    QTest::qWait(800); // needed for inits
    QVERIFY(MyFreeDiscSpace->freeDiscSpace(630));

    QVERIFY(!QFile::exists("./cache/test_dir/1.png"));
}

void TestDiscSpace::testFreeDiscSpaceToMuch()
{
    createFilesForDelete();

    NiceMock<DiscSpaceMocked>  MyDiscSpaceMock;
    FreeDiscSpace *MyFreeDiscSpace = new FreeDiscSpace(&MyDiscSpaceMock);
    MyFreeDiscSpace->init(cache_dir.path());
    QTest::qWait(800); // needed for inits

    // Free more than possible
    QVERIFY(!MyFreeDiscSpace->freeDiscSpace(200000));

    QVERIFY(!QFile::exists("./cache/test_dir/1.png"));
    QVERIFY(!QFile::exists("./cache/test_dir/1.wgt"));
    QVERIFY(!QFile::exists("./cache/1.png"));
    QVERIFY(!QFile::exists("./cache/1.wgt"));
    QVERIFY(!QFile::exists("./cache/2.png"));
    QVERIFY(!QFile::exists("./cache/2.wgt"));
    QVERIFY(!QFile::exists("./cache/3.png"));
    QVERIFY(!QFile::exists("./cache/4.png"));
    QVERIFY(!QFile::exists("./cache/5.png"));

    // This tests fails sometimes because of the files will not be deleted
    // maybe the os cache makes some crap
}

void TestDiscSpace::testClearPlayerCache()
{
    createFilesForDelete();
    NiceMock<DiscSpaceMocked>  MyDiscSpaceMock;
    FreeDiscSpace *MyFreeDiscSpace = new FreeDiscSpace(&MyDiscSpaceMock);
    MyFreeDiscSpace->init(cache_dir.path());
    QTest::qWait(800); // needed for inits

    MyFreeDiscSpace->clearPlayerCache();
    QVERIFY(QFile::exists("./cache/QtWebEngine"));
    QVERIFY(!QFile::exists("./cache/test_dir/1.png"));
    QVERIFY(!QFile::exists("./cache/test_dir/1.wgt"));
    QVERIFY(!QFile::exists("./cache/1.png"));
    QVERIFY(!QFile::exists("./cache/1.wgt"));
    QVERIFY(!QFile::exists("./cache/2.png"));
    QVERIFY(!QFile::exists("./cache/2.wgt"));
    QVERIFY(!QFile::exists("./cache/3.png"));
    QVERIFY(!QFile::exists("./cache/4.png"));
    QVERIFY(!QFile::exists("./cache/5.png"));
}


void TestDiscSpace::testClearWebCache()
{
    createFilesForDelete();
    NiceMock<DiscSpaceMocked>  MyDiscSpaceMock;
    FreeDiscSpace *MyFreeDiscSpace = new FreeDiscSpace(&MyDiscSpaceMock);
    MyFreeDiscSpace->init(cache_dir.path());

    QVERIFY(QFile::exists("./cache/QtWebEngine"));

    MyFreeDiscSpace->clearWebCache();

    QVERIFY(!QFile::exists("./cache/QtWebEngine"));
    QVERIFY(QFile::exists("./cache/test_dir/1.png"));
    QVERIFY(QFile::exists("./cache/test_dir/1.wgt"));
    QVERIFY(QFile::exists("./cache/1.png"));
    QVERIFY(QFile::exists("./cache/1.wgt"));
    QVERIFY(QFile::exists("./cache/2.png"));
    QVERIFY(QFile::exists("./cache/2.wgt"));
    QVERIFY(QFile::exists("./cache/3.png"));
    QVERIFY(QFile::exists("./cache/4.png"));
    QVERIFY(QFile::exists("./cache/5.png"));
}

void TestDiscSpace::createFilesForLastReadDelete()
{
    // put two files in the testdir
    openCloseForLastRead(":/test.png", "./cache/test_dir/1.png"); // => 629 Byte
    openCloseForLastRead(":/test.wgt", "./cache/test_dir/1.wgt"); // without directory => 1118
    QTest::qWait(800); // needed for set as oldest read

    openCloseForLastRead(":/test.wgt", "./cache/2.wgt");
    openCloseForLastRead(":/test.png", "./cache/5.png");
    openCloseForLastRead(":/test.png", "./cache/4.png");
    openCloseForLastRead(":/test.png", "./cache/3.png");
    openCloseForLastRead(":/test.png", "./cache/2.png");
    openCloseForLastRead(":/test.png", "./cache/1.png");
    openCloseForLastRead(":/test.wgt", "./cache/1.wgt");
    openCloseForLastRead(":/test.png", "./cache/index.smil");
}


void TestDiscSpace::createFilesForDelete()
{
    // put two files in the testdir
    openCloseForLastRead(":/test.png", "./cache/test_dir/1.png"); // => 629 Byte
    openCloseForLastRead(":/test.wgt", "./cache/test_dir/1.wgt"); // without directory => 1118
    openCloseForLastRead(":/test.wgt", "./cache/2.wgt");
    openCloseForLastRead(":/test.png", "./cache/5.png");
    openCloseForLastRead(":/test.png", "./cache/4.png");
    openCloseForLastRead(":/test.png", "./cache/3.png");
    openCloseForLastRead(":/test.png", "./cache/2.png");
    openCloseForLastRead(":/test.png", "./cache/1.png");
    openCloseForLastRead(":/test.wgt", "./cache/1.wgt");
    openCloseForLastRead(":/test.png", "./cache/index.smil");

}

void TestDiscSpace::openCloseForLastRead(QString source, QString destination)
{
    QFile original(source);
    original.copy(destination); // without directory => 1118
    QTest::qWait(10); // to get different values for lastRead
    QFile copy(destination);
    copy.open(QIODevice::ReadOnly);
    copy.close();
}

int TestDiscSpace::countFilesInDir(QFileInfo fi)
{
    QDir dir(fi.absoluteDir());
    QStringList totalfiles;
    totalfiles = dir.entryList(QStringList("*"), QDir::Files | QDir::NoSymLinks);
    return totalfiles.length();
}


QTEST_APPLESS_MAIN(TestDiscSpace)

#include "tst_free_disc_space.moc"
