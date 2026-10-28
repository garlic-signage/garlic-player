/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2022 Nikolaos Saghiadinos <ns@smil-control.com>
    This file is part of the garlic-player source code

    This program is free software: you can redistribute it and/or  modify
    it under the terms of the GNU Affero General Public License, version 3,
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************************/

#ifndef JSON_MODEL_H
#define JSON_MODEL_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace RestApi
{
    class JsonModel : public QObject
    {
            Q_OBJECT
        public:
            explicit JsonModel(QObject *parent = nullptr);
            void reset();
            void setJsonObject(QJsonObject jo);
            void insertStringValuePair(QString key, QString value);
            void insertIntegerValuePair(QString key, int value);
            void insertBoolValuePair(QString key, bool value);
            void insertObjectValuePair(QString key, QJsonObject value);
            void insertArrayValuePair(QString key, QJsonArray value);
            QString getStringValueByKey(QString key);
            bool readFromString(QString json_string);
            QString asString(bool compact = true);
            const QJsonObject &getJsonObj() const;

        private:
            QJsonObject json_obj;
            QJsonParseError *error;

        signals:

    };
}
#endif // JSON_MODEL_H
