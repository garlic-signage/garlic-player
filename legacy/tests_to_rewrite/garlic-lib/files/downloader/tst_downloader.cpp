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
#include <QFileInfo>
#include <QIODevice>
#include <QtTest>
#include <stdio.h>
#include <stdlib.h>

#include <files/downloader.h>

void noMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg){Q_UNUSED(type); Q_UNUSED(context); Q_UNUSED(msg)}

class TestDownloader : public QObject
{
    Q_OBJECT
public:
    TestDownloader(){}

private:
    MainConfiguration *getConfig(QByteArray agent);

private Q_SLOTS:
    void cleanup();
    void initTestCase(){qInstallMessageHandler(noMessageOutput);}
    void testObjectCreateAndDelete();
    void testAgentString();
    void testDownloadSmilNew();
    void testDownloadSmilNotModified();
    void testDownloadSmilUpdated();
    void testDownloadMedia();
    void testDownloadMediaDirect();
    void testDownloadFailed();
    void testDownloadWebSite();
    void testDownloadWebSiteWith301Redirect();
};

MainConfiguration *TestDownloader::getConfig(QByteArray agent)
{
    QSettings *Settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "SmilControl", "garlic-player-test");
    MainConfiguration *MyConfig  = new MainConfiguration(Settings);
    MyConfig->setUserAgent(agent);
    return MyConfig;
}

void TestDownloader::cleanup()
{
    QFile file("./agent.txt");
    if (file.exists())
        file.remove();
    file.setFileName("./index.smil");
    if (file.exists())
        file.remove();
    file.setFileName("./index.smil" + FILE_DOWNLOADED_SUFFIX);
    if (file.exists())
        file.remove();
    file.setFileName("./server.jpg");
    if (file.exists())
        file.remove();
    file.setFileName("./server.jpg" + FILE_DOWNLOADED_SUFFIX);
    if (file.exists())
        file.remove();
}

void TestDownloader::testObjectCreateAndDelete()
{
    MainConfiguration *MyConfig  = getConfig("GAPI/1.0 (UUID:f9d65c88-e4cd-43b4-89eb-5c338e54bcae; NAME:TestTDownload) xxxxxx-xx/x.x.x (MODEL:GARLIC)");
    Downloader *MyDownloader  = new Downloader(MyConfig);
    Downloader *MyDownloader1 = new Downloader(MyConfig);
    Downloader *MyDownloader2 = new Downloader(MyConfig);
    Downloader *MyDownloader3 = new Downloader(MyConfig);
    Downloader *MyDownloader4 = new Downloader(MyConfig);
    Downloader *MyDownloader5 = new Downloader(MyConfig);
    delete MyDownloader;
    delete MyDownloader1;
    delete MyDownloader2;
    delete MyDownloader3;
    delete MyDownloader4;
    delete MyDownloader5;

    MyDownloader = new Downloader(MyConfig);
    QUrl url("http://garlic-player.com/downloads/garlic-player.deb");
    QFileInfo fi("garlic-player.deb");
    qRegisterMetaType<TNetworkAccess *>();
    QSignalSpy spy(MyDownloader, SIGNAL(succeed(TNetworkAccess *)));
    QVERIFY(spy.isValid());
    MyDownloader->processFile(url, fi);
    QCOMPARE(spy.count(), 0);
    delete MyDownloader;
}

void TestDownloader::testAgentString()
{
    QByteArray agent("This is a user agent");
    MainConfiguration *MyConfig  = getConfig(agent);
    Downloader *MyDownloader = new Downloader(MyConfig);
    QUrl url("http://testing.smil-admin.com/garlic/test.php?action=show_agent_string");
    QFileInfo fi("./agent.txt");
    QFile file(fi.absoluteFilePath());

    qRegisterMetaType<TNetworkAccess *>();
    QSignalSpy spy(MyDownloader, SIGNAL(succeed(TNetworkAccess *)));
    QVERIFY(spy.isValid());
    MyDownloader->processFile(url, fi);

    int i = 0;
    while (spy.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }

    QCOMPARE(spy.count(), 1);
    QCOMPARE(qvariant_cast<Downloader *>(spy.at(0).at(0)), MyDownloader);
    QVERIFY(file.exists());
    file.open(QIODevice::ReadOnly);
    QCOMPARE(file.readAll(), agent);
    file.close();
    delete MyDownloader;
}

