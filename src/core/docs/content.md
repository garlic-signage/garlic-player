# content

The SMIL index and its media: download, check, hand on. Namespace `garlic::core::content`. garlic-lib: `IndexManager`,
`MediaManager`, `MediaModel`, `Wgt`.

```cpp
IndexManager index(queue, fs, clock, logger, config, cache_dir,
                   [&](smil::Document document, std::string base) { /* swap document + engine, media.load() */ });
CacheSpace   space(fs, inventory, logger, cache_dir, free_space);     // free_space: free bytes of the disk
MediaManager media(queue, inventory, fs, logger, cache_dir, supports,   // the smil::MediaSource of the engine
                   [&](std::uint64_t bytes) { return space.makeSpace(bytes); });

index.start();
// host loop: transport.poll(); queue.poll(); index.poll(); engine.poll() …
// download results: the one for index.url() to index.downloaded(), all others to media.downloaded()
```

| File                     | Content                                                     |
|--------------------------|-------------------------------------------------------------|
| `index_manager.hpp/.cpp` | `IndexManager`: get, check and hand on the SMIL index       |
| `media_manager.hpp/.cpp` | `MediaManager`: the media in the cache, `smil::MediaSource` |
| `widget.hpp/.cpp`        | `extractWidget()`: unzip `.wgt` with miniz                  |
| `cache_space.hpp/.cpp`   | `CacheSpace`: remove old files when the disk is full        |

## IndexManager

Only valid documents are handed on. A broken index leaves the current one playing; the old index file is never
replaced by a broken one.

```
start()
├── local index (path, file://, USB)  read and hand on, no download, no refresh (like garlic-lib)
└── remote index
    ├── the last played index (cache/index.smil) plays at once, also offline
    └── download as cache/index.smil
         ├── first version               parse → hand on
         ├── update (index.smil.ready)   parse first: valid → replaces index.smil → hand on
         │                                        broken → deleted, SMIL_OPEN_ERROR, the current one plays on
         └── not modified / failed        nothing; without any index yet the cached copy is tried
refresh: meta http-equiv="Refresh" of the index, at least 60 s, 0 = never

startTemporary(uri)                   REST /v2/app/exec: another index for a while
└── like a remote/local index, but in cache/temporary.smil, not stored as index_uri or last played;
    start() goes back to the stored index
```

- Handed on with the document is the **base** for relative `src`: the directory of a local index or the URL of a
  remote one up to the last `/` (query removed), garlic-lib: `getIndexPath`.
- The handed on file is stored as `last_played_index_path` in the configuration.
- If the cached index was removed (by hand), the next check downloads it fully, without `.ready`; it is handed on
  like an update, also when it is the same, so its media are registered again.
- Events like in garlic-lib: `SMIL_LOADED`, `SMIL_OPEN_ERROR` (with `errorMessage`, `errorLine`), `SMIL_NOT_AVAILABLE`,
  with `instanceId` `display:0`, `resourceURI`, `contentLength`, `lastModifiedTime`.
- Without any valid index (first start, broken file) the manager tries again every 60 s. garlic-lib restarted the
  player (`reboot("index_broken")`), and after the restart found the broken file again.
- garlic-lib stopped the playing body first and renamed the download over the old index before parsing it. Now the
  new document is parsed while the old one plays and swapped only if it is valid.

## MediaManager

`load(document, base)` registers the media of a new index (garlic-lib: `registerFile`): first the background images,
then images, widgets and web pages, then video and audio, each in document order.

| `src`                                   | Handling                                                          |
|-----------------------------------------|-------------------------------------------------------------------|
| not supported (`supports` says no)      | not downloaded, event `MEDIA_UNSUPPORTED` (contentId, resourceURI, mediaType), once per index version |
| `http://`, `https://`, `ftp://`         | download queue; a version in the cache is playable at once, also offline |
| relative                                | against the base of the index, then as above                     |
| absolute path, `file://`, `data:`       | used as it is                                                     |
| `adapi:…`                               | used as it is                                                     |
| local `.wgt`                            | extracted next to the file                                        |

