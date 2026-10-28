#ifndef ADAPIWRAPPER_HPP
#define ADAPIWRAPPER_HPP

#include <QString>
#include <QDateTime>

class AdapiWrapper
{
    public:
        AdapiWrapper();
        QString replaceAdapiFunctions(QString e);

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

#endif // ADAPIWRAPPER_HPP
