#ifndef ISOPERIOD_H
#define ISOPERIOD_H

#include <QDateTime>
#include <QRegularExpression>

struct IsoPeriod
{
    int    years   = 0;
    int    months  = 0;
    qint64 days    = 0;
    qint64 hours   = 0;
    qint64 minutes = 0;
    qint64 seconds = 0;
};

class IsoDate
{
    public:
        IsoDate();

        IsoPeriod analysePeriods(QString p_value);
        int       analyseRepeats(QString r_value);
        QDateTime analyseDate(QString date);
        bool hasWeekDay(){return has_weekday;}
        int  getWeekDay(){return weekday;}
        int  determineDayDiffByOperator(QChar op, int day_diff);
        bool hasPeriod(IsoPeriod period);
    private:
        bool  has_weekday = false;
        int   weekday;

        QDate determineDate(QString the_date);
        QDate calculateDateWithWeekDayDifference(QString the_date, int w_position);
        QDate seperateDateFromOperator(QChar op, QString the_date, int w_position);
        QTime determineTime(QStringList sl);
        QTime analyzeTime(QString stime);
        int   calculateDiffAfter(int day_diff);
        int   calculateDiffBefore(int day_diff);
};
#endif // ISOPERIOD_H
