#include "placeholder.h"

SmilHead::PlaceHolder::PlaceHolder(QObject *parent) : QObject(parent)
{
    placeholder_list.clear();
}

void SmilHead::PlaceHolder::parsePlaceholder(QDomElement data)
{
     if (!data.hasChildNodes())
         return;

     QDomNodeList childs = data.childNodes();
     QString tag_name    = "";
     QString path        = "";
     for (int i = 0; i < childs.length(); i++)
     {
         tag_name = childs.item(i).toElement().tagName();
         path     = childs.item(i).toElement().text();
         if (!tag_name.isEmpty() && !path.isEmpty())
            insert(tag_name, path);
     }
}

bool SmilHead::PlaceHolder::isPlaceHolder(QString src)
{
    return (src.at(0) == '{' && src.at(src.length()-1) == "}");
}

QString SmilHead::PlaceHolder::findPathByPlaceHolder(QString placeholder)
{
   QHash<QString, QString>::iterator i = placeholder_list.find(placeholder);

   if (i != placeholder_list.end())
        return i.value();
   else
       return "";
}

void SmilHead::PlaceHolder::insert(QString placeholder, QString path)
{
    placeholder_list.insert("{" + placeholder + "}", path);
}
