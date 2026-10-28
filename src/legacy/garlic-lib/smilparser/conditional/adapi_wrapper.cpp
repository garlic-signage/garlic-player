#include "adapi_wrapper.hpp"

AdapiWrapper::AdapiWrapper()
{

}

QString AdapiWrapper::replaceAdapiFunctions(QString e)
{
    expr = e;
    replaceSmilPlayerId();
    replaceSmilPlayerName();
    replaceDate();
    replaceGmDate();
    replaceWeekday();
    replaceGmWeekday();
    replaceEncoded();
    replaceCompare();
    return expr;
}

void AdapiWrapper::replaceSmilPlayerId()
{
    replace("smil-playerId()", "");
}

void AdapiWrapper::replaceSmilPlayerName()
{
    replace("smil-playerName()", "");
}

void AdapiWrapper::replaceDate()
{
    replace("adapi-date()", "'" + getCurrentTime().toString(Qt::ISODate) + "'");
}

void AdapiWrapper::replaceGmDate()
{
    replace("adapi-gmdate()", "'" + getCurrentTimeUTC().toString(Qt::ISODate) + "'");
}

void AdapiWrapper::replaceWeekday()
{
    int weekday = getCurrentTime().date().dayOfWeek();

    replace("adapi-weekday()", convertWeekdayToAdapi(weekday));
}

void AdapiWrapper::replaceGmWeekday()
{
    int weekday = getCurrentTimeUTC().date().dayOfWeek();

    if (weekday == 7) // convert sunday == 7 to sunday == 0
        weekday = 0;

    replace("adapi-weekday()", convertWeekdayToAdapi(weekday));
}

void AdapiWrapper::replaceCompare()
{
    replace("adapi-compare", QString("compare"));
}

void AdapiWrapper::replaceEncoded()
{
    replace("&gt;",">");
    replace("&lt;","<");
}

void AdapiWrapper::replace(QString param, QString value)
{
    expr.replace(param, value);
}

QDateTime AdapiWrapper::getCurrentTime()
{
    return QDateTime::currentDateTime();
}

QDateTime AdapiWrapper::getCurrentTimeUTC()
{
    return QDateTime::currentDateTimeUtc();
}

QString AdapiWrapper::convertWeekdayToAdapi(int weekday)
{
    if (weekday == 7) // convert sunday == 7 to sunday == 0
        weekday = 0;

    return QString::number(weekday);
}
