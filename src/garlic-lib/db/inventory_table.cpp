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
    int count = countByCacheName(dataset.cache_name);
    if (count == -1)
    {
        qCritical(Database) << "replace failed" << query.lastError().text();
        return false;
    }
    QString sql = (count > 0) ? buildInsertSql(dataset) : buildUpdateSql(dataset);
    query.prepare(sql);
    query.bindValue(":resource_uri",    dataset.resource_uri);
    query.bindValue(":cache_name",      dataset.cache_name);
    query.bindValue(":content_type",    dataset.content_type);
    query.bindValue(":content_length",  dataset.content_length);
    query.bindValue(":last_update",     dataset.last_update.toString());
    query.bindValue(":expires",         dataset.expires.toString());
    query.bindValue(":state",           dataset.state);
    query.bindValue(":etag",            dataset.etag);
    if (!query.exec())
    {
        qCritical(Database) << "replace failed" << sql << query.lastError().text();
        return false;
    }
    return true;
}

DB::InventoryDataset DB::InventoryTable::findByCacheName(QString filename)
{
    QSqlQuery query(db);
    query.prepare("SELECT * FROM inventory WHERE cache_name = :cache_name ORDER BY last_update DESC LIMIT 1");
    query.bindValue(":cache_name", filename);
    if (!query.exec())
        qCritical(Database) << "select failed" << query.lastError().text();

    if (!query.first())
        return InventoryDataset();

    return collectResult(&query);
}


void DB::InventoryTable::updateFileStatusAndSize(QString resource_uri, int state, int size)
{
    QSqlQuery query(db);
    query.prepare("UPDATE inventory SET state = :state, content_length = :size WHERE resource_uri = :resource_uri");
    query.bindValue(":state", state);
    query.bindValue(":size", size);
    query.bindValue(":resource_uri", resource_uri);
    if (!query.exec())
        qCritical(Database) << "delete failed" << query.lastError().text();
}

void DB::InventoryTable::deleteByResourceURI(QString resource_uri)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM inventory WHERE resource_uri = :resource_uri");
    query.bindValue(":resource_uri", resource_uri);
    if (!query.exec())
        qCritical(Database) << "delete failed" << query.lastError().text();
}

void DB::InventoryTable::deleteByCacheName(QString cache_name)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM inventory WHERE cache_name = :cache_name");
    query.bindValue(":cache_name", cache_name);
    if (!query.exec())
        qCritical(Database) << "delete failed" << query.lastError().text();
}

DB::InventoryDataset DB::InventoryTable::findByCacheBaseName(QString base_name)
{
    QSqlQuery query(db);
    InventoryDataset result;
    query.prepare("SELECT * FROM inventory WHERE cache_name LIKE :pattern LIMIT 1");
    query.bindValue(":pattern", base_name + ".%");
    query.exec();
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
    Q_UNUSED(dataset);
    return "INSERT INTO inventory (resource_uri, cache_name, content_type, content_length, last_update, expires, state, etag) "
           "VALUES(:resource_uri, :cache_name, :content_type, :content_length, :last_update, :expires, :state, :etag)";
}

QString DB::InventoryTable::buildUpdateSql(InventoryDataset dataset)
{
    Q_UNUSED(dataset);
    return "UPDATE inventory SET resource_uri = :resource_uri, content_type = :content_type, "
           "content_length = :content_length, last_update = :last_update, expires = :expires, "
           "state = :state, etag = :etag WHERE cache_name = :cache_name";
}

int DB::InventoryTable::countByCacheName(QString cacheName)
{
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(cache_name) FROM inventory WHERE cache_name = :cache_name");
    query.bindValue(":cache_name", cacheName);
    if (!query.exec())
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
