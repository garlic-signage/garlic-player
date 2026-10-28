#include <QString>
#include <QtTest>
#include "main_configuration.h"
#include "reports/create/event_logs.h"

void noMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg){Q_UNUSED(type); Q_UNUSED(context); Q_UNUSED(msg)}

class I_CreateEventLogs : public Reporting::CreateEventLogs
{
        Q_OBJECT
    public:
       I_CreateEventLogs(MainConfiguration *config, QObject *parent) : Reporting::CreateEventLogs(config, parent){}
       void test_mergeXML(QString xml){mergeXml(xml);}
};

class TestCreateEventLogs : public QObject
{
    Q_OBJECT

public:
    TestCreateEventLogs();

private Q_SLOTS:
    void initTestCase(){qInstallMessageHandler(noMessageOutput);}
    void testProcess();
};

TestCreateEventLogs::TestCreateEventLogs()
{
}

void TestCreateEventLogs::testProcess()
{
    QFile file(":/event_log.xml");
    file.copy("./event_log.xml");
    QSettings *Settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "SmilControl", "garlic-player-test");
    MainConfiguration *MyConfiguration = new MainConfiguration(Settings);
    MyConfiguration->setLogDir("./");
    I_CreateEventLogs *MyCreateEventLogs = new I_CreateEventLogs(MyConfiguration, Q_NULLPTR);
    MyCreateEventLogs->process("./event_log.xml");
    QString result = MyCreateEventLogs->asXMLString();
    QCOMPARE(result.size(), 3537);

    QDomDocument test;
    test.setContent(result);
    xxq(test.elementsByTagName("event").length(), 8);
    QCOMPARE(test.elementsByTagName("eventType").length(), 8);
    QCOMPARE(test.elementsByTagName("eventTime").length(), 8);
    QCOMPARE(test.elementsByTagName("report").length(), 1);
    QCOMPARE(test.elementsByTagName("player").length(), 1);
    QCOMPARE(test.elementsByTagName("playerEventLog").length(), 1);

}

QTEST_APPLESS_MAIN(TestCreateEventLogs)

#include "tst_event_logs.moc"
x
