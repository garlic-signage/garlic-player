#include <QString>
#include <QtTest>
#include "files/download_queue.h"

class I_DownloadQueue : public DownloadQueue
{
    Q_OBJECT
public:
    explicit I_DownloadQueue(MainConfiguration *config) : DownloadQueue(config){}
    void test_doSucceed(TNetworkAccess *Downloader){doSucceed(Downloader);}
    void test_doNotCacheable(TNetworkAccess *Downloader){doNotCacheable(Downloader);}
    void test_doNotModified(TNetworkAccess *Downloader){doNotModified(Downloader);}
    void test_doFailed(TNetworkAccess *Downloader){doFailed(Downloader);}

};
void noMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg){Q_UNUSED(type); Q_UNUSED(context); Q_UNUSED(msg)}

class TestDownloadQueue : public QObject
{
        Q_OBJECT

    public:
private:
    MainConfiguration *getConfig();
    private Q_SLOTS:
        void initTestCase(){qInstallMessageHandler(noMessageOutput);}
        void testInsertQueue();
        void testDoSuccess();
        void testDoNotCacheable();
        void testDoNotModified();
        void testDoFailed();
};

MainConfiguration *TestDownloadQueue::getConfig()
{
    QSettings *Settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "SmilControl", "garlic-player-test");
    MainConfiguration *MyConfig  = new MainConfiguration(Settings);
    MyConfig->setUserAgent("GAPI/1.0 (UUID:f9d65c88-e4cd-43b4-89eb-5c338e54bcae; NAME:TestTDownload) xxxxxx-xx/x.x.x (MODEL:GARLIC)");
    return MyConfig;
}

void TestDownloadQueue::testInsertQueue()
{
    DownloadQueue *MyDownloadQueue = new DownloadQueue(getConfig());

    MyDownloadQueue->insertQueue("http://notexisting1.ddd", "http://notexisting1.ddd");
    MyDownloadQueue->insertQueue("http://notexisting2.ddd", "http://notexisting2.ddd");
    MyDownloadQueue->insertQueue("http://notexisting3.ddd", "http://notexisting3.ddd");
    MyDownloadQueue->insertQueue("http://notexisting4.ddd", "http://notexisting4.ddd");
    MyDownloadQueue->insertQueue("http://notexisting5.ddd", "http://notexisting5.ddd");
    MyDownloadQueue->insertQueue("http://notexisting6.ddd", "http://notexisting6.ddd");
    MyDownloadQueue->insertQueue("http://notexisting7.ddd", "http://notexisting7.ddd");
    MyDownloadQueue->insertQueue("http://notexisting8.ddd", "http://notexisting8.ddd");
    MyDownloadQueue->insertQueue("http://notexisting9.ddd", "http://notexisting9.ddd");
    MyDownloadQueue->insertQueue("http://notexisting10.ddd", "http://notexisting10.ddd");
    MyDownloadQueue->insertQueue("http://notexisting11.ddd", "http://notexisting11.ddd");
    MyDownloadQueue->insertQueue("http://notexisting12.ddd", "http://notexisting12.ddd");
    MyDownloadQueue->insertQueue("http://notexisting13.ddd", "http://notexisting13.ddd");
    MyDownloadQueue->insertQueue("http://notexisting14.ddd", "http://notexisting14.ddd");
    MyDownloadQueue->insertQueue("http://notexisting15.ddd", "http://notexisting15.ddd");
    MyDownloadQueue->insertQueue("http://notexisting16.ddd", "http://notexisting16.ddd");
    MyDownloadQueue->insertQueue("http://notexisting17.ddd", "http://notexisting17.ddd");
    MyDownloadQueue->insertQueue("http://notexisting18.ddd", "http://notexisting18.ddd");
    MyDownloadQueue->insertQueue("http://notexisting19.ddd", "http://notexisting19.ddd");
    MyDownloadQueue->insertQueue("http://notexisting20.ddd", "http://notexisting20.ddd");
    QVERIFY(MyDownloadQueue->getDownloadSlots().size() < 6);
    QVERIFY(MyDownloadQueue->getMediaQueue().size() > 10 );
}

void TestDownloadQueue::testDoSuccess()
{
    MainConfiguration *MyConfig = getConfig();
    I_DownloadQueue *MyDownloadQueue = new I_DownloadQueue(MyConfig);
    Downloader *MyDownloader = new Downloader(MyConfig);
    QFileInfo fi(":/test.png");
    MyDownloader->setLocalFileInfo(fi);
    QUrl url("http://ihoabsverstan.dn");
    MyDownloader->setRemoteFileUrl(url);
    qRegisterMetaType<QString>();
    QSignalSpy spy(MyDownloadQueue, SIGNAL(succeed(QString, QString)));
    QVERIFY(spy.isValid());
    MyDownloadQueue->test_doSucceed(MyDownloader);
    int i = 0;
    while (spy.count() == 0 && i < 1000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<QString>(spy.at(0).at(0)), url.toString());
    QCOMPARE(qvariant_cast<QString>(spy.at(0).at(1)), fi.absoluteFilePath());
}

void TestDownloadQueue::testDoNotCacheable()
{
    MainConfiguration *MyConfig = getConfig();
    I_DownloadQueue *MyDownloadQueue = new I_DownloadQueue(MyConfig);
    Downloader *MyDownloader = new Downloader(MyConfig);
    MyDownloader->setLocalFileInfo(QFileInfo(":/test.png"));
    QUrl url("http://notunderstandi.ng");
    MyDownloader->setRemoteFileUrl(url);
    qRegisterMetaType<QString>();
    QSignalSpy spy(MyDownloadQueue, SIGNAL(notcacheable(QString)));
    QVERIFY(spy.isValid());
    MyDownloadQueue->test_doNotCacheable(MyDownloader);
    int i = 0;
    while (spy.count() == 0 && i < 1000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<QString>(spy.at(0).at(0)), url.toString());
}

void TestDownloadQueue::testDoNotModified()
{
    MainConfiguration *MyConfig = getConfig();
    I_DownloadQueue *MyDownloadQueue = new I_DownloadQueue(MyConfig);
    Downloader *MyDownloader = new Downloader(MyConfig);
    MyDownloader->setLocalFileInfo(QFileInfo(":/test.png"));
    QUrl url("http://denkatalave.no");
    MyDownloader->setRemoteFileUrl(url);
    qRegisterMetaType<QString>();
    QSignalSpy spy(MyDownloadQueue, SIGNAL(notmodified(QString)));
    QVERIFY(spy.isValid());
    MyDownloadQueue->test_doNotModified(MyDownloader);
    int i = 0;
    while (spy.count() == 0 && i < 1000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<QString>(spy.at(0).at(0)), url.toString());
}

void TestDownloadQueue::testDoFailed()
{
    MainConfiguration *MyConfig = getConfig();
    I_DownloadQueue *MyDownloadQueue = new I_DownloadQueue(MyConfig);
    Downloader *MyDownloader = new Downloader(MyConfig);
    MyDownloader->setLocalFileInfo(QFileInfo(":/test.png"));
    QUrl url("http://nixverste.hn");
    MyDownloader->setRemoteFileUrl(url);
    qRegisterMetaType<QString>();
    QSignalSpy spy(MyDownloadQueue, SIGNAL(failed(QString)));
    QVERIFY(spy.isValid());
    MyDownloadQueue->test_doFailed(MyDownloader);
    int i = 0;
    while (spy.count() == 0 && i < 1000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<QString>(spy.at(0).at(0)), url.toString());
}

QTEST_MAIN(TestDownloadQueue)

#include "tst_download_queue.moc"
