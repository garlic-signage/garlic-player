#include "base_testsuite.hpp"
#include <QtTest/QtTest>
#include "garlic-lib/mock_main_configuration.hpp"
#include "i_weekdayconverter.hpp"

#include "scheduler.hpp"

class MockWeekdayConverter : public IWeekdayConverter
{
    public:
        MOCK_METHOD(int, convertWeekDay, (const QString &weekday_string), (const, override));
};

class TestScheduler: public BaseTestSuite
{
    Q_OBJECT
	private slots:
		void initTestCase();
		void cleanupTestCase();

        void test_DetermineNextRebootNoRebootDays();
        void test_DetermineNextRebootInvalidRebootTime();
        void test_CalculateNextReboot();
        void test_CalculateNextRebootOnSameDayPassed();
        void test_CalculateNextRebootOnSameDay();

    private:
		MockMainConfiguration *mockConfig;
		MockWeekdayConverter *mockConverter;
        Scheduler *MyScheduler;

};
