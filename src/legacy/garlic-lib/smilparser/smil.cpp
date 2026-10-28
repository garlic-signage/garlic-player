#include "smil.h"

Smil::Smil(QObject *parent) : TBase(parent)
{

}

void Smil::preloadParse(QDomElement element)
{
    root_element   = element;
    setBaseAttributes();
}
