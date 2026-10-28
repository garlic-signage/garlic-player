# rest_api

REST API v2 of the player (IAdea compatible). Namespace `garlic::core::rest_api`.

Ported 1:1 from garlic-lib: same paths, parameters, JSON fields, error texts and status codes, so existing clients
keep working. Behaviour that looks wrong but is visible to clients is kept on purpose (see "Known issues").

```cpp
rest_api::RestApi api{config, inventory, fs, clock};
api.registerRoutes(router);              // router of the httpd module
```

## Endpoints

| Path                  | Method | Parameters                                                        |
|-----------------------|--------|-------------------------------------------------------------------|
| `/v2/oauth2/token`    | any    | `grant_type`, `username`, `password` (form, URL or JSON)          |
| `/v2/files/new`       | any    | `data` (file), `fileSize`, `downloadPath`, `mimeType`, `modifiedDate` |
| `/v2/files/find`      | any    | `maxResults`, `pageToken` (URL, form or JSON)                     |
| `/v2/files/delete`    | any    | `id` (URL, form or JSON)                                          |
| `/v2/files/{id}`      | GET    | file info                                                         |
| `/v2/files/{id}`      | POST   | replace the file: `data`, `fileSize`, `downloadPath`, …           |

All `/v2/files` calls need `access_token` (URL or form). Not yet ported, follow with their modules: `/v2/system`,
`/v2/task`, `/v2/app`.

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
- No check of free disk space before an upload yet (garlic-lib: `FreeDiscSpace`); comes with the disk space module.

## Small differences to garlic-lib (no client can rely on the old behaviour)

- An upload replaces an existing cache file. garlic-lib used `QFile::copy`, which fails if the file exists, so a
  repeated upload silently kept the old file.
- `files/find` with a JSON body accepts numbers (`"maxResults": 10`) as well as strings; garlic-lib read numbers as 0.
- `Content-Type: application/json; charset=utf-8` counts as JSON; garlic-lib only recognised the exact
  `application/json`.
- A POST to `/v2/files/{id}` without file no longer crashes (garlic-lib dereferenced a null pointer).
- `createdDate` is the modification time of the cache file (garlic-lib: birth time, often not available on Linux).
