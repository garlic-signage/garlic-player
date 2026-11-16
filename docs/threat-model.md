# garlic-player Threat Model

This document describes which security properties the garlic-player (version 2.0, garlic-core and the Qt frontends)
is designed to provide, which behaviours are deliberate design decisions, and which weaknesses are known and
accepted for now. It exists so that design decisions arising from the deployment model of digital signage are not
mistaken for implementation defects, and so that operators know what they have to protect themselves.

Please read this document before filing a security report.

---

## 1. Deployment Model

The garlic-player runs unattended on a display device (PC, Raspberry Pi, Android, later other platforms). It is
provisioned with a content URL (the SMIL index) and then:

- fetches the index from a CMS (e.g. garlic-hub) over HTTP(S) and the media it references,
- plays images, videos, audio, web pages and widgets in regions,
- sends reports (play logs, events, inventory, task states) to the URLs the index names,
- runs tasks the CMS sends: new settings, a firmware update, a restart, clearing caches,
- offers a REST API (IAdea compatible) on its HTTP server for CMS and tools in the local network.

The CMS is trusted: it decides what the player shows, which web pages it opens and which firmware it installs.

---

## 2. Assets

| Asset | Description |
| --- | --- |
| Player UUID | Random UUID v4, created on the first start and stored in the ini; identifies the player in the CMS |
| Content URL | Address of the index, stored in the ini |
| Playback | What is shown on the screen |
| Cache | Downloaded index, media and widgets |
| Logs and reports | Play logs, events, inventory, screenshots |
| Device | The operating system the player runs on, restart and software installation through the supervisor |

---

## 3. Trust Boundaries

1. **CMS → player** (index, media, widgets, tasks, firmware): fetched by the player with libcurl. TLS certificates
   are verified (libcurl default; on Linux the system store, elsewhere the configured `ca_bundle`).
2. **Player → CMS** (reports): HTTP `PUT` to the URLs in the subscriptions of the index, with the player's user agent.
3. **Network → player** (REST API, `src/core/httpd`, `src/core/rest_api`): libmicrohttpd, by default on all IPv4
   interfaces, port 8080. See section 5.
4. **Web content → device** (web pages and widgets in Qt WebEngine): see section 6.
5. **Local user → player** (keys, menu, settings dialog): physical access to the device.

---

## 4. What the garlic-player Is Designed to Protect Against

- **Manipulated downloads in transit**: HTTPS with certificate verification for index, media, widgets and firmware.
- **Corrupted firmware downloads**: the MD5 checksum the task names is checked before the installation is handed to
  the supervisor; a wrong checksum or an unknown method fails the task (`CHECKSUM_ERROR`).
- **Path traversal in widgets** ("zip slip"): a `.wgt` with an absolute path, a drive or `..` in an entry is
  rejected as a whole (`content/widget.cpp`).
- **Memory exhaustion by requests**: request bodies above `maxRequestSize` and multipart uploads above
  `maxMultiPartSize` are refused with 413; uploads are written to the cache in pieces, never held in RAM.
- **Malformed input**: JSON is parsed without exceptions and with type checks; SMIL with libxml2. The core is tested
  with unit and scenario tests, under ASan/UBSan and TSan, with clang-tidy and mutation tests (`.github/workflows`).
- **Installation and restarts by the player itself**: the core installs nothing and restarts nothing; it hands the
  request to the supervisor (launcher, daemon, service) of the platform.

---

## 5. REST API: Known Weakness

**Behaviour.** The REST API is ported 1:1 from garlic-lib so that existing CMS and tools keep working
(`src/core/docs/rest_api.md`, "Known issues"):

- the login is fixed: user `admin` with an empty password,
- the token is `MD5(expire)` and can be guessed from the `expires_in` of the answer,
- the server listens on all IPv4 interfaces, port 8080, and is on by default.

Whoever reaches the port can therefore set the content URL (`/v2/app/start`, `/v2/app/exec`), upload and delete
files in the cache, take screenshots and restart the device (`/v2/task/reboot`).

**Accepted risk, for now.** This is a known weakness, not a design goal. A proper login is planned.

**Operator requirement.** Switch the HTTP server off when no CMS or tool uses the REST API (settings dialog or
`httpd/enabled=false` in the ini), or bind it to `127.0.0.1` (`httpd/host`), or block port 8080 at the firewall of
the device or the network. Never expose the port to the internet.

---

## 6. Web Content: Design Decision

**Behaviour.** Web pages and widgets run in Qt WebEngine (Chromium). To let local widgets load remote data, the
frontends start it with `--allow-file-access-from-files --disable-web-security` and allow local content to reach
remote URLs and files and insecure content (`qwidget/main.cpp`, `qwidget/web.cpp`, `qml/main.cpp`).

**Rationale.** Signage widgets are local HTML packages that fetch feeds and data from other origins; without these
flags most of them do not work.

**Accepted risk.** A web page or widget shown by the player is not isolated by the same-origin policy: it can read
local files the player's user may read and send them elsewhere. Only content from the trusted CMS is played.

**Operator requirement.** Play only web pages and widgets you trust. Run the player under a user of its own without
access to other data.

---

## 7. The CMS Is Trusted

The index decides what is shown and where reports go; a task can change the settings and install a firmware from a
URL the CMS names. The MD5 checksum comes from the same CMS and protects against corrupted downloads, not against a
compromised CMS.

**Operator requirement.** Use a CMS you control, over HTTPS.

---

## 8. Out of Scope

- Behaviour described in sections 5, 6 and 7, absent a demonstrated attack beyond what is described there.
- Attacks requiring physical access to the device (keys, settings dialog, storage).
- Attacks requiring control of the CMS or of the network path when the operator uses plain HTTP.
- Denial of service through raw request volume.
- Vulnerabilities in the operating system, Qt, Chromium or the device firmware; reports about a bundled library are
  welcome if the player uses the affected function.

---

## 9. In Scope

- Code execution, memory corruption or crashes caused by an index, media file, widget, task or report answer.
- Path traversal or file access outside the cache through downloads, uploads or widgets.
- Bypassing the checksum check of a firmware update.
- TLS verification that can be disabled or bypassed by content.
- Anything in the REST API beyond the known weakness of section 5.

---

## 10. Reporting

See [SECURITY.md](../SECURITY.md) for the reporting process and GPG key. Reports must include a proof of concept
demonstrating the claimed impact.
