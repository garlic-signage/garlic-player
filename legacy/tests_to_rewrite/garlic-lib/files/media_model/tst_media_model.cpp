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
#include <files/media_model.h>

class I_MediaModel : public MediaModel
{
    Q_OBJECT
public:
    QString   test_determinePathByMedia(QString src, QString local){return determinePathByMedia(src, local);}
};


void noMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg){Q_UNUSED(type); Q_UNUSED(context); Q_UNUSED(msg)}
class TestMediaModel : public QObject
{
    Q_OBJECT

public:
    TestMediaModel(){}
    QDir cache_dir;

private Q_SLOTS:
    void init();
    void cleanup();
    void testInsertAvaibleFile();
    void testDeterminePathByMediaIfWgt();
    void testDeterminePathByMediaIfOther();
    void testDetermineHashedFilePath();
};

void TestMediaModel::init()
{
   qInstallMessageHandler(noMessageOutput);
    cache_dir.setPath("./");
    if (cache_dir.mkpath("./cache/test"))
        cache_dir.setPath("./cache");
}

void TestMediaModel::cleanup()
{
   cache_dir.setPath("./cache"); // for cases whem test breaks and did nor cleanup correct
   cache_dir.removeRecursively();
}



void TestMediaModel::testInsertAvaibleFile()
{
    MediaModel *MyMediaModel = new MediaModel;
    QCOMPARE(MyMediaModel->getAvailableMediaList().size(), 0);
    QFile file(":/test.wgt");
    file.copy("./cache/test.wgt");
    QFileInfo fi("./cache/test.wgt");

    QCOMPARE(MyMediaModel->findLocalBySrcPath(fi.absoluteFilePath()), QString(""));
    QCOMPARE(MyMediaModel->findStatusBySrcPath(fi.absoluteFilePath()), MEDIA_NOT_EXISTS);

    // do a first insert
    MyMediaModel->insertAvaibleFile(fi.absoluteFilePath(), fi.absoluteFilePath());
    QCOMPARE(MyMediaModel->getAvailableMediaList().size(), 1);
    QFile file1(":/test.png");
    file1.copy("./uiuiui.png");
    QFileInfo fi1("./uiuiui.png");
    MyMediaModel->insertAvaibleFile(fi1.absoluteFilePath(), fi1.absoluteFilePath());
    QCOMPARE(MyMediaModel->getAvailableMediaList().size(), 2);


    // check first insert
    QString local = MyMediaModel->findLocalBySrcPath(fi.absoluteFilePath());
#if defined  Q_OS_WIN
    QVERIFY(local.contains("file://")); // Windows needs file:// for open absolute paths in WebEngine
#else
    QVERIFY(local.contains("file:///"));
#endif
    QVERIFY(local.contains("/cache/test/index.html"));
    QCOMPARE(MyMediaModel->findStatusBySrcPath(fi.absoluteFilePath()), MEDIA_AVAILABLE);

    local = MyMediaModel->findLocalBySrcPath(fi1.absoluteFilePath());
    QVERIFY(local.contains("/uiuiui.png"));
    QCOMPARE(MyMediaModel->findStatusBySrcPath(fi.absoluteFilePath()), MEDIA_AVAILABLE);

    // check second insert
    MyMediaModel->insertAvaibleFile(fi1.absoluteFilePath(), fi1.absoluteFilePath());
    QCOMPARE(MyMediaModel->getAvailableMediaList().size(), 2);
    MyMediaModel->setStatusBySrcPath(fi1.absoluteFilePath(), MEDIA_MODIFIED);
    QCOMPARE(MyMediaModel->findStatusBySrcPath(fi1.absoluteFilePath()), MEDIA_MODIFIED);

}

void TestMediaModel::testDeterminePathByMediaIfWgt()
{
    MainConfiguration *MyConfiguration = new MainConfiguration(new QSettings);
    MyConfiguration->setUserAgent("This-is-my-agent");
    I_MediaModel *MyMediaModel = new I_MediaModel;
    QFile file(":/test.wgt");
    file.copy("./cache/test.wgt");
    QFileInfo fi("./cache/test.wgt");
    QString local = MyMediaModel->test_determinePathByMedia(fi.absoluteFilePath(), fi.absoluteFilePath());

#if defined  Q_OS_WIN
    QVERIFY(local.contains("file://")); // Windows needs file:// for open absolute paths in WebEngine
#else
    QVERIFY(local.contains("file:///"));
#endif
    QVERIFY(local.contains("/cache/test/index.html"));
}

void TestMediaModel::testDeterminePathByMediaIfOther()
{
    I_MediaModel *MyMediaModel = new I_MediaModel;
    QFile file(":/test.png");
    file.copy("./uiuiui.png");
    QFileInfo fi("./uiuiui.png");

    QString local = MyMediaModel->test_determinePathByMedia(fi.absoluteFilePath(), fi.absoluteFilePath());

    QVERIFY(local.contains("/"));
    QVERIFY(local.contains("/uiuiui.png"));

    file.remove();
}

void TestMediaModel::testDetermineHashedFilePath()
{
    MediaModel *MyMediaModel = new MediaModel;

    QCOMPARE(QString("ff09d70446a146d3f8426b9136db0a00.wgt"), MyMediaModel->determineHashedFilePath("https://www.smil-admin.com/uiuiui.wgt"));
}

QTEST_MAIN(TestMediaModel)

#include "tst_media_model.moc"
