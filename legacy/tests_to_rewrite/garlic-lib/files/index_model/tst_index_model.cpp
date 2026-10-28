#include <QString>
#include <QtTest>
#include "index_model.h"

class TestIndexModel : public QObject
{
        Q_OBJECT
    public:
        TestIndexModel(){}

    private Q_SLOTS:
        void cleanup();
        void testLoadDocument();
        void testLoadDocumentError();
        void testGetHead();
        void testGetHeadWithoutHead();
        void testGetBody();
        void testGetBodyWithoutBody();
};

void TestIndexModel::cleanup()
{
    QFile::remove("./simple.smil");
    QFile::remove("./no_body.smil");
    QFile::remove("./no_head.smil");
}

void TestIndexModel::testLoadDocument()
{
    QFile file(":/simple.smil");
    file.copy("./simple.smil");
    QFileInfo fi("./simple.smil");
    QVERIFY(fi.exists());
    QFile file2(fi.absoluteFilePath());
    QDomDocument    document;
    file2.open(QIODevice::ReadOnly | QIODevice::Text);
    document.setContent(&file2);

    IndexModel *MyIndexModel = new IndexModel;
    QVERIFY(MyIndexModel->loadDocument(fi.absoluteFilePath()));

    QCOMPARE(MyIndexModel->getDocument().toString(-1), document.toString(-1));
}

void TestIndexModel::testLoadDocumentError()
{
    // no existing
    IndexModel *MyIndexModel = new IndexModel;
    QVERIFY(!MyIndexModel->loadDocument("no existing"));

    // exists but not a XML
    QVERIFY(!MyIndexModel->loadDocument("./tst_index_model"));

}

void TestIndexModel::testGetHead()
{
    QFile file(":/simple.smil");
    file.copy("./simple.smil");
    QFileInfo fi("./simple.smil");
    QVERIFY(fi.exists());

    IndexModel *MyIndexModel = new IndexModel;
    QVERIFY(MyIndexModel->loadDocument(fi.absoluteFilePath()));

    QDomElement head = MyIndexModel->getHead();
    QCOMPARE(head.tagName(), QString("head"));
    QVERIFY(head.hasChildNodes());
}

void TestIndexModel::testGetHeadWithoutHead()
{
    QFile file(":/no_head.smil");
    file.copy("./no_head.smil");
    QFileInfo fi("./no_head.smil");
    QVERIFY(fi.exists());

    IndexModel *MyIndexModel = new IndexModel;
    QVERIFY(MyIndexModel->loadDocument(fi.absoluteFilePath()));

    QDomElement head = MyIndexModel->getHead();
    QCOMPARE(head.tagName(), QString("head"));
    QVERIFY(!head.hasChildNodes());
   // to makes sure it is the right File
    QDomElement body = MyIndexModel->getBody();
    QCOMPARE(body.tagName(), QString("body"));
    QVERIFY(body.hasChildNodes());
}

void TestIndexModel::testGetBody()
{
    QFile file(":/simple.smil");
    file.copy("./simple.smil");
    QFileInfo fi("./simple.smil");
    QVERIFY(fi.exists());

    IndexModel *MyIndexModel = new IndexModel;
    QVERIFY(MyIndexModel->loadDocument(fi.absoluteFilePath()));

    QDomElement head = MyIndexModel->getBody();
    QCOMPARE(head.tagName(), QString("body"));
    QVERIFY(head.hasChildNodes());
}

void TestIndexModel::testGetBodyWithoutBody()
{
    QFile file(":/no_body.smil");
    file.copy("./no_body.smil");
    QFileInfo fi("./no_body.smil");
    QVERIFY(fi.exists());

    IndexModel *MyIndexModel = new IndexModel;
    QVERIFY(MyIndexModel->loadDocument(fi.absoluteFilePath()));

    // to make sure it is the right file
    QDomElement head = MyIndexModel->getHead();
    QCOMPARE(head.tagName(), QString("head"));
    QVERIFY(head.hasChildNodes());

    QDomElement body = MyIndexModel->getBody();
    QCOMPARE(body.tagName(), QString("body"));
    QVERIFY(!body.hasChildNodes());
}


QTEST_APPLESS_MAIN(TestIndexModel)

#include "tst_index_model.moc"
