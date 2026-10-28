#ifndef SMIL_H
#define SMIL_H

#include "base.hpp"

class Smil : public TBase
{
        Q_OBJECT
    public:
        Smil(QObject *parent);
        void preloadParse(QDomElement element);
};

#endif // SMIL_H