void TestDownloader::testDownloadSmilNew()
{
    Downloader *MyDownloader = new Downloader(getConfig("GAPI/1.0 (UUID:f9d65c88-e4cd-43b4-89eb-5c338e54bcae; NAME:TestTDownload) xxxxxx-xx/x.x.x (MODEL:GARLIC)"));
    QUrl url("http://testing.smil-admin.com/garlic/test.php?action=get_smil_new");
    QFileInfo fi("./index.smil");
    qRegisterMetaType<TNetworkAccess *>();
    QSignalSpy spy1(MyDownloader, SIGNAL(succeed(TNetworkAccess *)));
    QVERIFY(spy1.isValid());
    MyDownloader->processFile(url, fi);
    int i = 0;
    while (spy1.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy1.count(), 1);
    QFile file(":/simple.smil");
    QFile data(fi.absoluteFilePath());
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    data.open(QIODevice::ReadOnly | QIODevice::Text);
    QCOMPARE(data.read(68), file.read(68)); // readAll() makes not equal \r\n when read from web
    QCOMPARE(MyDownloader, qvariant_cast<QObject *>(spy1.at(0).at(0))); // readAll() makes not equal \r\n when read from web
    QDateTime first_download = fi.lastModified();
    file.close();
    data.close();

    // second try with same file is already on disk downloaded previously
    QSignalSpy spy2(MyDownloader, SIGNAL(notmodified(TNetworkAccess *)));
    QVERIFY(spy2.isValid());
    MyDownloader->processFile(url, fi);
    i = 0;
    while (spy2.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy2.count(), 1);
    QVERIFY(first_download == fi.lastModified());
    delete MyDownloader;
}

void TestDownloader::testDownloadSmilNotModified()
{
    Downloader *MyDownloader = new Downloader(getConfig("GAPI/1.0 (UUID:f9d65c88-e4cd-43b4-89eb-5c338e54bcae; NAME:TestTDownload) xxxxxx-xx/x.x.x (MODEL:GARLIC)"));
    // request a SMIL which returned a not modiefied 304
    QUrl url("http://testing.smil-admin.com/garlic/test.php?action=get_smil_cached");
    QFileInfo fi(":/notavaible.smil"); // to make sure file is not exist
    QSignalSpy spy(MyDownloader, SIGNAL(notmodified(TNetworkAccess *)));
    QVERIFY(spy.isValid());
    MyDownloader->processFile(url, fi);
    int i = 0;
    while (spy.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy.count(), 1);
    delete MyDownloader;
}

void TestDownloader::testDownloadSmilUpdated()
{
    Downloader *MyDownloader = new Downloader(getConfig("GAPI/1.0 (UUID:f9d65c88-e4cd-43b4-89eb-5c338e54bcae; NAME:TestTDownload) xxxxxx-xx/x.x.x (MODEL:GARLIC)"));
    QUrl url("http://testing.smil-admin.com/garlic/test.php?action=get_smil_new");
    QFileInfo fi("./index.smil");
    qRegisterMetaType<TNetworkAccess *>();
    QSignalSpy spy1(MyDownloader, SIGNAL(succeed(TNetworkAccess *)));
    QVERIFY(spy1.isValid());
    MyDownloader->processFile(url, fi);
    int i = 0;
    while (spy1.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy1.count(), 1);
    QDateTime first_download = fi.lastModified();

    // request an updated SMIL
    url.setUrl("http://testing.smil-admin.com/garlic/test.php?action=get_smil_updated");
    fi.setFile("./index.smil"); // set back
    QSignalSpy spy2(MyDownloader, SIGNAL(succeed(TNetworkAccess *)));
    MyDownloader->processFile(url, fi);
    QVERIFY(spy2.isValid());
    i = 0;
    while (spy2.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy2.count(), 1);

    QFileInfo fi2(fi.absoluteFilePath() + FILE_DOWNLOADED_SUFFIX); // to make sure file exists
    QVERIFY(fi2.exists());
    QVERIFY(first_download != fi2.lastModified());
    delete MyDownloader;
}

