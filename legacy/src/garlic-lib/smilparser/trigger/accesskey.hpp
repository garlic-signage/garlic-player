#ifndef ACCESSKEY_H
#define ACCESSKEY_H

#include "base_trigger.h"

class AccessKey : public Trigger::BaseTrigger
{
        Q_OBJECT
    public:
        explicit AccessKey(QObject *parent = nullptr);
        bool     parse(QString char_value);

        QChar    getCharSymbol();

    private:
        QChar char_symbol;
        QString determineAccesskey(QString value);

};

#endif // ACCESSKEY_H
