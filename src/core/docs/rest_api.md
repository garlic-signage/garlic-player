# rest_api

REST API v2 of the player (IAdea compatible). Namespace `garlic::core::rest_api`.

Ported 1:1 from garlic-lib: same paths, parameters, JSON fields, error texts and status codes, so existing clients
keep working. Behaviour that looks wrong but is visible to clients is kept on purpose (see "Known issues").

```cpp
rest_api::RestApi api{config, inventory, fs, clock, actions};   // actions: what only the runtime can do
api.registerRoutes(router);              // router of the httpd module
api.poll();                              // with every poll of the core: screenshots that did not come in time
```

The REST API does not know the `PlayerRuntimeApi`. What it cannot do itself it gets as `RestActions`: `notify`,
`reboot`, `reloadIndex`, `playTemporary`, `screenshot`, `makeSpace` (empty functions do nothing).

## Endpoints

| Path                  | Method | Parameters                                                        |
|-----------------------|--------|-------------------------------------------------------------------|
| `/v2/oauth2/token`    | any    | `grant_type`, `username`, `password` (form, URL or JSON)          |
| `/v2/files/new`       | any    | `data` (file), `fileSize`, `downloadPath`, `mimeType`, `modifiedDate` |
| `/v2/files/find`      | any    | `maxResults`, `pageToken` (URL, form or JSON)                     |
| `/v2/files/delete`    | any    | `id` (URL, form or JSON)                                          |
| `/v2/files/{id}`      | GET    | file info                                                         |
| `/v2/files/{id}`      | POST   | replace the file: `data`, `fileSize`, `downloadPath`, …           |
| `/v2/system/firmwareInfo` | any | `{"firmwareVersion", "family": "garlic-player-<os>"}`             |
| `/v2/system/modelInfo`    | any | `{"modelName"/"PCB": "garlic-player", "licenseModel": <os>, "manufacturer": "Sagiadinos", …}` |
| `/v2/app/start`       | any    | `uri` (+ `packageName`, `className`): store as `index_uri`, load it |
| `/v2/app/exec`        | any    | like `start`, but play `uri` for a while; `index_uri` stays       |
| `/v2/app/switch`      | any    | `mode=start`: play the stored index again                         |
| `/v2/task/notify`     | any    | `smilEvent`: `notify(…)` triggers of the engine                    |
| `/v2/task/reboot`     | any    | restart the device (`Reboot{"httpd: <local time>"}` to the host)  |
| `/v2/task/screenshot` | any    | the frontend saves a JPEG; the answer waits for it                |

All calls except the token need `access_token` (URL or form); parameters come as form, URL or JSON
(`Content-Type: application/json`). `/v2/system`, `/v2/app` and `/v2/task` check the token first, then the name: an
unknown name is 403 without and 404 with a valid token (RequestMapper of garlic-lib).

`/v2/files/new` makes room for `fileSize` first (`RestActions::makeSpace`, the cache cleanup of the content module);
if it cannot, the answer is `{"error": "Neccessary <bytes> byte cannot be relesed from disc space"}` with the spelling
of garlic-lib. `/v2/files/{id}` did not check in garlic-lib and does not now.

### app, task and system in detail

- `app/start` and `app/exec` answer `{"uri", "packageName", "className", "data": uri}` with the uri as sent; an
  invalid uri gives the error text of `Configuration::validateIndexUri`. `app/switch` answers the stored `index_uri`,
  another mode `{"error":"wrong mode"}`. `Action` and `Type` are read by garlic-lib but not used, so they are not.
- `app/exec` plays the index of `uri` with its own refresh in `cache/temporary.smil` until `app/start`,
  `app/switch` or a restart; it is not stored as `index_uri` or as last played index. garlic-lib downloaded it as
  `index.smil` and set the source back to `index_uri` at once, so the temporary content stayed until the stored
  index changed.
