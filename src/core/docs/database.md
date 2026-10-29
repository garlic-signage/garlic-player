# database

SQLite storage of the core. Namespace `garlic::core::database`.

## InventoryTable

Header: `database/inventory_table.hpp`

Which URL is stored under which file name in the cache. Implements `downloader::Inventory`, so it is passed directly
to the `Downloader` / `DownloadQueue`.

```cpp
InventoryTable inventory{clock};
if (auto opened = inventory.open(log_dir + "/garlic.db"); !opened)
    log(opened.error());
DownloadQueue queue{transport, fs, inventory, clock, config, 4, on_finished};
```

| Method                       | Used by                              |
|------------------------------|--------------------------------------|
| `find(url)`, `save(entry)`   | Downloader                           |
| `findByCacheName(name)`      |                                      |
| `findByCacheBaseName(base)`  | REST API `files/{id}`                |
| `findAll()`                  | inventory report                     |
| `findPaginated(max, begin)`  | REST API files list                  |
| `deleteByCacheName(name)`    | disk space cleanup, REST API         |
| `clear()`                    | clearing the player cache (task `applyCommand`) |

Errors: `open()` returns them; the other methods return empty results and keep the message in `lastError()`. A
failing database never stops downloading or playing.

### Table

Same file (`garlic.db`) and table (`inventory`) as garlic-lib, plus the column `last_modified`:

| Column           | Content                                                |
|------------------|--------------------------------------------------------|
| `resource_uri`   | original URL, primary key                              |
| `cache_name`     | file name in the cache, unique (case-insensitive)      |
| `etag`           | ETag exactly as the server sent it                     |
| `last_modified`  | Last-Modified exactly as the server sent it            |
| `content_type`   |                                                        |
| `content_length` |                                                        |
| `state`          | 1 = transfer, 5 = complete (values of garlic-lib)      |
| `last_update`    | time of the last change, ISO 8601 UTC                  |
| `expires`        | unused, kept for compatibility                         |

`save()` is one upsert (`INSERT … ON CONFLICT DO UPDATE`). If a fixed file name such as `index.smil` moves to
another URL, the old entry is removed first.

### Differences to garlic-lib

- `replace()` in garlic-lib had insert and update swapped, so nothing was ever stored. A garlic-lib table (without
  `last_modified`) is therefore dropped and recreated on `open()`; no data is lost.
- Prepared statements instead of string concatenation: URLs with quotes work, no SQL injection.
- `last_update` in ISO 8601 instead of `QDateTime::toString()`, so sorting by it works.
- Not ported: `deleteByResourceURI` and `updateFileStatusAndSize` (no longer used).

## Decisions

- **Same file and table as garlic-lib, rewritten on the SQLite C API**, because the old code was built on QtSql. The
  used functions are ported 1:1.
- **`save()` is one upsert.** garlic-lib's `replace()` had insert and update swapped, so nothing was ever stored. That
  also means: the old downloader practically never had an ETag and always fell back to size and date.
- **Prepared statements**, because the old SQL was put together as strings: a URL with an apostrophe broke the query,
  and through the REST API it was an SQL injection.
- **New column `last_modified`, an old table is dropped.** The downloader needs the Last-Modified of the server.
  Dropping loses nothing, the old tables were empty because of the `replace()` bug.
- **`last_update` in ISO 8601**, so sorting by it is right.
- **A broken database never stops downloading or playing**: only `open()` returns an error, the rest gives empty
  results.

## When something goes wrong

- The file is `<log_dir>/garlic.db` (`PlayerRuntimeConfig::database_path` can change it; tests use `:memory:`).
- Without inventory rows the downloader cannot send conditional requests, so it downloads every medium again. This
  happens once at the first start after garlic-lib (the old table is dropped) and after a `ClearCaches` task.
- Errors after `open()` are only in `lastError()`.
- Tests: `tests/database/test_inventory_table.cpp`, among them a second `save()` that really updates and a URL with
  `' OR 1=1--`.
