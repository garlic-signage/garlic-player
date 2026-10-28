#include <QString>
#include <QtTest>
#include "httpd/json_model.h"

class TestJsonModel : public QObject
{
        Q_OBJECT
    public:
        TestJsonModel(){}
    private:

    private Q_SLOTS:
        void testReadFromString();
};

void TestJsonModel::testReadFromString()
{
    JsonModel MyJson;

    QString json_string = "{\"className\":\"TheClassName\",\"packagename\":\"ThePackageName\",\"uri\":\"https://indexes.smil-control.com/index.php?site=test\"}";

    MyJson.readFromString(json_string);
    QJsonObject jo_in = MyJson.getJsonObj();
    QCOMPARE(3, jo_in.size());

    QJsonObject::iterator i = jo_in.find("uri");
    QCOMPARE(i.key(), "uri");
    QCOMPARE(i.value(), "https://indexes.smil-control.com/index.php?site=test");
    i = jo_in.find("className");
    QCOMPARE(i.key(), "className");
    QCOMPARE(i.value(), "TheClassName");
    i = jo_in.find("packagename");
    QCOMPARE(i.key(), "packagename");
    QCOMPARE(i.value(), "ThePackageName");

    i = jo_in.find("unknown");
    QCOMPARE(i, jo_in.end());

    // convert back to test
    QJsonDocument json_doc(jo_in);
    QCOMPARE(json_string, json_doc.toJson(QJsonDocument::Compact));

}


QTEST_APPLESS_MAIN(TestJsonModel)

#include "tst_json_model.moc"
