#include <QString>
#include <QtTest>
#include "files/base_manager.h"

class I_BaseManager : public Files::BaseManager
{
        Q_OBJECT
    public:
        I_BaseManager(MainConfiguration *config, QObject *parent = Q_NULLPTR) : Files::BaseManager(config){Q_UNUSED(parent)}
        bool   test_isRemote(QString src){return isRemote(src);}
        bool   test_isRelative(QString src){return isRelative(src);}
        void   test_renameDownloadedFile(QString file_path){renameDownloadedFile(file_path);}
};


class TestBaseManager : public QObject
{
        Q_OBJECT
    public:
        TestBaseManager(){}
    private:
        MainConfiguration *getConfig();

    private Q_SLOTS:
        void testIsRemote();
        void testIsRelative();
        void testRenameDownloadedFile();
};

MainConfiguration *TestBaseManager::getConfig()
{
    QSettings *Settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "SmilControl", "garlic-player-test");
    MainConfiguration *MyConfig  = new MainConfiguration(Settings);
    return MyConfig;
}

void TestBaseManager::testIsRemote()
{
    I_BaseManager   *MyBaseManager  = new I_BaseManager(getConfig());
    QVERIFY(MyBaseManager->test_isRemote("http://google.de"));
    QVERIFY(MyBaseManager->test_isRemote("https://google.de"));
    QVERIFY(MyBaseManager->test_isRemote("ftps://google.de"));
    QVERIFY(MyBaseManager->test_isRemote("ftp://google.de"));
    QVERIFY(!MyBaseManager->test_isRemote("/google.de"));
    QVERIFY(!MyBaseManager->test_isRemote("google.de"));
}

void TestBaseManager::testIsRelative()
{
    I_BaseManager   *MyBaseManager  = new I_BaseManager(getConfig());
    QVERIFY(MyBaseManager->test_isRelative("path_to/tralala"));
    QVERIFY(MyBaseManager->test_isRelative("./path_to/tralala"));
    QVERIFY(!MyBaseManager->test_isRelative("/path_to/tralala"));
    QVERIFY(!MyBaseManager->test_isRelative("//path_to/tralala"));
    QVERIFY(!MyBaseManager->test_isRelative("file://path_to/something"));
    QVERIFY(!MyBaseManager->test_isRelative("data:some dtata to print"));
}

void TestBaseManager::testRenameDownloadedFile()
{
    I_BaseManager   *MyBaseManager  = new I_BaseManager(getConfig());

    QString file_path = "./test.txt";
    QFile file_in(file_path + FILE_DOWNLOADED_SUFFIX);
    file_in.open(QIODevice::ReadWrite);
    file_in.close();
    MyBaseManager->test_renameDownloadedFile(file_path);
    QFile file_out(file_path);
    QVERIFY(file_out.exists());
    QVERIFY(!file_in.exists());
    //cleanup
    file_out.remove();
}



QTEST_APPLESS_MAIN(TestBaseManager)

#include "tst_base_manager.moc"
