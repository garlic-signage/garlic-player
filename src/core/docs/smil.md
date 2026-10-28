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
- Region ids: `xml:id`, then `id`, otherwise `<tag>_<line>_<number>`. They are only used internally; the
  `contentId` of the play log comes from `<param name="logContentId">`.
- `<transition>` is ignored like in garlic-lib.

## Differences to garlic-lib

- Empty position values (`top=""`) keep the default; garlic-lib crashed (index −1).
- Numbers with a unit are read (`width="1920px"` → 1920); garlic-lib read 0.
- Generated ids have a running number instead of the column, libxml2 only knows the line.
- `std::stable_sort` for the regions; garlic-lib could swap regions with the same `z-index`.
- Subscription values are trimmed, so line breaks around the URL in `<action>` do no harm.
- The parser does not wait for background images. garlic-lib never reset this flag, so after one index with a
  background image every further index waited up to 10 s.
