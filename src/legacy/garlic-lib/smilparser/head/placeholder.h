#ifndef PLACEHOLDER_H
#define PLACEHOLDER_H

#include <QObject>
#include <QtXml>
#include <QHash>

namespace SmilHead
{
    class PlaceHolder : public QObject
    {
            Q_OBJECT
        public:
            explicit PlaceHolder(QObject *parent = nullptr);
            void     parsePlaceholder(QDomElement data);
            bool     isPlaceHolder(QString src);
            QString  findPathByPlaceHolder(QString placeholder);
        private:
             void insert(QString placeholder, QString path);
             QHash<QString, QString> placeholder_list;
    };
}
#endif // PLACEHOLDER_H
