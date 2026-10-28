#ifndef ADAPIWRAPPER_H
#define ADAPIWRAPPER_H

#include <QString>
#include <QDateTime>

class AdapiWrapper
{
public:
    AdapiWrapper();
    void setExpr(QString e);
    QString replaceAdapiFunctions();

private:
    QString expr;
    void replaceSmilPlayerId();
    void replaceSmilPlayerName();
    void replaceDate();
    void replaceGmDate();
    void replaceWeekday();
    void replaceGmWeekday();
    void replaceCompare();
    void replaceEncoded();
    void replace(QString param, QString value);
    QDateTime getCurrentTime();
    QDateTime getCurrentTimeUTC();
    QString convertWeekdayToAdapi(int weekday);

};

#endif // ADAPIWRAPPER_H
