# smil

SMIL parser. Namespace `garlic::core::smil`.

```cpp
auto document = smil::parse(xml);          // std::expected<Document, ParseError>
if (!document)
    logger.error("SmilParser", std::format("line {}: {}", document.error().line, document.error().message));
else
    for (const auto &region : document->head.regions) ...
```

The parser only reads: it turns the SMIL into plain data and has no timers, signals, downloads or other side effects.
Playing (timing, `seq`/`par`/`excl`, triggers) is the business of the engine, starting reports and tasks or waiting
for background images the business of the integration. libxml2 is only used in `parser.cpp`; network access is
disabled.

`parse` fails only if the XML is broken (with line number) or the root element is not `<smil>`. Everything else
that is missing gets the default values of garlic-lib, unknown elements and attributes are ignored. Element names
are compared without namespace prefix.

Status: the parser is complete, and so are the runtime functions `wallclockTriggers` and `evaluateExpr`. The engine
plays media, `seq` and `par` with `begin`/`end` offsets, `restart` and `expr`; `excl` and triggers (wallclock,
syncbase, events, accesskey, notify) follow step by step.

## How the parser works

### Files

| File           | Content                                                                 |
|----------------|-------------------------------------------------------------------------|
| `document.hpp` | the result: plain structs with the defaults of garlic-lib at the fields |
| `parser.hpp`   | the function `parse()` and `ParseError`                                 |
| `parser.cpp`   | everything else of the parser                                           |
| `wallclock.hpp/.cpp` | `wallclockTriggers()`: when a wallclock fires (runtime)           |
| `expr.hpp/.cpp`      | `evaluateExpr()`: whether an `expr` is true (runtime)             |
| `engine.hpp/.cpp`    | `Engine`: plays a `Document`, events in, commands out             |

libxml2 is only used in `parser.cpp` and `expr.cpp`.

### Flow

```
parse(xml)                         libxml2 reads the text, checks the root element <smil>
└── Parser::parse                  head first: the body needs its placeholders
    ├── parseHead                  <head>
    │   ├── parseMeta              <meta name="title"> <meta http-equiv="Refresh">
    │   ├── parseMetadata          <metadata> … <subscription>
    │   ├── parseLayout            <layout>
    │   │   ├── parseRootLayout    <root-layout>
    │   │   └── parseRegion        <region>, one per region
    │   └── parseState             <state><data>: placeholders
    └── parseContainer             <body>, <seq>, <par>, <excl>
        ├── parseTiming            begin end dur repeatCount restart
        │   └── parseTimeList      begin/end lists, parseTimeCondition per value
        ├── parseEndSync           <par|excl endsync>
        ├── parseChildren          children of body, seq, par, priorityClass
        │   ├── parsePickNumber    <seq><metadata> as first child
        │   ├── parseContainer     nested <seq> <par> <excl>: recursion
        │   └── parseMedia         <img> <video> … one per medium
        │       ├── parseTiming
        │       └── parseParams    <param name value>
        └── parsePriorityClasses   children of an excl
            └── parsePriorityClass <priorityClass>, then parseChildren
```

Every function reads one element and fills one struct. Above each function in `parser.cpp` is the SMIL snippet it
reads and the garlic-lib class that did it before.

`Parser` is a class only because it counts the generated ids during one run. Everything without state is a free
function in an anonymous namespace, invisible outside `parser.cpp`.

### Helpers

garlic-lib used QDom and `QString`; these small functions replace them:

| Helper             | Replaces                         | Does                                                      |
|--------------------|----------------------------------|-----------------------------------------------------------|
| `attribute()`      | `QDomElement::attribute`         | attribute without namespace, `nullopt` if missing         |
| `readAttribute()`  | `if (hasAttribute) x = attribute`| overwrites a default only if the attribute exists         |
| `forEachChild()`   | `childNodes()` loop              | direct child elements, skips text and comments            |
| `descendants()`    | `elementsByTagName`              | all elements with this name at any depth                  |
| `toInt/toDouble()` | `QString::toInt/toFloat`         | leading number, `"1920px"` → 1920, 0 if none              |
| `toNumber()`       | `QString::toFloat`               | the whole value as number, otherwise 0 (time values)      |
| `relative()`       | `calculatePercentBasedOnRoot`    | `"50%"`, `"960px"`, `"960"` → 0..1                        |
| `parseClockValue()`| `ClockValue`                     | SMIL time value → milliseconds                            |
| `parseTimeCondition()` | `EnhancedTimer::parseOneValue` | one `begin`/`end` value → `TimeCondition`             |
| `parseWallclock()` | `WallClock`, `IsoDate`           | `wallclock(…)` → `Wallclock`                              |
| `mediaKind()`      | `ElementFactory`                 | which medium an element is                                |

