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
#include "json_model.h"

RestApi::JsonModel::JsonModel(QObject *parent) : QObject(parent)
{

}

void RestApi::JsonModel::reset()
{
    if (json_obj.size() == 0)
        return;

    foreach(const QString& key, json_obj.keys())
    {
        json_obj.remove(key);
    }
}

void RestApi::JsonModel::setJsonObject(QJsonObject jo)
{
    json_obj = jo;
}

void RestApi::JsonModel::insertIntegerValuePair(QString key, int value)
{
    json_obj.insert(key, value);
}

void RestApi::JsonModel::insertBoolValuePair(QString key, bool value)
{
    json_obj.insert(key, value);
}

void RestApi::JsonModel::insertStringValuePair(QString key, QString value)
{
    json_obj.insert(key, value);
}

void RestApi::JsonModel::insertArrayValuePair(QString key, QJsonArray value)
{
    json_obj[key] = value;
}

void RestApi::JsonModel::insertObjectValuePair(QString key, QJsonObject value)
{
    json_obj.insert(key, value);
}

QString RestApi::JsonModel::getStringValueByKey(QString key)
{
    if (json_obj.size() == 0)
        return "error";

    QJsonObject::iterator i = json_obj.find(key);
    return i.value().toString();
}

bool RestApi::JsonModel::readFromString(QString json_string)
{
    QByteArray json_bytes = json_string.toUtf8();

    QJsonDocument json_doc = QJsonDocument::fromJson(json_bytes);

    if (json_doc.isNull())
    {
        qDebug() << "Failed to create JSON doc." << error->errorString();
        return false;
    }
    if (!json_doc.isObject())
    {
        qDebug() << "JSON is not an object.";
        return false;
    }

    json_obj = json_doc.object();
    if (json_obj.isEmpty())
    {
        qDebug() << "JSON object is empty.";
        return false;
    }

    return true;
}

QString RestApi::JsonModel::asString(bool compact)
{
    QJsonDocument json_doc(json_obj);
    if (compact)
        return json_doc.toJson(QJsonDocument::Compact);
    else
        return json_doc.toJson(QJsonDocument::Indented);
}

const QJsonObject &RestApi::JsonModel::getJsonObj() const
{
    return json_obj;
}
