# Golden master test protocol

Manual check of the golden master scenarios: every `smil/<path>.smil` is played in the player and compared with
`golden/<path>.expected`. A scenario is validated when the playback on screen matches the expected timeline.

Status: `ok` validated, `open` not checked yet, `bug` deviation found (see notes).

## seq

| Scenario               | Status | Notes |
|------------------------|--------|-------|
| `1_simple`             | ok     |       |
| `2_without_dur`        | ok     |       |
| `3_indefinite`         | ok     |       |
| `4_pick_number`        | ok     |       |
| `5_media_repeats`      | ok     |       |
| `6_conditional_play`   | ok     |       |

## par

| Scenario                     | Status | Notes |
|------------------------------|--------|-------|
| `1_simple`                   | ok     |       |
| `2_backgrounds`              | ok     |       |
| `3_overlapping`              | bug    | qwidget: video is not overlapped by image 0003 (z-index 5 over 2); QML correct |
| `4_with_dur`                 | ok     | stays in sync; the old player drifted apart |
| `5_wallclock`                | ok   |       |
| `6_endsync`                  | ok   |       |
| `7_multi_begin_endsync`      | ok   |       |
| `8_multi_begin_restart`      | open   |       |
| `bugs/1_hammering`           | open   |       |
| `bugs/2_pseudo_sync`         | open   |       |
| `trigger/1_syncbase`         | open   |       |
| `trigger/2_events`           | open   |       |
| `trigger/3_changes`          | open   |       |

## excl

| Scenario                          | Status | Notes |
|-----------------------------------|--------|-------|
| `bugs/1_nested_excl`              | open   |       |
| `bugs/2_parallel_trigger_past`    | open   |       |
| `bugs/3_hadi`                     | open   |       |
| `bugs/4_parallel_trigger_media`   | open   |       |
| `complex/1_customer`              | open   |       |
| `complex/2_signapp`               | open   |       |
| `complex/3_smilcontrol`           | open   |       |
| `complex/test`                    | open   |       |
| `higher/1_stop`                   | open   |       |
| `higher/2_pause`                  | open   |       |
| `lower/1_defer`                   | open   |       |
| `lower/2_never`                   | open   |       |
| `peers/1_stop`                    | open   |       |
| `peers/2_pause`                   | open   |       |
| `peers/3_defer`                   | open   |       |
| `peers/4_never`                   | open   |       |
| `trigger/1_indefinite`            | open   |       |
| `trigger/2_indefinite_with_clock` | open   |       |
| `trigger/3_activate_event`        | open   |       |
| `trigger/4_accesskey`             | open   |       |
| `trigger/5_priority_ping_pong`    | open   |       |
| `trigger/6_priority_realworld`    | open   |       |
| `trigger/7_notify`                | open   |       |

## body

| Scenario   | Status | Notes |
|------------|--------|-------|
| `align`    | open   |       |
| `simple`   | open   |       |

## head

| Scenario        | Status | Notes |
|-----------------|--------|-------|
| `head_complex`  | open   |       |
| `head_simple`   | open   |       |

## issues

| Scenario | Status | Notes |
|----------|--------|-------|
| `7`      | open   |       |
| `9`      | open   |       |
| `12`     | open   |       |
| `14`     | open   |       |
| `31`     | open   |       |
| `33`     | open   |       |

## other

| Scenario | Status | Notes |
|----------|--------|-------|
| `test`   | open   |       |

## Todo

- [ ] Take care of the 100 ms delay with `dur="media"`.
- [ ] _(second point, still to be added)_
- [ ] Next time build Qt completely with Qt WebView: the desktop kit 6.8.5 lacks
  `plugins/webview/libqtwebview_webengine.so`, so the QML player shows no web pages.