### Example

```xml
<smil>
  <head>
    <layout><root-layout width="1920" height="1080"/>
            <region xml:id="left" width="50%"/></layout>
    <state><data><logo>http://hub/logo.png</logo></data></state>
  </head>
  <body>
    <img src="{logo}" region="left" dur="10s"><param name="logContentId" value="42"/></img>
    <video src="intro.mp4" region="left" repeatCount="2"/>
  </body>
</smil>
```

becomes

```
Document
├── head
│   ├── root_layout   1920 × 1080
│   ├── regions       [ left: top 0, left 0, width 0.5, height 1 ]
│   └── placeholders  { "{logo}": "http://hub/logo.png" }
└── body              Container Body, begin 0s
    ├── Media Image   src http://hub/logo.png, region left, dur Clock 10 s, log_content_id 42
    └── Media Video   src intro.mp4, region left, dur None, repeat_count 2
```

## Head

| SMIL                                   | `Head`          | Default                                    |
|----------------------------------------|-----------------|--------------------------------------------|
| `<meta name="title" content>`          | `title`         | `No Title`                                 |
| `<meta http-equiv="Refresh" content>`  | `refresh`       | 0 s = never look for a new index           |
| `<layout><root-layout>`                | `root_layout`   | 1366 × 768, background `black`             |
| `<layout><region>`                     | `regions`       | one region `screen` covering the screen    |
| `<metadata> … <subscription>`          | `subscriptions` | none                                       |
| `<state><data><name>path</name>`       | `placeholders`  | none; key is `{name}` like in `src="{name}"` |

- `refresh` is the raw value. The minimum of 60 s (garlic-lib: release builds only) is set by the index manager.
- `<root-layout>` is only read if there is exactly one.
- Region positions and sizes are 0..1 relative to the root layout: `50%` → 0.5, `960` or `960px` at a width of
  1920 → 0.5. So the frontend can scale them to any screen.
- Regions are sorted by `z-index` ascending, regions with the same `z-index` keep the order of the SMIL.
- Ids of regions, media and containers: `xml:id`, then `id`, otherwise `<tag>_<line>_<number>`. They are only used
  internally; the `contentId` of the play log comes from `<param name="logContentId">`.
- `<transition>` is ignored like in garlic-lib.

## Body

The body is a tree: `Document::body` is a `Container`, its `children` are `Node`s, and a `Node` is either a `Media`
or a `Container` (`std::variant`). There is no class per tag; the kind is an enum, the engine handles the
differences with a `switch`.

```cpp
for (const Node &node : document->body.children)
    if (const auto *media = std::get_if<Media>(&node))
        ...
```

### seq and par

| SMIL                                        | `Container`                 | Default                   |
|---------------------------------------------|-----------------------------|---------------------------|
| `<seq>`, `<par>`                            | `kind` `Seq`, `Par`         |                           |
| `id`, `xml:id`, `title`, `expr`, timing     | like media                  |                           |
| `<seq><metadata>` as first child            | `pick_number`               | none = all children in order |
| `<meta name="adapi:pickNumber" content>`    | `pick_number`               | 1 if missing, below 1 → 1 |
| `<par endsync>`                             | `endsync`, `endsync_id`     | `Last`                    |

- The surrounding container decides the defaults of its children: in a `seq` only the first value of a `begin`/`end`
  list counts, in a `par` the whole list.