void TestDownloader::testDownloadMedia()
{
    Downloader *MyDownloader = new Downloader(getConfig("GAPI/1.0 (UUID:f9d65c88-e4cd-43b4-89eb-5c338e54bcae; NAME:TestTDownload) xxxxxx-xx/x.x.x (MODEL:GARLIC)"));
    QUrl url("http://testing.smil-admin.com/garlic/test.php?action=get_media_new");
    QFileInfo fi("./server.jpg");
    QFileInfo fi_compare(fi.absoluteFilePath());
    qRegisterMetaType<TNetworkAccess *>();
    QSignalSpy spy1(MyDownloader, SIGNAL(succeed(TNetworkAccess *)));
    QVERIFY(spy1.isValid());
    MyDownloader->processFile(url, fi);
    int i = 0;
    while (spy1.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy1.count(), 1);
    QDateTime first_download = fi_compare.lastModified();

    QFile data("./server.jpg");
    QFile file(":/server.jpg");
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    data.open(QIODevice::ReadOnly | QIODevice::Text);
    QCOMPARE(data.read(50), file.read(50)); // readAll() makes not equal \r\n when read from web
    file.close();
    data.close();

    // second try with already previous downloaded media
    QSignalSpy spy2(MyDownloader, SIGNAL(notmodified(TNetworkAccess *)));
    QVERIFY(spy2.isValid());
    MyDownloader->processFile(url, fi);
    i = 0;
    while (spy2.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy2.count(), 1);
    QVERIFY(fi.lastModified() == first_download);

    // check to react on http 304
    url.setUrl("http://testing.smil-admin.com/garlic/test.php?action=get_media_cached");
    fi.setFile("./server_notavaible.jpg"); // to make sure file is not exist
    QSignalSpy spy3(MyDownloader, SIGNAL(notmodified(TNetworkAccess *)));
    QVERIFY(spy3.isValid());
    MyDownloader->processFile(url, fi);
    i = 0;
    while (spy3.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy3.count(), 1);
    QVERIFY(!fi.exists());
    QFileInfo fi1("./server.jpg");
    QVERIFY(first_download == fi1.lastModified());

    url.setUrl("http://testing.smil-admin.com/garlic/test.php?action=get_media_updated");
    fi.setFile("./server.jpg"); // to make sure file exists
    QSignalSpy spy4(MyDownloader, SIGNAL(succeed(TNetworkAccess *)));
    QVERIFY(spy4.isValid());
    MyDownloader->processFile(url, fi);
    i = 0;
    while (spy4.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy4.count(), 1);
    QFileInfo fi2(fi.absoluteFilePath() + FILE_DOWNLOADED_SUFFIX); // to make sure file exists
    QVERIFY(first_download < fi2.lastModified());
    delete MyDownloader;
}

void TestDownloader::testDownloadMediaDirect()
{
    Downloader *MyDownloader = new Downloader(getConfig("GAPI/1.0 (UUID:f9d65c88-e4cd-43b4-89eb-5c338e54bcae; NAME:TestTDownload) xxxxxx-xx/x.x.x (MODEL:GARLIC)"));
    QUrl url("http://testing.smil-admin.com/garlic/server.jpg");
    QFileInfo fi("./server.jpg");
    QSignalSpy spy1(MyDownloader, SIGNAL(succeed(TNetworkAccess *)));
    QVERIFY(spy1.isValid());
    QFile data("./server.jpg");
    QVERIFY(!data.exists());
    MyDownloader->processFile(url, fi);
    int i = 0;
    while (spy1.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy1.count(), 1);
    QFile file(":/server.jpg");
    QVERIFY(data.exists());
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    data.open(QIODevice::ReadOnly | QIODevice::Text);
    QCOMPARE(data.read(50), file.read(50)); // readAll() makes not equal \r\n when read from web
    file.close();
    data.close();

    // second try with already downloaded media
    QSignalSpy spy2(MyDownloader, SIGNAL(notmodified(TNetworkAccess *)));
    QVERIFY(spy2.isValid());
    MyDownloader->processFile(url, fi);
    i = 0;
    while (spy2.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy2.count(), 1);
}

