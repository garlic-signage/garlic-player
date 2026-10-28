#include <QString>
#include <QtTest>
#include "db/inventory_table.h"

class I_InventoryTable : public DB::InventoryTable
{
    public:
        bool test_createTable(){return createTable();}
        bool test_openDbFile(){return openDbFile();}
        bool test_createDbFile(){return createDbFile();}
};

class TestInventoryTable : public QObject
{
        Q_OBJECT

    public:
        TestInventoryTable(){}

    private Q_SLOTS:
        void cleanup();
        void testReplace();
        void testUpdateFileStatus();
        void testDeletes();
        void testgetAllNotEmpty();
        void testgetAllEmpty();
        void testCreateTable();
        void testOpenDbFile();
        void testCreateDbFile();
};

void TestInventoryTable::cleanup()
{
    // prevent Warnings QSqlDatabasePrivate::addDatabase: duplicate connection name
    // must be first to deinitialize DB to remove file especially under Windows
    QSqlDatabase::removeDatabase("SQLITE");
    QFile file("./garlic.db");
    // needed for Windows
    file.setPermissions(QFile::WriteOwner | QFile::WriteUser | QFile::WriteOther);
    if (file.exists() && !file.remove())
        qDebug() << file.errorString();
}

void TestInventoryTable::testReplace()
{
    // work with no_empty.db is not possible, cause there comes a read-only message

    DB::InventoryTable MyTable;
    MyTable.init("./");
    DB::InventoryDataset dataset;
    dataset.resource_uri   = "remote_file_url";
    dataset.cache_name     = "local_file_info";
    dataset.content_type   = "content_type";
    dataset.content_length = 123456;
    dataset.last_update    = QDateTime::currentDateTime();
    dataset.expires        = QDateTime();
    dataset.state          = 99;
    MyTable.replace(dataset);
    DB::InventoryDataset result = MyTable.getByResourceURI("remote_file_url");
    QCOMPARE(result.state, 99);
}

void TestInventoryTable::testUpdateFileStatus()
{
    DB::InventoryTable MyTable;
    MyTable.init("./");
    DB::InventoryDataset dataset;
    dataset.resource_uri   = "remote_file_url";
    dataset.cache_name     = "local_file_info";
    dataset.content_type   = "content_type";
    dataset.content_length = 123456;
    dataset.last_update    = QDateTime::currentDateTime();
    dataset.expires        = QDateTime();
    dataset.state          = 99;
    MyTable.replace(dataset);
    MyTable.updateFileStatus("remote_file_url", 12);
    dataset = MyTable.getByResourceURI("remote_file_url");
    QCOMPARE(dataset.state, 12);

}

void TestInventoryTable::testDeletes()
{
    DB::InventoryTable MyTable;
    MyTable.init("./");

    DB::InventoryDataset dataset;
    dataset.resource_uri   = "remote_file_url";
    dataset.cache_name     = "local_file_info";
    dataset.content_type   = "content_type";
    dataset.content_length = 123456;
    dataset.last_update    = QDateTime::currentDateTime();
    dataset.expires        = QDateTime();
    dataset.state          = 11;
    MyTable.replace(dataset);
    dataset.resource_uri   = "remote_file_url2";
    dataset.cache_name     = "local_file_info2";
    dataset.content_type   = "content_type2";
    dataset.content_length = 1234567;
    dataset.last_update    = QDateTime::currentDateTime();
    dataset.expires        = QDateTime();
    dataset.state          = 22;
    MyTable.replace(dataset);

    MyTable.deleteByCacheName("notexisting2.jpg");
    MyTable.deleteByResourceURI("http://alsonotexisting.com/a.jpg");

    QList<DB::InventoryDataset> db_list = MyTable.getAll();
    QCOMPARE(db_list.size(), 2);
    MyTable.deleteByCacheName("local_file_info2");

    db_list = MyTable.getAll();
    QCOMPARE(db_list.size(), 1);

    MyTable.deleteByResourceURI("remote_file_url");
    db_list = MyTable.getAll();
    QCOMPARE(db_list.size(), 0);
}


void TestInventoryTable::testgetAllNotEmpty()
{
    QFile file(":/not_empty.db");
    file.copy("./garlic.db");

    DB::InventoryTable MyTable;
    MyTable.init("./");
    QList<DB::InventoryDataset> db_list = MyTable.getAll();

    QCOMPARE(db_list.size(), 14);
}

void TestInventoryTable::testgetAllEmpty()
{
    QFile file(":/empty.db");
    file.copy("./garlic.db");

    DB::InventoryTable MyTable;
    MyTable.init("./");
    QList<DB::InventoryDataset> db_list = MyTable.getAll();

    QCOMPARE(db_list.size(), 0);
}

void TestInventoryTable::testCreateTable()
{
    I_InventoryTable MyTable;
    MyTable.setDbPath("./");
    QTest::ignoreMessage(QtWarningMsg, "QSqlQuery::exec: database not open");
    QTest::ignoreMessage(QtCriticalMsg, "Inventory table could not be created \" \"");
    QVERIFY(!MyTable.test_createTable());

    MyTable.setDbPath("./");
    QVERIFY(MyTable.test_openDbFile());
    QVERIFY(MyTable.test_createTable());
    QFileInfo fi("./garlic.db");
    QVERIFY(fi.exists());
    QCOMPARE(fi.size(), 16384);
}

void TestInventoryTable::testOpenDbFile()
{
    I_InventoryTable MyTable;
    MyTable.setDbPath("/");

    QTest::ignoreMessage(QtCriticalMsg, "database file \"/garlic.db\" could not be created");
    QVERIFY(!MyTable.test_openDbFile());

    MyTable.setDbPath("./");
    QVERIFY(MyTable.test_openDbFile());
    QFileInfo fi("./garlic.db");
    QVERIFY(fi.exists());
    QCOMPARE(fi.size(), 0);
}


void TestInventoryTable::testCreateDbFile()
{
    I_InventoryTable MyTable;
    MyTable.setDbPath("/");
    QTest::ignoreMessage(QtCriticalMsg, "DB file could not be created");
    QVERIFY(!MyTable.test_createDbFile());

    MyTable.setDbPath("./");
    QVERIFY(MyTable.test_createDbFile());

    QFileInfo fi("./garlic.db");
    QVERIFY(fi.exists());
    QCOMPARE(fi.size(), 0);

}

QTEST_APPLESS_MAIN(TestInventoryTable)

#include "tst_inventorytable.moc"
