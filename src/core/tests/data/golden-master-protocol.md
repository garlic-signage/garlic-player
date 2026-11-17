# Golden master test protocol

Manual check of the golden master scenarios: every `smil/<path>.smil` is played in the player and compared with
`golden/<path>.expected`. A scenario is validated when the playback on screen matches the expected timeline.

Checked in player-ui: legacy qwidget 1.0 , qwidget 2.0, QML 2.0, and SDL

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
| `3_overlapping`              | bug    | qwidget: video is not overlapped by image 0003 (z-index 5 over 2); QML and SDL correct |
| `4_with_dur`                 | ok     | stays in sync; the old player drifted apart |
| `5_wallclock`                | ok   |       |
| `6_endsync`                  | ok   |       |
| `7_multi_begin_endsync`      | ok   |       |
| `8_multi_begin_restart`      | ok     |  |
| `bugs/1_hammering`           | ok   |       |
| `bugs/2_pseudo_sync`         | ok   |       |
| `trigger/1_syncbase`         | ok   |       |
| `trigger/2_events`           | ok   |       |
| `trigger/3_changes`          | ok   |       |

## excl

| Scenario                          | Status | Notes |
|-----------------------------------|--------|-------|
| `bugs/1_nested_excl`              | ok   |       |
| `bugs/2_parallel_trigger_past`    | ok   |       |
| `bugs/3_hadi`                     | ok   |       |
| `bugs/4_parallel_trigger_media`   | ok   |       |
| `complex/1_customer`              | 20   |       |
| `complex/2_signapp`               | ok   |       |
| `complex/3_smilcontrol`           | ok   |       |
| `higher/1_stop`                   | ok   |       |
| `higher/2_pause`                  | ok   |       |
| `lower/1_defer`                   | ok   |       |
| `lower/2_never`                   | ok   |       |
| `peers/1_stop`                    | ok   |       |
| `peers/2_pause`                   | ok   |       |
| `peers/3_defer`                   | ok   |       |
| `peers/4_never`                   | ok   |       |
| `trigger/1_indefinite`            | ok   |       |
| `trigger/2_indefinite_with_clock` | ok   |       |
| `trigger/3_activate_event`        | ok   |       |
| `trigger/4_accesskey`             | ok   |       |
| `trigger/5_priority_ping_pong`    | ok   |       |
| `trigger/6_priority_realworld`    | ok   |       |
| `trigger/7_notify`                | ok   |       |

## body

| Scenario   | Status | Notes |
|------------|--------|-------|
| `align`    | ok   |       |
| `simple`   | ok   |       |
| `video_align`   | ok (SDL)  | QWidget and QML not support      |

## head

| Scenario        | Status | Notes |
|-----------------|--------|-------|
| `head_complex`  | ok   |       |
| `head_simple`   | ok   |       |

## issues

| Scenario | Status | Notes |
|----------|--------|-------|
| `7`      | ok   |       |
| `9`      | ok   |       |
| `12`     | ok   |       |
| `14`     | bug   | qwidget video hide the overay, QML SDL are korrekt      |
| `31`     | ok   |       |
| `33`     | ok   |       |

## Todo

- [ ] Take care of the 100 ms delay with `dur="media"`.
- [ ] _(second point, still to be added)_
- [ ] Next time build Qt completely with Qt WebView: the desktop kit 6.8.5 lacks
  `plugins/webview/libqtwebview_webengine.so`, so the QML player shows no web pages.
