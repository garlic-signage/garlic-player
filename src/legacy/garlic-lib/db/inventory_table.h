#ifndef INVENTORYTABLE_H
#define INVENTORYTABLE_H

#include <QtSql>
#include <QList>
#include <QtDebug>
#include <QDateTime>
#include <QFile>
#include "logger.h"

namespace DB
{
    struct InventoryDataset
    {
            QString resource_uri  = "";
            QString cache_name     = "";
            QString content_type   = "";
            qint64  content_length = 0;
            QDateTime last_update  = QDateTime();
            QString etag = "";
            QDateTime expires      = QDateTime();
            int       state        = 0;

    };
    enum state
    {
        TRANSFER          = 1,
        STREAMING         = 2,
        CHECKSUM          = 3,
        CHECKSUM_ERROR    = 4,
        COMPLETE          = 5
    };

    class InventoryTable : public QObject
    {
            Q_OBJECT
        public:
            explicit InventoryTable(QObject *parent = nullptr);
            ~InventoryTable();
            bool init(QString path);
            bool replace(InventoryDataset dataset);
            void updateFileStatusAndSize(QString resource_uri, int state, int size);
            void deleteByResourceURI(QString resource_uri);
            void deleteByCacheName(QString cache_name);
            QList<DB::InventoryDataset> findPaginated(int max_results, int begin = 0);
            QList<InventoryDataset> findAll();
            DB::InventoryDataset findByCacheName(QString filename);
            DB::InventoryDataset findByCacheBaseName(QString base_name);
            void setDbPath(QString path);
        private:
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "SQLITE");
            QFile db_file;
            bool createDbFile();
            bool openDbFile();
            bool createTable();
            bool dropTable(QString tableName);
            bool tableExists(const QString &tableName);
            QString buildInsertSql(InventoryDataset dataset);
            QString buildUpdateSql(InventoryDataset dataset);
            int countByCacheName(QString cacheName);
            bool hasField(const QString &tableName, const QString &fieldName);
            InventoryDataset collectResult(QSqlQuery *query);
    };
}
#endif // INVENTORYTABLE_H
