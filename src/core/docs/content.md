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
- Events like in garlic-lib: `SMIL_LOADED`, `SMIL_OPEN_ERROR` (with `errorMessage`, `errorLine`), `SMIL_NOT_AVAILABLE`,
  with `instanceId` `display:0`, `resourceURI`, `contentLength`, `lastModifiedTime`.
- Without any valid index (first start, broken file) the manager tries again every 60 s. garlic-lib restarted the
  player (`reboot("index_broken")`), and after the restart found the broken file again.
- garlic-lib stopped the playing body first and renamed the download over the old index before parsing it. Now the
  new document is parsed while the old one plays and swapped only if it is valid.

## MediaManager

`load(document, base)` registers the media of a new index (garlic-lib: `registerFile`):

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
- not cacheable (a web page) → the URL itself, unless `cacheControl="onlyIfCached"`
- an update waits as `<file>.ready`; it replaces the file at the next `resolve()` when no instance plays the file any
  more. The engine reports the end of an instance with `release()`, this replaces `currently_playing` of garlic-lib
  that the player had to keep.
- widgets → the extracted `index.html`; the engine appends the query

`supports` comes from the frontend and says what the player can show, e.g. no video on an e-ink display.


## Widgets

A `.wgt` is a zip archive. It is extracted next to the file: `cache/abc.wgt` → `cache/abc/`, played is
`cache/abc/index.html`. miniz reads the archive; it is one C file, also on WASM, and ESP-IDF brings it along.

- Extracted again only if the file is newer than the extracted `index.html` (garlic-lib: `mustExtract`).
- Extracted into `<directory>.part` first; only when everything worked it replaces the directory. A broken archive
  keeps the old widget. garlic-lib deleted the directory first.
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
├── keep: index.smil, temporary.smil, QtWebEngine, groups with a ".part" file (download), ".tmp" files (upload)
├── sort by the last read (lastAccess, the newest file of the group), the longest unread first
├── remove group by group until the removed bytes reach needed:
│     files and directories, the inventory row of the cache file, event OBJECT_REMOVED
└── enough: OBJECTS_REMOVED, true      not enough: FAILED_FREEING_SPACE, false
```

- `free_space` comes from the host: `system::diskSpace(cache_dir).free`, the available bytes minus 4 % for the
  operating system, like garlic-lib. If the disk is unknown, nothing is cleaned up; without the system module
  (ESP32, WASM) the frontend gives its own or the cache is never cleaned up.
- Media of the current index can go, also the one that is playing, like in garlic-lib: otherwise a cache filled by
  the index would block every download. A removed medium is downloaded again when it is needed.
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
- **Cache cleanup: the index is kept, media may go, also the playing one** (like garlic-lib). If the media of the
  current index were kept, a cache filled by the index would block every further download; a removed medium is
  downloaded again when it is needed.
- **`.tmp` files are kept**: the HTTP server writes running uploads as `upload-N.tmp` into the cache directory, the
  cleanup for `/v2/files/new` would otherwise remove its own upload.
- **Unknown disk space: no cleanup.** Otherwise an error of `diskSpace` would count as "0 free" and empty the cache.
- The name of the module comes from garlic-lib's `ContentManager`, which is also the event source in the log.

## When something goes wrong

- **Events** (source `ContentManager`): `SMIL_LOADED`, `SMIL_OPEN_ERROR` (with `errorMessage` and `errorLine`),
  `SMIL_NOT_AVAILABLE`, `MEDIA_UNSUPPORTED`, `OBJECT_REMOVED`, `OBJECTS_REMOVED`, `FAILED_FREEING_SPACE`.
- **debug.log**: `index download failed: …`, `cannot replace index: …`, a widget that cannot be extracted
  (`<path>: <reason>`), a cache file that cannot be removed.
- **The new playlist does not come**: is there an `index.smil.ready` (an update waiting, deleted if broken), a
  `SMIL_OPEN_ERROR`, or is the refresh still running (at least 60 s, `0` = never)?
- **A medium is skipped again and again**: not downloaded yet (`resolve()` gives nothing), not supported by the
  frontend (`MEDIA_UNSUPPORTED`), or `cacheControl="onlyIfCached"` on a web page.
- **Leftovers**: a `.part` of a download that is never started again and a `.tmp` after a crash stay in the cache.
- **Order of the cleanup** follows the atime, see [common](common.md#when-something-goes-wrong).
- Tests: `tests/content/test_index_manager.cpp`, `test_media_manager.cpp`, `test_widget.cpp`,
  `test_cache_space.cpp`, all on a `MemoryFileSystem` with the fake transport (`content_fixture.hpp`).