- `pick_number` means: per run of the `seq` play this many random children (garlic-lib: `TShuffle`). The shuffling
  is done by the engine. `adapi:pickingAlgorithm` and `adapi:pickingBehavior` are not read; garlic-lib read them but
  always used "shuffle" and "pickN".
- `<metadata>` is never a child. Only as first child of a `seq` it switches on the random pick, even when empty.
- `endsync="first"` → `First`, `"last"` or missing → `Last`, any other value → `Element` with that id. If no child
  has the id, the `par` behaves like `Last` (garlic-lib compared the id and never matched).
- The body is played like a `seq`, but its children may have `begin` lists like in garlic-lib (`TBody` was not
  treated as `TSeq` when parsing).

### excl and priorityClass

The children of an `excl` are always `PriorityClass` containers, so the engine knows only one case. If the SMIL has
no `<priorityClass>`, all children of the `excl` form one with the defaults and a generated id
(`priorityClass_<line of the excl>_<number>`).

```
<excl>                                  excl
  <priorityClass peers="pause">         ├── priorityClass  peers Pause
    <img xml:id="a"/>                   │   └── a
  </priorityClass>                      └── priorityClass  defaults
  <priorityClass>                           └── b
    <img xml:id="b"/>
  </priorityClass>
</excl>
```

| Attribute | `Container` | Values        | Default | When the new element comes from … |
|-----------|-------------|---------------|---------|-----------------------------------|
| `peers`   | `peers`     | stop, pause, defer, never | stop  | the same priorityClass   |
| `higher`  | `higher`    | stop, pause   | pause   | a higher priorityClass            |
| `lower`   | `lower`     | defer, never  | defer   | a lower priorityClass             |

`Interrupt` says what happens with the playing element: `Stop` stops it, `Pause` pauses and resumes it afterwards,
`Defer` lets it finish and starts the new one afterwards, `Never` ignores the new one.

- The order of the SMIL is the priority: the first `priorityClass` is the highest.
- Invalid values become the default. A `priorityClass` has no timing, only `id` and `title`.
- Children of a `priorityClass` start by trigger: without `begin` it is `indefinite`. `begin` lists are allowed.
- `excl` has timing and `endsync` like `par`.
- Like garlic-lib: direct children next to `<priorityClass>` elements are ignored (no mixing), a `priorityClass`
  inside a `priorityClass` or outside an `excl` is ignored. Nested `excl` work recursively.
- Not read, like in garlic-lib: `pauseDisplay` (its validation was never called), `peer` and `weight` (only in some
  old test files).

### Media

| SMIL                                                       | `MediaKind` |
|------------------------------------------------------------|-------------|
| `<img>`, `<ref type="image/…">`                            | `Image`     |
| `<video>`, `<ref type="video/…">`                          | `Video`     |
| `<audio>`, `<ref type="audio/…">`                          | `Audio`     |
| `<text>`, `<ref type="text/…">`                            | `Web`       |
| `<ref type="application/widget">`                          | `Widget`    |
| `<ref src="adapi:…">` without `type`                       | `Command`   |
| `<brush color>`                                            | `Brush`     |
| `<prefetch>`                                               | `Prefetch`  |

Other elements (also `<ref>` with an unknown `type`) are not part of the tree, so they take no time. garlic-lib
created a `TUnknown` that was skipped like a missing medium (100 ms in a `seq`, an ending child in a `par`).

| Attribute / param                   | `Media`          | Note                                           |
|-------------------------------------|------------------|------------------------------------------------|
| `src`                               | `src`            | `{name}` replaced by the placeholder, unknown → empty; not for brush |
| `region`                            | `region`         | name or id of a region                         |
| `fit`, `mediaAlign`                 | `fit`, `media_align` | empty = default of the region              |
| `soundLevel`                        | `sound_level`    | audio and video only                           |
| `color`                             | `color`          | brush only                                     |
| `title`, `expr`                     | `title`, `expr`  | `expr` is evaluated with the triggers          |
| `<param name="logContentId">`       | `log_content_id` | `contentId` in the play log                    |
| `<param name="filename">`           | `filename`       |                                                |
| `<param name="cacheControl">`       | `cache_control`  | `onlyIfCached` (any case) → `OnlyIfCached`     |
| other `<param>` of a widget         | `query`          | `?name=value&…`, values percent-encoded like `QUrl::toPercentEncoding` |

