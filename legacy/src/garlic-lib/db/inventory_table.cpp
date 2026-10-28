#include "inventory_table.h"

DB::InventoryTable::InventoryTable(QObject *parent) : QObject(parent)
{
}

DB::InventoryTable::~InventoryTable()
{
    db.close();
}

bool DB::InventoryTable::init(QString path)
{
    setDbPath(path);
    if (!db_file.exists())
    {
        if (!createDbFile() || !openDbFile() || !createTable())
        {
            db_file.remove(); // delete to try again
            return false;
        }
        return true;
    }
    else
        return openDbFile();
}

bool DB::InventoryTable::replace(DB::InventoryDataset dataset)
{
    QSqlQuery query(db);
    QString   sql;
    if (countByCacheName(dataset.cache_name) > 0)
        sql = buildInsertSql(dataset);
    else if (countByCacheName(dataset.cache_name) == -1)
    {
        qCritical(Database) << "replase failed" << sql << query.lastError().text();
        return false;
    }
    else
        sql = buildUpdateSql(dataset);

    if (!query.exec(sql))
    {
        qCritical(Database) << "replace failed" << sql << query.lastError().text();
        return false;
    }
    return true;

}

DB::InventoryDataset DB::InventoryTable::findByCacheName(QString filename)
{
    QSqlQuery query(db);
    if (!query.exec("SELECT * FROM inventory WHERE cache_name ='" +filename+"' ORDER BY last_update DESC LIMIT 1"))
        qCritical(Database) << "select failed" << query.lastError().text();

    if (!query.first())
        return InventoryDataset();

    return collectResult(&query);
}


void DB::InventoryTable::updateFileStatusAndSize(QString resource_uri, int state, int size)
{
    QSqlQuery query(db);
    if (!query.exec("UPDATE inventory SET state = " +
                    QString::number(state) +
                    ", content_length = " + QString::number(size) +
                    " WHERE resource_uri ='" +resource_uri+"'"))
        qCritical(Database) << "delete failed" << query.lastError().text();
}

void DB::InventoryTable::deleteByResourceURI(QString resource_uri)
{
    QSqlQuery query(db);
    if (!query.exec("DELETE FROM inventory WHERE resource_uri ='" +resource_uri+"'"))
        qCritical(Database) << "delete failed" << query.lastError().text();
}

void DB::InventoryTable::deleteByCacheName(QString cache_name)
{
    QSqlQuery query(db);
    if (!query.exec("DELETE FROM inventory WHERE cache_name ='" +cache_name+"'"))
        qCritical(Database) << "delete failed" << query.lastError().text();
}

DB::InventoryDataset DB::InventoryTable::findByCacheBaseName(QString base_name)
{
    QSqlQuery query(db);
    InventoryDataset result;
    query.exec("SELECT * FROM inventory WHERE cache_name LIKE '" + base_name +".%' LIMIT 1");
    if (!query.first())
        return result;

    return collectResult(&query);
}

QList<DB::InventoryDataset> DB::InventoryTable::findAll()
{
    QSqlQuery query(db);
    QList<InventoryDataset> result;
    query.exec("SELECT * FROM inventory ORDER BY UPPER(last_update) DESC");
    if (!query.first())
        return result;
    do
    {
        result.append(collectResult(&query));
    }
    while (query.next());

    return result;
}

QList<DB::InventoryDataset> DB::InventoryTable::findPaginated(int max_results, int begin)
{
    QSqlQuery query(db);
    QList<InventoryDataset> result;
    query.exec("SELECT * FROM inventory ORDER BY UPPER(last_update) DESC LIMIT " + QString::number(begin) + ", " +  QString::number(max_results));
    if (!query.first())
        return result;
    do
    {
        result.append(collectResult(&query));
    }
    while (query.next());

    return result;
}


void DB::InventoryTable::setDbPath(QString path)
{
    db_file.setFileName(path+"garlic.db");
}

bool DB::InventoryTable::createTable()
{
    QSqlQuery query(db);
    QString sql = "CREATE TABLE inventory ( \
                  resource_uri TEXT PRIMARY KEY, \
                  cache_name TEXT UNIQUE, \
                  content_type TEXT, \
                  content_length INTEGER, \
                  last_update TEXT, \
                  etag TEXT, \
                  expires TEXT, \
                  state INTEGER \
                  )";

    if (!query.exec(sql))
    {
        qCritical(Database) << "Inventory table could not be created" << query.lastError().text();
        return false;
    }
    if (!query.exec("CREATE INDEX cache_name ON inventory (cache_name collate nocase)"))
    {
        qCritical(Database) << "Index cache_name could not be created on table inventory" << query.lastError().text();
        return false;
    }
    return true;
}

