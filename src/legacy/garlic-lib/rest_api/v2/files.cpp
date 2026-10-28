/*************************************************************************************
    garlic-player: SMIL Player for Digital Signage
    Copyright (C) 2024 Nikolaos Saghiadinos <ns@smil-control.com>
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
#include "files.h"

RestApi::V2::Files::Files(QObject *parent) : BaseController{parent}
{

}

QString RestApi::V2::Files::findInfoByID(QString id)
{
   DB::InventoryDataset result = MyLibfacade->getInventoryTable()->findByCacheBaseName(id);

   JsonResponse.reset();
   if (!result.resource_uri.isEmpty())
   {
       JsonResponse.setJsonObject(createObject(result));
   }
   else
   {
       JsonResponse.insertStringValuePair("error", "Record not found: " + id);
   }

   return JsonResponse.asString(false).toUtf8();
}

QString RestApi::V2::Files::modifyByID(QString id,  qint64 seek, QTemporaryFile *tfile, qint64 file_size, QString download_path, QString etag, QString mime_type, QString modified_date)
{
    Q_UNUSED(seek);
    Q_UNUSED(etag);

    JsonResponse.reset();
    if (download_path.isEmpty())
    {
        JsonResponse.insertStringValuePair("error", "downloadPath field missed");
        return JsonResponse.asString(false).toUtf8();
    }

    DB::InventoryDataset result = MyLibfacade->getInventoryTable()->findByCacheBaseName(id);
    if (result.resource_uri.isEmpty())
    {
        JsonResponse.insertStringValuePair("error", "Record not found: " + id);
        return JsonResponse.asString(false).toUtf8();
    }

    if (result.resource_uri != download_path)
    {
        JsonResponse.insertStringValuePair("error", "Record not identical to downloadPath" + id);
        return JsonResponse.asString(false).toUtf8();
    }

    QFileInfo fi_tmp(download_path);
    QString cache_path = MyLibfacade->getConfiguration()->getPaths("cache") + result.cache_name;
    if (tfile->exists()) // and seek == 0;
    {
        if (QFile::exists(cache_path))
            QFile::remove(cache_path);

        tfile->copy(cache_path);
        tfile->close();
    }

    QFileInfo fi(cache_path);
    DB::InventoryDataset dataset;

    if (mime_type.isEmpty())
    {
        QMimeDatabase db;
        QMimeType type   = db.mimeTypeForFile(MyLibfacade->getConfiguration()->getPaths("cache") + fi.fileName());
        dataset.content_type   = type.name();
    }
    else
        dataset.content_type   = mime_type;

    dataset.resource_uri   = download_path;
    dataset.cache_name     = result.cache_name;
    dataset.content_length = file_size;

    QDateTime dt =  QDateTime::fromString(modified_date, Qt::ISODate);
    if (dt.isValid())
        dataset.last_update = dt;
    else
        dataset.last_update = fi.birthTime();

    bool completed = true;
    dataset.state          = DB::COMPLETE;
    if (fi.size() != file_size)
    {
        dataset.state          = 0;
        completed = false;
    }
    MyLibfacade->getInventoryTable()->replace(dataset);

    JsonResponse.insertStringValuePair("downloadPath", dataset.resource_uri);
    JsonResponse.insertIntegerValuePair("fileSize", file_size);
    JsonResponse.insertStringValuePair("id", fi.baseName());
    JsonResponse.insertStringValuePair("createdDate", fi.birthTime().toString(Qt::ISODate));
    JsonResponse.insertIntegerValuePair("transferredSize", fi.size());
    JsonResponse.insertBoolValuePair("completed", completed);

    return JsonResponse.asString(false).toUtf8();
}

QString RestApi::V2::Files::createNew(QTemporaryFile *tfile, qint64 file_size, QString download_path, QString etag, QString mime_type, QString modified_date)
{
    Q_UNUSED(etag); // ToDo later maybe

    JsonResponse.reset();
    if (!canCreateNewProceed(tfile, file_size, download_path))
         return JsonResponse.asString(false).toUtf8();


    QString md5_hash = QString(QCryptographicHash::hash((download_path.toUtf8()), QCryptographicHash::Md5).toHex());
    QFileInfo fi_tmp(download_path);
    QString cache_path = MyLibfacade->getConfiguration()->getPaths("cache") + md5_hash + "." + fi_tmp.suffix();
    tfile->copy(cache_path);
    tfile->close();

    QFileInfo fi(cache_path);
    DB::InventoryDataset dataset;

    if (mime_type.isEmpty())
    {
        QMimeDatabase db;
        QMimeType type   = db.mimeTypeForFile(MyLibfacade->getConfiguration()->getPaths("cache") + fi.fileName());
        dataset.content_type   = type.name();
    }
    else
        dataset.content_type   = mime_type;

    dataset.resource_uri   = download_path;
    dataset.cache_name     = md5_hash + "."+fi.suffix();
    dataset.content_length = file_size;

    QDateTime dt =  QDateTime::fromString(modified_date, Qt::ISODate);
    if (dt.isValid())
        dataset.last_update = dt;
    else
        dataset.last_update = fi.birthTime();

    bool completed = true;
    dataset.state          = DB::COMPLETE;
    if (fi.size() != file_size)
    {
        dataset.state          = 0;
        completed = false;
    }
    MyLibfacade->getInventoryTable()->replace(dataset);

    JsonResponse.insertStringValuePair("downloadPath", dataset.resource_uri);
    JsonResponse.insertIntegerValuePair("fileSize", file_size);
    JsonResponse.insertStringValuePair("id", md5_hash);
    JsonResponse.insertStringValuePair("createdDate", fi.birthTime().toString(Qt::ISODate));
    JsonResponse.insertIntegerValuePair("transferredSize", fi.size());
    JsonResponse.insertBoolValuePair("completed", completed);

    return JsonResponse.asString(false).toUtf8();
}

QString RestApi::V2::Files::findPaginatedJson(QString json_string)
{
    if (!JsonRequest.readFromString(json_string))
        return respondJSONError("No JSON found");

    return findPaginated(JsonRequest.getStringValueByKey("maxResults").toInt(), JsonRequest.getStringValueByKey("pageToken").toInt());
}

QString RestApi::V2::Files::findPaginated(int max_results, int begin)
{
    QList<DB::InventoryDataset> results;

    JsonResponse.reset();
    if (max_results > 0)
        results = MyLibfacade->getInventoryTable()->findPaginated(max_results, begin);
    else
       results = MyLibfacade->getInventoryTable()->findAll();

    createJsonFromList(results);
    JsonResponse.insertIntegerValuePair("nextPageToken", (max_results + begin));

    return JsonResponse.asString(false).toUtf8();
}

QString RestApi::V2::Files::removeByIdJson(QString json_string)
{
    if (!JsonRequest.readFromString(json_string))
        return respondJSONError("No JSON found");

    return removeById(JsonRequest.getStringValueByKey("id"));
}

QString RestApi::V2::Files::removeById(QString id)
{
    DB::InventoryDataset result = MyLibfacade->getInventoryTable()->findByCacheBaseName(id);
    if (result.resource_uri.isEmpty())
        return respondJSONError("Record not found: " + id);

   QString cache_path = MyLibfacade->getConfiguration()->getPaths("cache") + result.cache_name;
   if (!MyLibfacade->getFreeDiscSpace()->deleteFile(cache_path))
       return respondJSONError("delete failed: " + id);

   MyLibfacade->getInventoryTable()->deleteByCacheName(result.cache_name);

   JsonResponse.reset();
   JsonResponse.insertStringValuePair("success", id + " deleted");
   return JsonResponse.asString(false).toUtf8();
}

bool RestApi::V2::Files::canCreateNewProceed(QTemporaryFile *tfile, quint64 file_size, QString download_path)
{
    if (tfile == Q_NULLPTR || !tfile->exists())
    {
        JsonResponse.insertStringValuePair("error", "Data field missed");
        return false;
    }

    if (file_size == 0)
    {
        JsonResponse.insertStringValuePair("error", "fileSize field missed");
        return false;
    }

    if (download_path.isEmpty())
    {
        JsonResponse.insertStringValuePair("error", "downloadPath field missed");
        return false;
    }

    qint64 calc = MyLibfacade->getFreeDiscSpace()->calculateNeededDiscSpaceToFree(file_size);
    if (calc > 0 && !MyLibfacade->getFreeDiscSpace()->freeDiscSpace(calc))
    {
        JsonResponse.insertStringValuePair("error", "Neccessary "+ QString::number(calc) +" byte cannot be relesed from disc space");
        return false;
    }

    return true;
}

void RestApi::V2::Files::createJsonFromList(QList<DB::InventoryDataset> results)
{
    QJsonArray json_array;
    for (QList<DB::InventoryDataset>::iterator i = results.begin(); i != results.end(); i++)
    {
        json_array.append(createObject(*i));
    }
    JsonResponse.insertArrayValuePair("items", json_array);

}

QJsonObject RestApi::V2::Files::createObject(DB::InventoryDataset dataset)
{
    QFileInfo fi(MyLibfacade->getConfiguration()->getPaths("cache") + dataset.cache_name);
    QJsonObject json;

    json.insert("id", fi.baseName());
    json.insert("etag", fi.baseName());
    json.insert("mimeType", dataset.content_type);
    json.insert("createdDate", fi.birthTime().toString(Qt::ISODate));
    json.insert("modifiedDate", dataset.last_update.toString(Qt::ISODate));
    json.insert("downloadPath", fi.filePath());
    json.insert("fileSize", QString::number(dataset.content_length));
    json.insert("transferredSize", fi.size());
    json.insert("completed", (dataset.state == DB::COMPLETE));
    return json;
}

QString RestApi::V2::Files::getState(int state)
{
    switch (state)
    {
        case DB::TRANSFER:
            return "COMPLETE";
        case DB::STREAMING:
            return "STREAMING";
        case DB::CHECKSUM:
            return "CHECKSUM";
        case DB::CHECKSUM_ERROR:
            return "CHECKSUM_ERROR";
        case DB::COMPLETE:
            return "COMPLETE";
        default:
            return "ERROR";
     }
}
