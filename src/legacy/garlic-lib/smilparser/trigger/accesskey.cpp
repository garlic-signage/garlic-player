#include "accesskey.hpp"

AccessKey::AccessKey(QObject *parent) : Trigger::BaseTrigger(parent)
{

}

bool AccessKey::parse(QString char_value)
{
    QString cleared    = handlePossibleClockValue(char_value);
    cleared            = determineAccesskey(cleared);

    has_extern_trigger = false;

    if (cleared.length() != 1)
        return false;

    bool    ret = false;
    source_id   = "accesskey";
    char_symbol = cleared.at(0).toLower();
    if (char_symbol.isDigit() || char_symbol.isLetter())
        ret = true;

    if (ret)
        has_extern_trigger = true;

    return ret;

}

QChar AccessKey::getCharSymbol()
{
    return char_symbol;
}

QString AccessKey::determineAccesskey(QString value)
{
    // accesskey( => 10
    return value.mid(10, value.length()-11);
}