Region and root-layout backgrounds are registered too; the PlayerRuntimeApi finds their paths with `locate(src)` and
hands them to the frontend with `NewLayout` or `BackgroundImage`. The playback does not wait for them (SMIL standard: no rule to wait for media; garlic-lib waited up to 10 s).

`resolve(media)` for the engine (garlic-lib: `getLoadablePath`, `checkCacheStatus`):

- not supported, or not downloaded yet → `nullopt`: the engine skips it and tries again next time
- the cache file comes from the inventory of the downloader (URL → file name)
- missing in the cache (no inventory row or no file: removed by hand, by the REST API, by the cleanup) → `nullopt`,
  and the URL goes to the download queue again, see [Missing files](#missing-files)
- not cacheable (a web page) → the URL itself, unless `cacheControl="onlyIfCached"`
- a web page (`text`) whose download check is not done yet → the URL itself as well, unless
  `cacheControl="onlyIfCached"`. Otherwise it would be skipped at the first start and never come in a container that
  does not repeat (garlic-lib waited up to 10 s at the start).
- an update waits as `<file>.ready`; it replaces the file at the next `resolve()` when no instance plays the file any
  more. The engine reports the end of an instance with `release()`, this replaces `currently_playing` of garlic-lib
  that the player had to keep.
- widgets → the extracted `index.html`; the engine appends the query

`supports` comes from the frontend and says what the player can show, e.g. no video on an e-ink display.

### Missing files

Files of the index are downloaded when the index is loaded. If one of them is gone later, `resolve()` (and
`locate()` for backgrounds) puts its URL into the download queue again; the queue takes a URL only once. So a cache
that was emptied by hand fills again while the playlist runs, without a new index.

```
downloaded(result)
├── not Failed                       the URL may be asked for again at once
├── Failed                           not again before IndexManager::retry (60 s), also not by <prefetch>
└── Failed with no_space             event DISK_FULL (resourceURI, errorMessage), not again before the next index
load(next index)                     all waits are forgotten: every file is tried once more
```

- **The wait after an error**: a URL that the server does not have (404) or a server that cannot be reached would
  otherwise be asked for every time the element is due; with many short images that is a request every few seconds.
  The retry time of the index is used, no own setting.
- **No new try after `DISK_FULL`**: the files of the index are kept by the cleanup (see below), so the space does not
  get bigger until the index changes; trying again would only fail again and cost traffic.
- When the server is reachable again, a missing file comes at the latest after one retry time.
- **After `clear_cache` nothing is downloaded again until the restart** (`holdDownloads()`): the cache was emptied on
  purpose, the restart follows when the frontend has cleared its web cache. The index refresh is not held.
- garlic-lib downloaded the media only when the index was loaded again; a file removed from the cache came back only
  with a new index or a restart.


## Widgets

A `.wgt` is a zip archive. It is extracted next to the file: `cache/abc.wgt` → `cache/abc/`, played is
`cache/abc/index.html`. miniz reads the archive; it is one C file, also on WASM, and ESP-IDF brings it along.

- Extracted again only if the file is newer than the extracted `index.html` (garlic-lib: `mustExtract`).
- Extracted into `<directory>.part` first; only when everything worked it replaces the directory: the old one is
  moved to `<directory>.old`, the new one into place, then `.old` is removed. If the new one cannot take its place, the
  old one is moved back. A broken archive keeps the old widget. garlic-lib deleted the directory first.
- Entries with an absolute path, a drive or `..` reject the whole archive ("zip slip"). garlic-lib did not check.
- Before extracting, `make_space` gets the unpacked size (garlic-lib: `calculateUncompressedSize`); if it cannot be
  made free, the widget is not extracted.
- The directory is the whole name without `.wgt`. garlic-lib compared with `baseName()`, which cuts at the first dot,
  so `xyz.secret.wgt` was extracted again at every check.

## Space in the cache (CacheSpace)

garlic-lib: `FreeDiscSpace`. Asked before a download (`make_space` of the downloader with the size from the server),
before a widget is extracted and before an upload of the REST API (`/v2/files/new`) is stored.

```
makeSpace(bytes)
├── needed = bytes + 10 - free_space()             0: fits, nothing to do (garlic-lib: calculateNeededDiscSpaceToFree)
├── list the cache directory, group the entries by their cache file
│     a.jpg, a.jpg.part, a.jpg.ready → a.jpg        widget w.wgt with its directory w (and w.part) → w.wgt
├── keep: index.smil, temporary.smil, the files of the current index (MediaManager::currentFiles),
│         groups with a ".part" file (download), ".tmp" files (upload)
├── sort by the last read (lastAccess, the newest file of the group), the longest unread first
├── remove group by group until the removed bytes reach needed:
│     files and directories, the inventory row of the cache file, event LRU_OBJECT_REMOVED
└── enough: OBJECTS_REMOVED, true      not enough: FAILED_FREEING_SPACE, false
```

- `free_space` comes from the host: `system::diskSpace(cache_dir).free`, the available bytes minus 4 % for the
  operating system, like garlic-lib. If the disk is unknown, nothing is cleaned up; without the system module
  (ESP32, WASM) the frontend gives its own or the cache is never cleaned up.
- **The files of the current index are kept.** If the index needs more than the disk has, it does not push its own
  files out again and again: what does not fit is not downloaded, the download fails with `no_space` and the media
  manager reports `DISK_FULL`. The rest of the playlist plays, the missing elements are skipped. The files of the
  previous index are not kept; a file that is still playing is left to the operating system (Linux: it stays readable
  until it is closed; Windows: it cannot be removed, that is logged and counts as not freed).
- `currentFiles()` are the cache file names of the remote `src` of the index, from the inventory; files not downloaded
  yet have no name and are not in the cache anyway.
- The last read is the atime of the file system. Linux mounts with `relatime` update it at most once a day, `noatime`
  never; then the order is the one of the downloads. garlic-lib had the same (`QFileInfo::lastRead`).
- New: groups. garlic-lib removed a widget directory with its `.wgt` too, but left `.ready` and `.part` files, and
  a file with a running download could go.

## Decisions

- **Parse first, swap only if valid.** garlic-lib stopped the playing body, renamed the download over the old index
  and parsed afterwards; a broken index made it restart, and after the restart it found the broken file again. Now the
  same rule as in the downloader holds: on an error the old one plays on.
- **Broken means only what `parse()` rejects.** A valid SMIL with an empty body is no error, it may be wanted (a black
  screen).
- **Without any valid index: try again every 60 s** instead of garlic-lib's restart, which looped.
- **The last played index first**, so the player plays at once, also offline, and only then asks the hub.
- **The engine reports the end of an instance (`release()`)** instead of the list `currently_playing` that the player
  had to keep in garlic-lib. So `resolve()` is a pure query; garlic-lib's `checkCacheStatus` swapped `.ready` files
  as a side effect of asking.
- **Background images are not waited for** (SMIL standard: the timing does not wait for media; garlic-lib waited up to
  10 s, and after one index with a background image every further index too). The frontend shows them when they come.
- **`MEDIA_UNSUPPORTED` once per index version**, so an endless playlist does not flood the event log.
- **miniz for widgets**: one C file under MIT, runs on WASM and comes with ESP-IDF; libzip and minizip-ng are bigger
  and need zlib. Extracted into `<directory>.part` and swapped afterwards, so a broken archive or a full disk keeps the
  old widget (garlic-lib deleted the directory first). Entries with `..` or an absolute path are rejected ("zip
  slip"), garlic-lib did not check.
- **Cache cleanup: the index and its files are kept.** garlic-lib removed media of the current index too, also the
  playing one. If the index is bigger than the disk, its files then push each other out, and every round of the
  playlist downloads them again (like IAdea players do): traffic limits, wear of the flash and gaps when a file is not
  there in time. Now what does not fit is not downloaded, the hub gets `DISK_FULL` and the operator makes the playlist
  smaller. A cache filled by the index also blocks other downloads (tasks, firmware) until the index changes.
- **Small files first.** garlic-lib queued the media in document order, so a playlist that starts with large videos
  showed nothing for minutes while two downloads ran on them. Now images, widgets and web pages go into the download
  queue before video and audio: the first images play after seconds while the videos still load.
- **Missing files come back without a new index.** `resolve()` asks for them again, after an error at most once per
  retry time of the index, after `DISK_FULL` only with the next index (garlic-lib: only with a new index).
- **No `QtWebEngine` in the cache any more.** garlic-lib kept the web cache directory of the Qt players there; the
  frontends of the core keep their web cache in the data directory.
- **`.tmp` files are kept**: the HTTP server writes running uploads as `upload-N.tmp` into the cache directory, the
  cleanup for `/v2/files/new` would otherwise remove its own upload.
- **Unknown disk space: no cleanup.** Otherwise an error of `diskSpace` would count as "0 free" and empty the cache.
- The name of the module comes from garlic-lib's `ContentManager`, which is also the event source in the log.

## When something goes wrong

- **Events** (source `ContentManager`): `SMIL_LOADED`, `SMIL_OPEN_ERROR` (with `errorMessage` and `errorLine`),
  `SMIL_NOT_AVAILABLE`, `MEDIA_UNSUPPORTED`, `OBJECTS_REMOVED`, `FAILED_FREEING_SPACE`,
  `LRU_OBJECT_REMOVED` (IAdea; garlic-lib called it `OBJECT_REMOVED`: an entry the cleanup removed, `resourceURI`
  and `lastAccessTime`; informational if not read for more than 30 days, else a warning: the disk is too small),
  `BROKEN_OBJECT_REMOVED` (IAdea, at the start: a complete file of the inventory whose size is not the one of the
  server, or that is gone, is removed with its row and downloaded again; `resourceURI`, `lastModifiedTime`,
  `contentLength` as the server told, `transferLength` as found, `-1` if gone),
  `DISK_FULL` (with `resourceURI` and `errorMessage`: the file that did not fit and its size). For every download
  (index, media, task files, firmware; written by `PlayerRuntimeApi`, as garlic-lib and IAdea): `OBJECT_UPDATED`
  (`resourceURI`, `contentLength`, `lastModifiedTime`) for a new file, `FETCH_FAILED` (`resourceURI`,
  `errorMessage`, e.g. `HTTP 404`) for a failure, a warning with `lastCachedLength` and `lastCachedModifiedTime` if
  a cached copy plays on, an error without. Not for a download without space: that is `DISK_FULL`.
- **debug.log**: `index download failed: …`, `cannot replace index: …`, a widget that cannot be extracted
  (`<path>: <reason>`), a cache file that cannot be removed.
- **The new playlist does not come**: is there an `index.smil.ready` (an update waiting, deleted if broken), a
  `SMIL_OPEN_ERROR`, or is the refresh still running (at least 60 s, `0` = never)?
- **A medium is skipped again and again**: not downloaded yet (`resolve()` gives nothing), not supported by the
  frontend (`MEDIA_UNSUPPORTED`), `cacheControl="onlyIfCached"` on a web page, the download failed (asked for again
  after 60 s), or it did not fit (`DISK_FULL`: only with the next index).
- **Leftovers**: a `.part` of a download that is never started again and a `.tmp` after a crash stay in the cache. A
  widget directory `<directory>.old` after a crash goes with the next extraction or the cache cleanup.
- **Order of the cleanup** follows the atime, see [common](common.md#when-something-goes-wrong).
- Tests: `tests/content/test_index_manager.cpp`, `test_media_manager.cpp`, `test_widget.cpp`,
  `test_cache_space.cpp`, all on a `MemoryFileSystem` with the fake transport (`content_fixture.hpp`).
