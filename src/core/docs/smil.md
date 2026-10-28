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

Status: head, media, `seq`, `par`, `excl` and `priorityClass` are done. Wallclock and triggers follow; until then
such `begin`/`end` values are skipped.

## How the parser works

### Files

| File           | Content                                                                 |
|----------------|-------------------------------------------------------------------------|
| `document.hpp` | the result: plain structs with the defaults of garlic-lib at the fields |
| `parser.hpp`   | the function `parse()` and `ParseError`                                 |
| `parser.cpp`   | everything else, the only file that knows libxml2                       |

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

Other elements (also `<ref>` with an unknown `type`) are not part of the tree; garlic-lib created a `TUnknown` that
did nothing.

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
- For now `begin`/`end` know only offsets and `indefinite`. Wallclock, syncbase (`id.begin`), events, accesskey and
  notify follow with the triggers; until then such values are skipped.

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
- An empty `src=""` is allowed; garlic-lib crashed on the placeholder check.
- Not taken over: `zoom_factor`/`extra_js` (never set, dead code), `class`, `xml:lang`, `longdesc`, `alt` (never
  used).
