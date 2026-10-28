#ifndef I_WEEKDAYCONVERTER_HPP
#define I_WEEKDAYCONVERTER_HPP

#include <QString>

class IWeekdayConverter
{
public:
    virtual ~IWeekdayConverter() = default;

    virtual int convertWeekDay(const QString &weekday_string) const = 0;
};
#endif // I_WEEKDAYCONVERTER_HPP
