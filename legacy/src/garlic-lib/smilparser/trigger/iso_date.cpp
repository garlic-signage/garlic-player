#include "iso_date.h"

IsoDate::IsoDate()
{

}

IsoPeriod IsoDate::analysePeriods(QString p_value)
{
    IsoPeriod               iso_period;
    QRegularExpression      re("^P(?:(\\d+)Y)?(?:(\\d+)M)?(?:(\\d+)W)?(?:(\\d+)D)?(?:T(?:(\\d+)H)?(?:(\\d+)M)?(?:(\\d+)S)?)?");
    QRegularExpressionMatch match  = re.match(p_value);
    if (match.hasMatch())
    {
        iso_period.years    = match.captured(1).toInt();
        iso_period.months   = match.captured(2).toInt();
        iso_period.days     = match.captured(3).toInt()*7; // weeks
        iso_period.days    += match.captured(4).toInt();
        iso_period.hours    = match.captured(5).toInt();
        iso_period.minutes  = match.captured(6).toInt();
        iso_period.seconds  = match.captured(7).toInt();
    }
    return iso_period;
}

int IsoDate::analyseRepeats(QString r_value)
{
    int  repeats    = 0;
    if(r_value.length() == 1)
        repeats = -1;
    else
        repeats = r_value.midRef(1, r_value.length()-1).toInt();
    return repeats;
}

/**
 * @brief This format yyyy-MM-dd(+/-)w(1..7)THH:mm:ss is supported
 *
 * @param date
 * @return
 */
QDateTime IsoDate::analyseDate(QString date)
{
    // we must do some work arounds cause QDateTime is pretty useless for wallclock
    // btw. also not recognizing all iso 8601 dates

    QStringList sl = date.split('T');

    return QDateTime(determineDate(sl.at(0)), determineTime(sl));
}


QDate IsoDate::determineDate(QString the_date)
{
    int w_position = the_date.indexOf('w');
    QDate ret;
    if (w_position > -1)
    {
        has_weekday = true;
        ret =  calculateDateWithWeekDayDifference(the_date, w_position);
    }
    else
    {
        has_weekday = false;
        ret =  QDate::fromString(the_date, "yyyy-MM-dd");
    }
    // a valid date is mandatory otherwise the app react unpredictable
    if (ret.isValid())
        return ret;
    else
        return QDate::fromString("2000-01-01", "yyyy-MM-dd");

}

QTime IsoDate::determineTime(QStringList sl)
{
    QTime ret;
    if (sl.size() > 1)
    {
        ret = analyzeTime(sl.at(1));
    }
    else
    {
        ret = QTime::fromString("00:00:00", "HH:mm:ss");
    }
    // valid time mandatory otherwise the app react unpredictable
    if (ret.isValid())
        return ret;
    else
        return QTime::fromString("00:00:00", "HH:mm:ss");
}

QTime IsoDate::analyzeTime(QString stime)
{
    int len = stime.length();

    switch (len)
    {
        case 8:
            return QTime::fromString(stime, "HH:mm:ss");
        case 5:
            return QTime::fromString(stime, "HH:mm");
        case 2:
            return QTime::fromString(stime, "HH");
    }

    return QTime::fromString("00:00:00", "HH:mm:ss");
}

int IsoDate::determineDayDiffByOperator(QChar op, int day_diff)
{
    // if the is no +/- we assume a missing operator so we set default to +
    if (op == '-')
        return calculateDiffBefore(day_diff);
    else
        return calculateDiffAfter(day_diff);
}

bool IsoDate::hasPeriod(IsoPeriod period)
{
    if (period.days == 0 && period.hours == 0 && period.minutes == 0 && period.months == 0 && period.seconds == 0 && period.years == 0)
        return false;

    return true;
}

QDate IsoDate::calculateDateWithWeekDayDifference(QString the_date, int w_position)
{
    QChar op    = the_date.at(w_position-1);
    weekday     = QString(the_date.at(w_position+1)).toInt();
    QDate temp  = seperateDateFromOperator(op, the_date, w_position);

    return temp.addDays(determineDayDiffByOperator(op, weekday - temp.dayOfWeek()));

}

QDate IsoDate::seperateDateFromOperator(QChar op, QString the_date, int w_position)
{
    if (op == '+' || op == '-')
        return QDate::fromString(the_date.mid(0, w_position-1), "yyyy-MM-dd");
    else
        return QDate::fromString(the_date.mid(0, w_position), "yyyy-MM-dd");
}

int IsoDate::calculateDiffBefore(int day_diff)
{
    // because modulo % fails
    if (day_diff >= 0)
          day_diff -= 7;
    return day_diff;
}


int IsoDate::calculateDiffAfter(int day_diff)
{
    // because modulo % fails
    if (day_diff <= 0)
          day_diff += 7;

    return day_diff;
}