bool DB::InventoryTable::dropTable(QString tableName)
{
    QSqlQuery query(db);
    QString sql = "DROP TABLE " + tableName;
    if (!query.exec(sql))
    {
        qCritical(Database) << "Table "+tableName+" could not dropped" << query.lastError().text();
        return false;
    }
    return true;
}


bool DB::InventoryTable::openDbFile()
{
    db.setDatabaseName(db_file.fileName());
    if (!db.open())
    {
        qCritical(Database) << "database file" << db_file.fileName() << "could not be created";
        return false;
    }
    if (tableExists("inventory") && !hasField("inventory", "etag"))
    {
        dropTable("inventory");
        return createTable();
    }
    QSqlQuery query(db);
    if (!query.exec("SELECT EXISTS (SELECT 1 FROM pragma_index_list('inventory') AS il JOIN pragma_index_info(il.name) AS ii ON il.unique = 1 AND ii.name = 'cache_name');"))
    {
        qCritical(Database) << "Failed to execute query:" << query.lastError().text();
        if (query.next() && !query.value(0).toBool())
        {
            dropTable("inventory");
            return createTable();
        }
    }
    return true;
}

bool DB::InventoryTable::createDbFile()
{
    if (!db_file.open(QIODevice::WriteOnly))
    {
        qCritical(Database) << "DB file could not be created";
        return false;
    }

    db_file.close();
    return true;
}

QString DB::InventoryTable::buildInsertSql(InventoryDataset dataset)
{
    return "INSERT INTO inventory (resource_uri, cache_name, content_type, content_length, last_update, expires, state, etag ) \
    VALUES( \
            '" + dataset.resource_uri + "', \
            '" + dataset.cache_name + "', \
            '" + dataset.content_type + "', \
            " + QString::number(dataset.content_length) + ", \
            '" + dataset.last_update.toString() + "', \
            '" + dataset.expires.toString() + "', \
            " + QString::number(dataset.state) + ", \
            '" + dataset.etag + "' \
        )";
}

QString DB::InventoryTable::buildUpdateSql(InventoryDataset dataset)
{
    return "UPDATE inventory SET \
           resource_uri = '" + dataset.resource_uri + "', \
           content_type = '" + dataset.content_type + "', \
           content_length = " + QString::number(dataset.content_length) + ", \
           last_update = '" + dataset.last_update.toString() + "', \
           expires = '" + dataset.expires.toString() + "', \
           state = " + QString::number(dataset.state) + ", \
           etag = '" + dataset.etag + "' \
           WHERE cache_name = '" + dataset.cache_name + "'";
}

int DB::InventoryTable::countByCacheName(QString cacheName)
{
    QSqlQuery query(db);
    if (!query.exec("SELECT COUNT(cache_name) FROM inventory WHERE cache_name ='" +cacheName+"'"))
        return -1;

    int count = 0;
    if (query.next())
        count = query.value(0).toInt();

    return count;
}


bool DB::InventoryTable::hasField(const QString &tableName, const QString &fieldName)
{
    QSqlQuery q;
    q.prepare(QStringLiteral("PRAGMA table_info(%1)").arg(tableName));
    if (!q.exec()) return false;
    while (q.next()) {
        if (q.value(1).toString() == fieldName)
            return true;
    }
    return false;
}

bool DB::InventoryTable::tableExists(const QString &tableName)
{
    QSqlQuery query(db);
    if (!query.exec(
            QString("SELECT name FROM sqlite_master WHERE type='table' AND name='%1'")
                .arg(tableName)))
        return false;
    return query.next();
}

DB::InventoryDataset DB::InventoryTable::collectResult(QSqlQuery *query)
{
    DB::InventoryDataset dataset;
    dataset.resource_uri   = query->value("resource_uri").toString();
    dataset.cache_name     = query->value("cache_name").toString();
    dataset.content_type   = query->value("content_type").toString();
    dataset.content_length = query->value("content_length").toLongLong();
    dataset.last_update    = QDateTime::fromString(query->value("last_update").toString());
    dataset.etag           = query->value("etag").toString();
    dataset.expires        = QDateTime::fromString(query->value("expires").toString());
    dataset.state          = query->value("state").toInt();
    return dataset;
}