void TestDownloader::testDownloadFailed()
{
    Downloader *MyDownloader = new Downloader(getConfig("GAPI/1.0 (UUID:f9d65c88-e4cd-43b4-89eb-5c338e54bcae; NAME:TestTDownload) xxxxxx-xx/x.x.x (MODEL:GARLIC)"));
    QUrl url("http://testing.smil-admin.com/garlic/notexistingfile.jpg");
    QFileInfo fi("./notexisting");
    QSignalSpy spy1(MyDownloader, SIGNAL(failed(TNetworkAccess *)));
    QVERIFY(spy1.isValid());
    MyDownloader->processFile(url, fi);
    int i = 0;
    while (spy1.count() == 0 && i < 2000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy1.count(), 1);

    // file exists but not valid mime/extension
    QUrl url2("http://testing.smil-admin.com/garlic/no.bin");
    QSignalSpy spy2(MyDownloader, SIGNAL(failed(TNetworkAccess *)));
    QVERIFY(spy2.isValid());
    MyDownloader->processFile(url2, fi);
    i = 0;
    while (spy2.count() == 0 && i < 2000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy2.count(), 1);
    delete MyDownloader;
}

void TestDownloader::testDownloadWebSite()
{
    Downloader *MyDownloader = new Downloader(getConfig("GAPI/1.0 (UUID:f9d65c88-e4cd-43b4-89eb-5c338e54bcae; NAME:TestTDownload) xxxxxx-xx/x.x.x (MODEL:GARLIC)"));
    QUrl url("http://smil-control.com");
    QFileInfo fi;
    qRegisterMetaType<TNetworkAccess *>();
    QSignalSpy spy1(MyDownloader, SIGNAL(notcacheable(TNetworkAccess *)));
    QVERIFY(spy1.isValid());
    MyDownloader->processFile(url, fi);
    int i = 0;
    while (spy1.count() == 0 && i < 4000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(qvariant_cast<TNetworkAccess *>(spy1.at(0).at(0)), MyDownloader);
    QCOMPARE(MyDownloader->getRemoteFileUrl(), url);
    delete MyDownloader;
}

void TestDownloader::testDownloadWebSiteWith301Redirect()
{
    Downloader *MyDownloader = new Downloader(getConfig("GAPI/1.0 (UUID:f9d65c88-e4cd-43b4-89eb-5c338e54bcae; NAME:TestTDownload) xxxxxx-xx/x.x.x (MODEL:GARLIC)"));
    QUrl url("http://heise.de");
    QFileInfo fi;
    qRegisterMetaType<TNetworkAccess *>();
    QSignalSpy spy1(MyDownloader, SIGNAL(notcacheable(TNetworkAccess *)));
    QVERIFY(spy1.isValid());
    MyDownloader->processFile(url, fi);
    int i = 0;
    while (spy1.count() == 0 && i < 8000)
    {
        QTest::qWait(200);
        i +=200;
    }
    QCOMPARE(spy1.count(), 1);
    QCOMPARE(qvariant_cast<TNetworkAccess *>(spy1.at(0).at(0)), MyDownloader);
    QCOMPARE(MyDownloader->getRemoteFileUrl(), url); // should not be something like https://heise.de
    delete MyDownloader;
}



QTEST_MAIN(TestDownloader)

#include "tst_downloader.moc"