- `task/notify` answers `{}`, without `smilEvent` `{"error":" No SmilEvent exists"}` (with the space of garlic-lib).
- `task/reboot` answers an empty body (JSON headers) and hands `Reboot{"httpd: 2026-10-28T08:00:00"}` to the host.
- `task/screenshot`: the host gets `Screenshot{path}` with `path` = `<log dir>/yyyyMMddhhmmss.jpg` and calls
  `screenshotTaken(path)` when the JPEG is saved. The answer is held back meanwhile (`Response::later`, the server
  serves other requests); then `image/jpeg`, or `file not created.` / `file not readable.` like garlic-lib. Without
  an answer of the host after 5 s it is `file not created.`. garlic-lib waited one second in the event loop and sent
  whatever was there.

## Answers

- Success and errors: status **200**, `Content-Type: application/json; charset=utf-8`,
  `Cache-Control: no-cache,no-store`, `Pragma: no-cache`. Errors are `{"error":"…"}`.
- Wrong or expired token: status **403**, body `{'error':'ACCESSVIOLATION'}`.
- Unknown path: status **404**, body `The URL is wrong, no such document.`
- `/v2/files/{id}` with a method other than GET or POST: status 200 with an empty body.
- JSON keys are sorted alphabetically, like QJsonObject. In file entries `fileSize` is a string, `downloadPath` is the
  local cache path (both like garlic-lib); in the answer of an upload `fileSize` is a number.

## Known issues (kept for compatibility, security comes later)

- Login is fixed: user `admin` with an **empty password**.
- The token is `MD5(expire)` in uppercase: garlic-lib passed its never-set username/password members to the hash.
  Since `expires_in` is in the answer, the token can be guessed.
- The 403 body is no valid JSON (single quotes).

## Small differences to garlic-lib (no client can rely on the old behaviour)

- An upload replaces an existing cache file. garlic-lib used `QFile::copy`, which fails if the file exists, so a
  repeated upload silently kept the old file.
- `files/find` with a JSON body accepts numbers (`"maxResults": 10`) as well as strings; garlic-lib read numbers as 0.
- `Content-Type: application/json; charset=utf-8` counts as JSON; garlic-lib only recognised the exact
  `application/json`.
- A POST to `/v2/files/{id}` without file no longer crashes (garlic-lib dereferenced a null pointer).
- `createdDate` is the modification time of the cache file (garlic-lib: birth time, often not available on Linux).

## Decisions

- **1:1, nothing visible to clients changes.** Clients rely on the API as it is; security comes much later. So the
  fixed login, the guessable token, status 200 with errors and the invalid 403 body stay, and every one of these
  details is pinned by a test, so it cannot get lost by accident. Changed was only what crashed or never worked (see
  the small differences above).
- **The REST API does not know the `PlayerRuntimeApi`**; what it cannot do itself comes as functions (`RestActions`).
  So it is tested without the runtime.
- **Nothing prepared for a v3 or another login.** The router works by paths, a v3 would be another class registering
  `/v3/…` routes on the same router (the 404 fallback then moves to the host). The whole token check is in two places,
  `validToken()` and `token()`; another login becomes a small interface there when it is needed.
- **nlohmann/json**: the C++ standard, header-only, parsed without exceptions (`parse(…, nullptr, false)`, types
  checked before access). yyjson would be smaller, but its API is much more clumsy.
- **The screenshot answer is held back** until the frontend has saved the JPEG, at most 5 s (see
  [httpd](httpd.md#answers-that-come-later)).
- **`app/exec` plays a temporary index** until `app/start`, `app/switch` or a restart; garlic-lib's mix of
  downloading it as `index.smil` and switching back at once is gone.

## When something goes wrong

- **403 although the login worked**: the token is valid one day (`api_access_token_expire`, UTC) and is checked against
  the ini.
- **404 with text** `The URL is wrong, no such document.`: unknown path; under `/v2/system`, `/v2/app` and `/v2/task`
  an unknown name is 403 without and 404 with a valid token, like garlic-lib.
- **Errors come with status 200** and `{"error":"…"}`; a client that checks only the status sees no error.
- **Screenshot answers `file not created.`**: the frontend did not report `screenshotTaken()` within 5 s.
- Tests: `tests/rest_api/test_rest_api.cpp`, one test per endpoint and per quirk.