### Timing

| Attribute     | `Timing`              | Default / values                                                   |
|---------------|-----------------------|--------------------------------------------------------------------|
| `begin`       | `begin`               | list separated by `;`; missing or unreadable: `0s`, in `excl`: `indefinite` |
| `end`         | `end`                 | list, empty = no end                                               |
| `dur`         | `dur`                 | `None` (missing), `Clock`, `Indefinite` (`indefinite` or empty), `Media` |
| `repeatCount` | `repeat_count`, `repeat_indefinite` | 1; `indefinite`; `2.5` → 2; below 1 → 1              |
| `restart`     | `restart`             | `Always` if missing; `always`, `never`, anything else `WhenNotActive` |

- Time values: `01:30:00.5`, `1:30`, `2h`, `1.5min`, `90s`, `500ms`, `90` (seconds). Offsets in `begin`/`end`
  may have a sign (`-2s`). Negative durations become 0. An unknown unit (`30d`) is 0 like in garlic-lib.
- In a `seq` only the first value of a list counts. In a list with several values `indefinite` is ignored.
- `None` and `Indefinite` are different: garlic-lib skipped an image without `dur` and `end`, but showed an image
  with `dur="indefinite"` forever.

### begin and end values

Each value of a `begin`/`end` list becomes one `TimeCondition` (`std::variant`). The type is found in the same order
as in garlic-lib; unreadable values are skipped.

| Value                                         | Type         | Fields                                  |
|-----------------------------------------------|--------------|-----------------------------------------|
| contains `indefinite`                         | `Indefinite` | only alone, in a list it is ignored     |
| `wallclock(…)`                                | `Wallclock`  | not in a `seq` (like garlic-lib)        |
| starts with `+`, `-` or a digit: `5s`, `-2s`  | `Offset`     | `value`                                 |
| `accesskey(a)+2s`                             | `AccessKey`  | `key` (a-z, 0-9, lower case), `offset`  |
| `notify(door_open)`                           | `Notify`     | `name`, `offset`                        |
| `img1.begin`, `img1.end-1s`                   | `SyncBase`   | `id`, `edge`, `offset`                  |
| `img1.beginEvent`, `.endEvent`, `.activateEvent`, `.click` | `Event` | `id`, `type`, `offset`         |

- The offset (`+2s`, `-500ms`) is looked for only after the symbol or after `)`, so ids and names may contain `-`
  and `.`: `my-img.v2.end` has the id `my-img.v2`.
- Who listens to whom is not part of the tree; the engine finds it from the ids.

### Wallclock

`wallclock([R[n]/]date[/period])`

| Part      | Format                                   | `Wallclock`                                            |
|-----------|------------------------------------------|--------------------------------------------------------|
| repeats   | `R` endless, `R5` five times, missing    | `repeats` -1, 5, 0                                     |
| date      | `yyyy-MM-dd`, optional `+wN`/`-wN`       | `start`: the next (`+`, also without sign) or previous (`-`) weekday N after/before the date; 1 = Monday … 7 = Sunday; the same weekday counts as a week later/earlier |
| time      | `THH`, `THH:mm`, `THH:mm:ss`, missing = 00:00 | added to `start`                                  |
| time zone | none = local time, `Z`, `+hh:mm`, `-hh:mm` | converted to local time                             |
| period    | `P1Y2M3W4DT5H6M7S`                       | `period`; weeks become days                            |

