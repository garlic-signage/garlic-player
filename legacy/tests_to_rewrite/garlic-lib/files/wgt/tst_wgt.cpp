#include <QString>
#include <QtTest>
#include "files/wgt.h"

void noMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg){Q_UNUSED(type); Q_UNUSED(context); Q_UNUSED(msg)}
class TestWgt : public QObject
{
    Q_OBJECT

public:
    TestWgt(){}

    QDir cache_dir;

private Q_SLOTS:
    void init();
    void cleanup();
    void testIsOpen();
    void testCalculateSize();
    void testHandleRealPath();
    void testExtract();
    void testErrors();
};


void TestWgt::init()
{
   qInstallMessageHandler(noMessageOutput);
    cache_dir.setPath("./");
    if (cache_dir.mkdir("cache"))
        cache_dir.setPath("./cache");
}

void TestWgt::cleanup()
{
   cache_dir.setPath("./cache"); // for cases whem test breaks and did nor cleanup correct
   cache_dir.removeRecursively();
}

void TestWgt::testIsOpen()
{
    Wgt *MyWgt = new Wgt;
    QVERIFY(!MyWgt->isOpen());
    QFile file(":/test.wgt");
    file.copy("./cache/test.wgt");
    MyWgt->setFilePath("./cache/test.wgt");
    QVERIFY(MyWgt->isOpen());
}

void TestWgt::testHandleRealPath()
{
    QFile file(":/test.wgt");
    file.copy("./cache/test.wgt");
    Wgt *MyWgt = new Wgt;
    MyWgt->setFilePath("./cache/test_not_existing.wgt");
    QCOMPARE(MyWgt->handleRealPath(), QString(""));
    QFileInfo wgt("./cache/test.wgt");
    MyWgt->setFilePath(wgt.absoluteFilePath());
    QString real = MyWgt->handleRealPath();

#if defined  Q_OS_WIN
    QVERIFY(real.contains("file://")); // Windows needs file:// for open absolute paths in WebEngine
#else
    QVERIFY(real.contains("file:///"));
#endif
    QVERIFY(real.contains("/cache/test/index.html"));
}

void TestWgt::testCalculateSize()
{
    QFile file(":/test.wgt");
    file.copy("./cache/test.wgt");
    Wgt *MyWgt = new Wgt;
    MyWgt->setFilePath("./cache/test.wgt");
    QCOMPARE(MyWgt->calculateUncompressedSize(), 864);
    QFileInfo fi;
    fi.setFile("./cache/test/index.html");
    QVERIFY(!fi.exists());
}

void TestWgt::testExtract()
{
    QFile file(":/test.wgt");
    file.copy("./cache/test.wgt");
    Wgt *MyWgt = new Wgt;
    QFileInfo wgt("./cache/test.wgt");
    MyWgt->setFilePath(wgt.absoluteFilePath());

    MyWgt->extract();
    QFileInfo fi;
    fi.setFile("./cache/test/index.html");
    QVERIFY(fi.exists());
    fi.setFile("./cache/test/green-bullet.png");
    QVERIFY(fi.exists());
    fi.setFile("./cache/test/test.js");
    QVERIFY(fi.exists());

    QFile js("./cache/test/test.js");
    QVERIFY(js.open(QIODevice::ReadWrite));
    QCOMPARE(js.readAll(), QByteArray("function test{}{return;}\r\n"));
    file.close();
}

void TestWgt::testErrors()
{
    Wgt *MyWgt = new Wgt;
    QVERIFY(!MyWgt->isOpen());
    QCOMPARE(MyWgt->calculateUncompressedSize(), 0);

    MyWgt->extract();
    QFileInfo fi;
    fi.setFile("./cache/test/index.html");
    QVERIFY(!fi.exists());

}

QTEST_APPLESS_MAIN(TestWgt)

#include "tst_wgt.moc"
