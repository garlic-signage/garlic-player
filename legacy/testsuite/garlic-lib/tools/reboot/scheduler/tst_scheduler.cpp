#include "tst_scheduler.hpp"

using namespace testing;

void TestScheduler::initTestCase()
{
    mockConfig    = new MockMainConfiguration();
    mockConverter = new MockWeekdayConverter();
    MyScheduler     = new Scheduler(mockConfig, mockConverter);
}

void TestScheduler::cleanupTestCase() {
    delete MyScheduler;
    delete mockConverter;
    delete mockConfig;
}

void TestScheduler::test_DetermineNextRebootNoRebootDays()
{
    EXPECT_CALL(*mockConfig, getRebootDays()).WillOnce(Return(""));
    EXPECT_CALL(*mockConfig, getRebootTime()).WillOnce(Return(""));
    EXPECT_CALL(*mockConverter, convertWeekDay(QString(""))).Times(Exactly(1)).WillOnce(Return(0));

    MyScheduler->determineNextReboot(QDateTime::currentDateTime());
    QVERIFY(!MyScheduler->getNextDatetime().isValid());

}

void TestScheduler::test_DetermineNextRebootInvalidRebootTime()
{
    EXPECT_CALL(*mockConfig, getRebootDays()).WillOnce(Return("MoNday"));
    EXPECT_CALL(*mockConfig, getRebootTime()).WillOnce(Return("invalid_time"));
    EXPECT_CALL(*mockConverter, convertWeekDay(QString("MoNday"))).WillOnce(Return(1));

    MyScheduler->determineNextReboot(QDateTime::currentDateTime());
    QDateTime nextReboot = MyScheduler->getNextDatetime();
    QVERIFY(nextReboot.isValid());
    QVERIFY(MyScheduler->getNextDatetimeInMSecs() > 0);
    QCOMPARE(nextReboot.time(), QTime(3, 0, 0)); // default time
}

void TestScheduler::test_CalculateNextReboot()
{
    EXPECT_CALL(*mockConfig, getRebootDays()).WillOnce(Return("Monday Tuesday"));
    EXPECT_CALL(*mockConfig, getRebootTime()).WillOnce(Return("04:36:00"));

    EXPECT_CALL(*mockConverter, convertWeekDay(QString("Monday"))).WillOnce(Return(1));
    EXPECT_CALL(*mockConverter, convertWeekDay(QString("Tuesday"))).WillOnce(Return(2));

    MyScheduler->determineNextReboot(QDateTime::currentDateTime());
    QDateTime nextReboot = MyScheduler->getNextDatetime();
    QVERIFY(nextReboot.isValid());
    QVERIFY(MyScheduler->getNextDatetimeInMSecs() > 0);
    QCOMPARE(nextReboot.time(), QTime(4, 36, 0));
}


void TestScheduler::test_CalculateNextRebootOnSameDayPassed()
{
    // we set the current date for parameter to monday at 5:00:00
    QDate today = QDate::currentDate();
    int days_to_monday = (Qt::Monday - today.dayOfWeek()) % 7;
    QDate date_monday = today.addDays(days_to_monday);
    QDateTime monday_at_5 = QDateTime(date_monday, QTime(5, 0, 0));

    // we set the date for reboot to monday 4:36:00
    EXPECT_CALL(*mockConfig, getRebootDays()).WillOnce(Return("Monday"));
    EXPECT_CALL(*mockConfig, getRebootTime()).WillOnce(Return("04:36:00"));
    EXPECT_CALL(*mockConverter, convertWeekDay(QString("Monday"))).WillOnce(Return(1));

    // the next reboot must be in more than 6 days. So our currend date plus 6 days should be smaller then the next reboot.
    MyScheduler->determineNextReboot(monday_at_5);
    QDateTime test_future = monday_at_5.addDays(6);
    QDateTime next_reboot = MyScheduler->getNextDatetime();

    QVERIFY(test_future < next_reboot);
    QVERIFY(MyScheduler->getNextDatetimeInMSecs() > 0);
    QCOMPARE(next_reboot.time(), QTime(4, 36, 0));
}


void TestScheduler::test_CalculateNextRebootOnSameDay()
{
    // we set the current date for parameter to monday at 4:00:00
    QDate today = QDate::currentDate();
    int days_to_monday = (Qt::Monday - today.dayOfWeek()) % 7;
    QDate date_monday = today.addDays(days_to_monday);
    QDateTime monday_at_4 = QDateTime(date_monday, QTime(4, 0, 0));

    // we set the date for reboot to monday 4:36:00
    EXPECT_CALL(*mockConfig, getRebootDays()).WillOnce(Return("Monday"));
    EXPECT_CALL(*mockConfig, getRebootTime()).WillOnce(Return("04:36:00"));
    EXPECT_CALL(*mockConverter, convertWeekDay(QString("Monday"))).WillOnce(Return(1));

    MyScheduler->determineNextReboot(monday_at_4);
    QDateTime next_reboot = MyScheduler->getNextDatetime();

    QCOMPARE(monday_at_4.addSecs(60 * 36), next_reboot); // the next reboot must be in 36 minutes.
    QVERIFY(MyScheduler->getNextDatetimeInMSecs() > 0);
    QCOMPARE(next_reboot.time(), QTime(4, 36, 0));
}
static TestScheduler TEST_SCHEDULER;