- `start` is a `std::chrono::local_seconds`: wall time without zone, no tz database needed (ESP32).
- Without a period there is no repeat (`repeats` 0, GitHub issue #31).
- An invalid date becomes 2000-01-01, an invalid time 00:00 like in garlic-lib.
- When the wallclock fires next is calculated at runtime, not by the parser.

## Differences to garlic-lib

- Empty position values (`top=""`) keep the default; garlic-lib crashed (index −1).
- Numbers with a unit are read (`width="1920px"` → 1920); garlic-lib read 0.
- Generated ids have a running number instead of the column, libxml2 only knows the line.
- `std::stable_sort` for the regions; garlic-lib could swap regions with the same `z-index`.
- Subscription values are trimmed, so line breaks around the URL in `<action>` do no harm.
- The parser does not wait for background images. garlic-lib never reset this flag, so after one index with a
  background image every further index waited up to 10 s.
- `dur="500ms"` is 500 ms; garlic-lib read 0, because it checked the unit `s` before `ms`.
- Durations are `std::chrono::milliseconds`; garlic-lib used an `int` that overflowed at about 24 days.
- `repeatCount="2.5"` is 2; garlic-lib read 0 (once).
- `adapi:pickNumber` below 1 or unreadable is 1; garlic-lib picked nothing and the `seq` ended at once.
- A begin list with one readable value is valid (`5s;garbage`); garlic-lib only looked at the last value.
- Trigger offsets are split after the symbol; garlic-lib split at the first `+`/`-`, so `my-img.begin` and
  `notify(door-open)` broke.
- `wallclock(date/period)` without `R` works; garlic-lib accessed a missing third part and crashed.
- Wallclock times with a time zone (`Z`, `±hh:mm`) are converted; garlic-lib read them as 00:00.
- An empty `src=""` is allowed; garlic-lib crashed on the placeholder check.
- Not taken over: `zoom_factor`/`extra_js` (never set, dead code), `class`, `xml:lang`, `longdesc`, `alt` (never
  used).

## Runtime functions

The parser only reads. Two questions depend on the current time and are answered at runtime by pure functions: the
time is a parameter, there is no state, so they are tested without waiting.

### wallclockTriggers

```cpp
WallclockTriggers t = wallclockTriggers(wallclock, common::toLocalTime(clock.now()));
t.previous   // last time <= now, if there was one
t.next       // next time > now, if there is one left
```

garlic-lib: `WallClock::calculateCurrentTrigger`.

- Times: `start`, `start` + 1 period … `start` + `repeats` periods. `R5` means the start and 5 repeats (6 times);
  garlic-lib calculated one more. `repeats` -1 is endless, 0 only the start.
- Everything in local time, so a daily 08:00 stays 08:00 after a daylight saving change. The engine turns
  `next - toLocalTime(clock.now())` into a waiting time.
- `previous` is needed for elements that should already be playing: started at 08:00 with `dur="1h"`, the player
  starts at 08:20, so the element plays at once for the remaining 40 minutes. `previous == now` means "fire now".
- Months and years like Qt: the day is clamped to the end of the month (31.01. + `P1M` = 28.02.). Every time is
  calculated from `start`, so the clamping does not add up (the next one is 31.03. again).
- Short periods over a long time (`PT15S` since 2021) are calculated directly instead of adding period by period.

### evaluateExpr

```cpp
if (evaluateExpr(media.expr, ExprContext{clock.now(), config.uuid(), config.playerName()}))
    ... play
```

garlic-lib: `Expr` and `AdapiWrapper`. The functions are registered as real XPath functions in libxml2 (XPath 1.0),
not replaced as text:

| Function                               | Result                                     |
|----------------------------------------|--------------------------------------------|
| `adapi-date()`                         | local time `2026-10-28T08:00:00`           |
| `adapi-gmdate()`                       | UTC `2026-10-28T07:00:00Z`                 |
| `adapi-weekday()`, `adapi-gmweekday()` | 0 = Sunday … 6 = Saturday, local / UTC     |
| `compare(a, b)`, `adapi-compare(a, b)` | -1, 0, 1 like XPath 2.0 `fn:compare`       |
| `smil-playerId()`, `smil-playerName()` | from the `ExprContext`                     |

- An empty `expr` is true. Errors, unknown functions and results that are not boolean `true` (e.g. `"1"`) are false,
  like in garlic-lib. Errors are not printed.
- garlic-lib used `QXmlQuery` (XPath 2.0), but no other 2.0 function was used than `compare`. If one is needed
  later, it is registered by hand like `compare`.
- Fixed: `adapi-gmweekday()` returned the local weekday (garlic-lib replaced `adapi-weekday()` instead), and
  `smil-playerId()`/`smil-playerName()` were replaced by an empty string, so expressions with them were always false.

## Engine

The engine plays a `Document`. It decides **what** plays when and where; the player decides **how** (widget,
decoder, WebView). Events go in, commands come out. No timers, no threads: the host calls `poll()`.

```cpp
Engine engine(document, clock, media_source, logger, {uuid, player_name});
engine.start();
while (running)
{
    for (const Command &command : engine.poll())
        std::visit(player, command);               // StartMedia, StopMedia, RepeatMedia, PauseMedia, ResumeMedia
    sleepUntil(engine.nextDeadline());             // or until the player reports something
}
// from the player: engine.mediaFinished(id), engine.mediaFailed(id)
```

The `Document` must live as long as the engine; `StartMedia::media` points into it. For a new index the index
manager parses it first and swaps document and engine only if it is valid; a broken index leaves the old one playing.

### Player interface

| Engine → player | Meaning                                                                      |
|-----------------|------------------------------------------------------------------------------|
| `StartMedia`    | show instance `id`: `media` (kind, fit, soundLevel …), `source`, `regions`   |
| `StopMedia`     | remove instance `id`                                                         |
| `RepeatMedia`   | `repeatCount`: play instance `id` again from the beginning (video, audio); an image just stays |
| `PauseMedia`, `ResumeMedia` | for `excl` (follows)                                              |

| Player → engine        | When                                                                  |
|------------------------|-----------------------------------------------------------------------|
| `mediaFinished(id)`    | a video or audio ended by itself                                      |
| `mediaFailed(id)`      | a medium could not be played                                          |

- `InstanceId` is a running number per start, the basis for transitions later (two instances in one region).
- `source` is ready to use: local path from the cache, the URL of uncachable media, widgets with their query. A brush
  has no source.
- `regions` are indexes into `head.regions`: all regions whose `regionName` (or `id` without `regionName`) is the
  `region` of the medium, like the old player. Unknown region: the first one (lowest `z-index`); the old player
  took the alphabetically first id.
- The play log is written by the engine: one entry per instance with `logContentId`, from start to stop. Failed
  media get none.

### MediaSource

The engine asks the cache through this interface (garlic-lib: `MediaManager`), without knowing the downloader:
`resolve(media)` gives the local path or URL, `nullopt` skips the element; `prefetch(media)` only downloads.
`cacheControl` is the business of the implementation.

### How it works

```
Document (parser, unchanged)       Runtime per node (engine)
body ──────────────────────────►   Active, run started at 0 s, next child 2
├── img a ─────────────────────►   Ended
└── video v ───────────────────►   Active, instance #2
Schedule: [ 10 s: v SimpleEnd ] …
```

- Every node has a small `Runtime`: state (Idle, Active, Ended), remaining repeats, the instance of a medium, the
  next child of a container. The tree stays unchanged.
- One schedule (priority queue: time → node → event) replaces all QTimers of garlic-lib. `poll()` works off what is
  due and collects the commands. Events of a node that has ended since are ignored (generation counter).
- Calls of the player (`mediaFinished` …) only change the state; their commands come with the next `poll()`.

### Rules for media (garlic-lib: `prepareDurationTimers`, `finishIntrinsicDuration`, `finishedSimpleDuration`)

- Image, text, widget, brush, command: need `dur` with a time or `indefinite`; without (`dur` missing, `0s`,
  `media`) they are skipped.
- Video and audio: without `dur` until the player reports the end; with `dur` the `dur` decides, an earlier end is
  ignored (and a later one too).
- `mediaFailed` is like an early end: with `dur` the time still runs ("4s nothing" in `body/simple.smil`), without
  `dur` the next element follows at once.
- Media the `MediaSource` does not have are skipped; `prefetch` is downloaded and skipped.
- Skipped like in garlic-lib (`skipElement`): nothing is shown, the element ends after `skip_delay` (100 ms) like a
  normal one. So skipped images show as a short black gap ("black" in `seq/2_without_dur.smil`), they count as ended
  (e.g. for `endsync="first"`), and a repeating container of skipped elements tries again every 100 ms instead of
  looping without time; it starts by itself when the downloads are done.
- `repeatCount` plays the instance again (`RepeatMedia`), the play log has one entry for all repeats.

### Rules for seq and par (garlic-lib: `TSeq`, `TPar`)

| Container `dur` | End of a run                                                                        |
|-----------------|-------------------------------------------------------------------------------------|
| missing         | when the children are through: `seq` after the last, `par` by `endsync`             |
| `10s`           | after exactly 10 s: earlier children leave it empty, longer ones are cut            |
| `indefinite`    | never by itself                                                                     |

Then `repeatCount` decides: a new run or the end.

- Body and `seq` play their children one after another, `par` starts all at once.
- `par` with `endsync`: `last` when all children have ended, `first` when the first ends (the others are stopped),
  `<id>` when this child ends. Ignored if the `par` has a `dur`.
- A skipped child ends after 100 ms like any other, so it can trigger `endsync="first"` (like garlic-lib).
- Skipped: `dur="0s"`, `dur="media"`, empty without `dur`. An empty container with `dur` is a pause of that length.
- A container cut by its `dur` stops everything in it before the next run starts clean. garlic-lib started the next
  run while the old children kept playing.
- `pickNumber` N: the children are shuffled once per engine (garlic-lib: once when parsing, the engine is new for every
  index). Every run takes the next N of this fixed order; at the end it starts again from the front in the same
  order. This is deliberate: every round has the same order. The random generator (`std::mt19937`) gets
  `EngineConfig::random_seed`; the host passes a random value, tests a fixed one. The shuffle is an own
  Fisher-Yates, because `std::shuffle` gives a different order on every standard library.

### Lifecycle of an element: begin, end, restart, expr (garlic-lib: `BaseTimings`, `BeginTimer`, `EndTimer`)

```
activated ──► Waiting ──(begin time)──► Active ──(dur / end of medium / skipped)──► Waiting ──► … ──► Ended
                 ▲                                                                     │
                 └──────────────── begin times still to come (list) ◄─────────────────┘
```

- **Activation**: `begin` and `end` count from the moment the parent activates the element: a `par` (and the body)
  at the start of its run, a `seq` when the previous child has ended.
- **begin**: `0s` starts now, `5s` in 5 s. `-2s` means "began 2 s ago": the element starts now for the rest of its
  `dur`, if there is a rest (`dur` longer than 2 s, `indefinite` or `media`); without `dur` it does not start (like
  garlic-lib). `indefinite` waits for a trigger.
- **Intervals**: every begin starts an interval, a medium gets a new instance per interval (new `StartMedia`, own play
  log entry). `repeatCount` repeats within the interval (`RepeatMedia`, same instance), as in the SMIL standard.
  `repeatCount` is ignored while begin times are still to come (garlic-lib: `!isBeginTimerActive()`).
- **Waiting**: after an interval an element with begin times to come waits for them. A `par` with `endsync="last"`
  waits for such children too; `endsync="first"` and `"<id>"` react to the end of every interval. A `seq` goes on
  only when its child has ended.
- **end**: counts from the activation like `begin`, also while the element still waits. It cuts the interval, but
  does not extend it: if the `dur` is over earlier, the element ends then. When the end time comes, the element ends
  for this activation; later begin times are void. `end` before `begin` and `end="0s"` mean it is never shown.
- **restart** when a begin time comes:

  | `restart`               | element plays                    | element does not play                         |
  |-------------------------|----------------------------------|-----------------------------------------------|
  | `always` (default)      | ends the interval, starts a new  | starts                                        |
  | `whenNotActive`         | ignores the begin                | starts                                        |
  | `never`                 | ignores the begin                | only the first start of the activation        |

- **expr** is evaluated at every start (`evaluateExpr` with `ExprContext` from the clock and `EngineConfig`); false
  skips the element (100 ms). With later begin times it is checked again then.
- **Same time**: an interval that ends at the moment of the next begin ends first (`par/8`: `whenNotActive` with
  begin 6 s and 11 s and a run of 5 s plays three times).
- An element that can never start (only begin times in the past without rest, no `end`, no trigger) is skipped.
  garlic-lib left it waiting forever, so a `seq` behind it stopped.

