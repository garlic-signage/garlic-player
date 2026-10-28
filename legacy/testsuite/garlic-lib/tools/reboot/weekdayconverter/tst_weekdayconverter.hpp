#include "base_testsuite.hpp"
#include <QtTest/QtTest>

class TestWeekdayConverter : public BaseTestSuite
{
        Q_OBJECT
    public:
        TestWeekdayConverter(){};

    private Q_SLOTS:
        void test_ConvertWeekDay();
};
